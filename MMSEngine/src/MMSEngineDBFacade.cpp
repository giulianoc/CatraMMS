/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngineDBFacade.cpp
 * Author: giuliano
 * 
 * Created on January 27, 2018, 9:38 AM
 */

#include "catralibraries/Encrypt.h"

#define ENABLE_DBLOGGER 0
#if ENABLE_DBLOGGER == 1
    #include "spdlog/spdlog.h"
    static shared_ptr<spdlog::logger> _globalLogger = nullptr;
    #define DB_BORROW_DEBUG_LOGGER(x) if (_globalLogger != nullptr) _globalLogger->info(x);
    #define DB_BORROW_ERROR_LOGGER(x) if (_globalLogger != nullptr) _globalLogger->info(x);
    // #include <iostream>
    // #define DB_DEBUG_LOGGER(x) std::cout << x << std::endl;
    // #define DB_ERROR_LOGGER(x) std::cerr << x << std::endl;
#endif

#include "MMSEngineDBFacade.h"
#include <fstream>
#include <sstream>


// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{
    _logger     = logger;
    #if ENABLE_DBLOGGER == 1
        _globalLogger = logger;
    #endif

    _defaultContentProviderName     = "default";
    // _defaultTerritoryName           = "default";

    size_t dbPoolSize = configuration["database"].get("poolSize", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->poolSize: " + to_string(dbPoolSize)
    );
    string dbServer = configuration["database"].get("server", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->server: " + dbServer
    );
    _dbConnectionPoolStatsReportPeriodInSeconds = configuration["database"].get("dbConnectionPoolStatsReportPeriodInSeconds", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->dbConnectionPoolStatsReportPeriodInSeconds: " + to_string(_dbConnectionPoolStatsReportPeriodInSeconds)
    );

    string dbUsername = configuration["database"].get("userName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->userName: " + dbUsername
    );
    string dbPassword;
    {
        string encryptedPassword = configuration["database"].get("password", "XXX").asString();
        dbPassword = Encrypt::decrypt(encryptedPassword);        
    }    
    string dbName = configuration["database"].get("dbName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->dbName: " + dbName
    );
    string selectTestingConnection = configuration["database"].get("selectTestingConnection", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->selectTestingConnection: " + selectTestingConnection
    );

    _maxEncodingFailures            = configuration["encoding"].get("maxEncodingFailures", 3).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->maxEncodingFailures: " + to_string(_maxEncodingFailures)
    );

    _confirmationCodeRetentionInDays    = configuration["mms"].get("confirmationCodeRetentionInDays", 3).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
    );

    _contentRetentionInMinutesDefaultValue    = configuration["mms"].get("contentRetentionInMinutesDefaultValue", 1).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentRetentionInMinutesDefaultValue: " + to_string(_contentRetentionInMinutesDefaultValue)
    );
    _contentNotTransferredRetentionInDays    = configuration["mms"].get("contentNotTransferredRetentionInDays", 1).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentNotTransferredRetentionInDays: " + to_string(_contentNotTransferredRetentionInDays)
    );
    
    _predefinedVideoProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("videoDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->videoDir: " + _predefinedVideoProfilesDirectoryPath
    );
    _predefinedAudioProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("audioDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->audioDir: " + _predefinedAudioProfilesDirectoryPath
    );
    _predefinedImageProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("imageDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->imageDir: " + _predefinedImageProfilesDirectoryPath
    );

    _logger->info(__FILEREF__ + "Creating MySQLConnectionFactory...");
	bool reconnect = true;
	string defaultCharacterSet = "utf8";
    _mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName,
            reconnect, defaultCharacterSet, selectTestingConnection);

    // 2018-04-05: without an open stream the first connection fails
    // 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
    //      Stdout of the spdlog is now false and I commented the ofstream statement
    // ofstream aaa("/tmp/a.txt");
    _logger->info(__FILEREF__ + "Creating DBConnectionPool...");
    _connectionPool = make_shared<DBConnectionPool<MySQLConnection>>(
            dbPoolSize, _mySQLConnectionFactory);
     
    _lastConnectionStatsReport = chrono::system_clock::now();

    _logger->info(__FILEREF__ + "createTablesIfNeeded...");
    createTablesIfNeeded();
}

MMSEngineDBFacade::~MMSEngineDBFacade() 
{
}

/*
void MMSEngineDBFacade::getTerritories(shared_ptr<Workspace> workspace)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	
    _logger->debug(__FILEREF__ + "DB connection borrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );

    string lastSQLCommand =
        "select territoryKey, name from MMS_Territory t where workspaceKey = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        lastSQLCommand));
    preparedStatement->setInt(1, workspace->_workspaceKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    while (resultSet->next())
    {
        workspace->_territories.insert(make_pair(resultSet->getInt("territoryKey"), resultSet->getString("name")));
    }

    _logger->debug(__FILEREF__ + "DB connection unborrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );
    _connectionPool->unborrow(conn);
	conn = nullptr;
}
*/

/*
int64_t MMSEngineDBFacade::addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string territoryName
)
{
    int64_t         territoryKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into MMS_Territory (territoryKey, workspaceKey, name, currency) values ("
                "NULL, ?, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, territoryName);
            string currency("");
            if (currency == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, currency);

            preparedStatement->executeUpdate();
        }
        
        territoryKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return territoryKey;
}
*/

int64_t MMSEngineDBFacade::addEncodingProfilesSet (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
        MMSEngineDBFacade::ContentType contentType, 
        string label)
{
    int64_t     encodingProfilesSetKey;
    
    string      lastSQLCommand;
        
    try
    {
        {
            lastSQLCommand = 
                "select encodingProfilesSetKey from MMS_EncodingProfilesSet where workspaceKey = ? and contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey     = resultSet->getInt64("encodingProfilesSetKey");
            }
            else
            {
                lastSQLCommand = 
                    "insert into MMS_EncodingProfilesSet (encodingProfilesSetKey, workspaceKey, contentType, label) values ("
                    "NULL, ?, ?, ?)";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                preparedStatement->setString(queryParameterIndex++, label);

                preparedStatement->executeUpdate();

                encodingProfilesSetKey = getLastInsertId(conn);
            }
        }        
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfilesSetKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        EncodingTechnology encodingTechnology,
        string jsonEncodingProfile
)
{    
    int64_t         encodingProfileKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t encodingProfilesSetKey = -1;

        encodingProfileKey = addEncodingProfile(
            conn,
            workspaceKey,
            label,
            contentType, 
            encodingTechnology,
            jsonEncodingProfile,
            encodingProfilesSetKey);        

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    
    return encodingProfileKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        EncodingTechnology encodingTechnology,
        string jsonProfile,
        int64_t encodingProfilesSetKey  // -1 if it is not associated to any Set
)
{
    int64_t         encodingProfileKey;

    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "select encodingProfileKey from MMS_EncodingProfile where (workspaceKey = ? or workspaceKey is null) and contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfileKey     = resultSet->getInt64("encodingProfileKey");
                
                lastSQLCommand = 
                    "update MMS_EncodingProfile set technology = ?, jsonProfile = ? where encodingProfileKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
                preparedStatement->setString(queryParameterIndex++, jsonProfile);
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

                preparedStatement->executeUpdate();
            }
            else
            {
                lastSQLCommand = 
                    "insert into MMS_EncodingProfile ("
                    "encodingProfileKey, workspaceKey, label, contentType, technology, jsonProfile) values ("
                    "NULL, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                    preparedStatement->setString(queryParameterIndex++, label);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
                preparedStatement->setString(queryParameterIndex++, jsonProfile);

                preparedStatement->executeUpdate();

                encodingProfileKey = getLastInsertId(conn);
            }
        }
        
              
        if (encodingProfilesSetKey != -1)
        {
            {
                lastSQLCommand = 
                    "select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = ? and encodingProfileKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (!resultSet->next())
                {
                    {
                        lastSQLCommand = 
                            "select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                        if (resultSet->next())
                        {
                            int64_t localWorkspaceKey     = resultSet->getInt64("workspaceKey");
                            if (localWorkspaceKey != workspaceKey)
                            {
                                string errorMessage = __FILEREF__ + "It is not possible to use an EncodingProfilesSet if you are not the owner"
                                        + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
                                        + ", workspaceKey: " + to_string(workspaceKey)
                                        + ", localWorkspaceKey: " + to_string(localWorkspaceKey)
                                        + ", lastSQLCommand: " + lastSQLCommand
                                ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);                    
                            }
                        }
                    }

                    {
                        lastSQLCommand = 
                            "insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey)  values (?, ?)";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
                        preparedStatement->executeUpdate();
                    }
                }
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfile(
    int64_t workspaceKey, int64_t encodingProfileKey)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "delete from MMS_EncodingProfile where encodingProfileKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated == 0)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingProfileKey: " + to_string(encodingProfileKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

int64_t MMSEngineDBFacade::addEncodingProfileIntoSet(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string label,
        MMSEngineDBFacade::ContentType contentType, 
        int64_t encodingProfilesSetKey
)
{
    int64_t         encodingProfileKey;

    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "select encodingProfileKey from MMS_EncodingProfile where (workspaceKey = ? or workspaceKey is null) and contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfileKey     = resultSet->getInt64("encodingProfileKey");                
            }
            else
            {
                string errorMessage = string("Encoding profile label was not found")
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                        + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }       
              
        {
            {
                lastSQLCommand = 
                    "select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = ? and encodingProfileKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (!resultSet->next())
                {
                    {
                        lastSQLCommand = 
                            "select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                        if (resultSet->next())
                        {
                            int64_t localWorkspaceKey     = resultSet->getInt64("workspaceKey");
                            if (localWorkspaceKey != workspaceKey)
                            {
                                string errorMessage = __FILEREF__ + "It is not possible to use an EncodingProfilesSet if you are not the owner"
                                        + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
                                        + ", workspaceKey: " + to_string(workspaceKey)
                                        + ", localWorkspaceKey: " + to_string(localWorkspaceKey)
                                        + ", lastSQLCommand: " + lastSQLCommand
                                ;
                                _logger->error(errorMessage);

                                throw runtime_error(errorMessage);                    
                            }
                        }
                    }

                    {
                        lastSQLCommand = 
                            "insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey)  values (?, ?)";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
                        preparedStatement->executeUpdate();
                    }
                }
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfilesSet(
    int64_t workspaceKey, int64_t encodingProfilesSetKey)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "delete from MMS_EncodingProfilesSet where encodingProfilesSetKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated == 0)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

void MMSEngineDBFacade::getExpiredMediaItemKeys(
        string processorMMS,
        vector<pair<shared_ptr<Workspace>,int64_t>>& mediaItemKeyToBeRemoved,
        int maxMediaItemKeysNumber)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        int start = 0;
        bool noMoreRowsReturned = false;
        while (mediaItemKeyToBeRemoved.size() < maxMediaItemKeysNumber &&
                !noMoreRowsReturned)
        {
            lastSQLCommand = 
                "select workspaceKey, mediaItemKey, ingestionJobKey from MMS_MediaItem where "
                "DATE_ADD(ingestionDate, INTERVAL retentionInMinutes MINUTE) < NOW() "
                "and processorMMSForRetention is null "
                "limit ? offset ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, maxMediaItemKeysNumber);
            preparedStatement->setInt(queryParameterIndex++, start);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            noMoreRowsReturned = true;
            start += maxMediaItemKeysNumber;
            while (resultSet->next())
            {
                noMoreRowsReturned = false;
                
                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
                int64_t workspaceKey = resultSet->getInt64("workspaceKey");
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
                {
                    {
                        lastSQLCommand = 
                            "select count(*) from MMS_IngestionJobDependency ijd, MMS_IngestionJob ij where "
                            "ijd.ingestionJobKey = ij.ingestionJobKey "
                            "and ijd.dependOnIngestionJobKey = ? "
                            "and ij.status not like 'End_%'";
                        shared_ptr<sql::PreparedStatement> preparedStatementDependency (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementDependency->setInt(queryParameterIndex++, ingestionJobKey);

                        shared_ptr<sql::ResultSet> resultSetDependency (preparedStatementDependency->executeQuery());
                        if (resultSetDependency->next())
                        {
                            if (resultSetDependency->getInt64(1) > 0)
                                ingestionDependingOnMediaItemKey = true;
                        }
                        else
                        {
                            string errorMessage ("select count(*) failed");

                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                }
                
                if (!ingestionDependingOnMediaItemKey)
                {
                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    pair<shared_ptr<Workspace>,int64_t> workspaceAndMediaItemKey =
                            make_pair(workspace, mediaItemKey);

                    mediaItemKeyToBeRemoved.push_back(workspaceAndMediaItemKey);

                    {
                        lastSQLCommand = 
                            "update MMS_MediaItem set processorMMSForRetention = ? where mediaItemKey = ? and processorMMSForRetention is null";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                        preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, mediaItemKey);

                        int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
                        if (rowsUpdated != 1)
                        {
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", processorMMS: " + processorMMS
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                }
                else
                {
                    _logger->info(__FILEREF__ + "Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
                        + "ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                    );
                }
            }
        }
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }    
}

void MMSEngineDBFacade::resetProcessingJobsIfNeeded(string processorMMS)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ? where processorMMS = ? and "
                "status in (?, ?, ?) and sourceBinaryTransferred = 0";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_IngestionJob) to be reset because downloading was interrupted"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set processorMMS = NULL where processorMMS = ? and status not like 'End_%'";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_IngestionJob) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }
                        
        {
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();            
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_EncodingJob) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

        {
            lastSQLCommand = 
                "update MMS_MediaItem set processorMMSForRetention = NULL where processorMMSForRetention = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_MediaItem) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

void MMSEngineDBFacade::getIngestionsToBeManaged(
        vector<tuple<int64_t, shared_ptr<Workspace>, string, IngestionType, IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs
)
{
    string      lastSQLCommand;
    bool        autoCommit = true;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        chrono::system_clock::time_point now = chrono::system_clock::now();
        if (now - _lastConnectionStatsReport >= chrono::seconds(_dbConnectionPoolStatsReportPeriodInSeconds))
        {
            _lastConnectionStatsReport = chrono::system_clock::now();
            
            DBConnectionPoolStats dbConnectionPoolStats = _connectionPool->get_stats();	

            _logger->info(__FILEREF__ + "DB connection pool stats"
                + ", _poolSize: " + to_string(dbConnectionPoolStats._poolSize)
                + ", _borrowedSize: " + to_string(dbConnectionPoolStats._borrowedSize)
            );
        }

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // We have the Transaction because previously there was a select for update and then the update.
        // Now we have first the update and than the select. Probable the Transaction does not need, anyway I left it
        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

		// IngestionJobs taking too time to download/move/copy/upload the content are set to failed
		{
			lastSQLCommand = 
				"select ingestionJobKey from MMS_IngestionJob "
				"where status in (?, ?, ?, ?) and sourceBinaryTransferred = 0 "
				"and DATE_ADD(startProcessing, INTERVAL ? DAY) <= NOW()";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
			preparedStatement->setInt(queryParameterIndex++, _contentNotTransferredRetentionInDays);

			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

			while (resultSet->next())
			{
				int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
				{     
           			IngestionStatus newIngestionStatus = IngestionStatus::End_IngestionFailure;

					string errorMessage = "Set to Failure by MMS because of timeout to download/move/copy/upload the content";
					string processorMMS;
					_logger->info(__FILEREF__ + "Update IngestionJob"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + toString(newIngestionStatus)
						+ ", errorMessage: " + errorMessage
						+ ", processorMMS: " + processorMMS
					);                            
    				try
    				{
						updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
					}
    				catch(sql::SQLException se)
    				{
        				string exceptionMessage(se.what());
        
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", exceptionMessage: " + exceptionMessage
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}    
    				catch(runtime_error e)
    				{        
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", e.what(): " + e.what()
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}        
    				catch(exception e)
    				{        
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}        
				}
			}
		}

        // ingested jobs that do not have to wait a dependency
        {
            int mysqlOffset = 0;
            int mysqlRowCount = maxIngestionJobs;
            bool moreRows = true;
            while(ingestionsToBeManaged.size() < maxIngestionJobs && moreRows)
            {
                lastSQLCommand = 
                    "select ij.ingestionJobKey, ir.workspaceKey, ij.metaDataContent, ij.status, ij.ingestionType "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey and ij.processorMMS is null "
                        "and (ij.status = ? or (ij.status in (?, ?, ?, ?) and ij.sourceBinaryTransferred = 1)) "
                        "limit ? offset ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndexIngestionJob = 1;
                preparedStatement->setString(queryParameterIndexIngestionJob++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
                preparedStatement->setString(queryParameterIndexIngestionJob++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
                preparedStatement->setString(queryParameterIndexIngestionJob++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
                preparedStatement->setString(queryParameterIndexIngestionJob++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
                preparedStatement->setString(queryParameterIndexIngestionJob++, MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
                preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlRowCount);
                preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlOffset);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

                if (resultSet->rowsCount() < mysqlRowCount)
                    moreRows = false;
                else
                    moreRows = true;
                mysqlOffset += maxIngestionJobs;
                
                while (resultSet->next())
                {
                    int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
                    int64_t workspaceKey         = resultSet->getInt64("workspaceKey");
                    string metaDataContent      = resultSet->getString("metaDataContent");
                    IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("status"));
                    IngestionType ingestionType     = MMSEngineDBFacade::toIngestionType(resultSet->getString("ingestionType"));

//                    _logger->info(__FILEREF__ + "Analyzing dependencies for the IngestionJob"
//                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
//                    );
                    
                    bool ingestionJobToBeManaged = true;
                    bool atLeastOneDependencyRowFound = false;

                    lastSQLCommand = 
                        "select dependOnIngestionJobKey, dependOnSuccess from MMS_IngestionJobDependency where ingestionJobKey = ? order by orderNumber asc";
                    shared_ptr<sql::PreparedStatement> preparedStatementDependency (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndexDependency = 1;
                    preparedStatementDependency->setInt64(queryParameterIndexDependency++, ingestionJobKey);

                    int64_t dependOnIngestionJobKey = -1;
                    int dependOnSuccess = -1;
                    IngestionStatus ingestionStatusDependency;
                    shared_ptr<sql::ResultSet> resultSetDependency (preparedStatementDependency->executeQuery());
                    while (resultSetDependency->next())
                    {
                        if (!atLeastOneDependencyRowFound)
                            atLeastOneDependencyRowFound = true;

                        if (!resultSetDependency->isNull("dependOnIngestionJobKey"))
                        {
                            dependOnIngestionJobKey     = resultSetDependency->getInt64("dependOnIngestionJobKey");
                            dependOnSuccess                 = resultSetDependency->getInt("dependOnSuccess");

                            lastSQLCommand = 
                                "select status from MMS_IngestionJob where ingestionJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementIngestionJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndexStatus = 1;
                            preparedStatementIngestionJob->setInt64(queryParameterIndexStatus++, dependOnIngestionJobKey);

                            shared_ptr<sql::ResultSet> resultSetIngestionJob (preparedStatementIngestionJob->executeQuery());
                            if (resultSetIngestionJob->next())
                            {
                                string sStatus = resultSetIngestionJob->getString("status");

//                                _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
//                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
//                                    + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
//                                    + ", dependOnSuccess: " + to_string(dependOnSuccess)
//                                    + ", status (dependOnIngestionJobKey): " + sStatus
//                                );

                                ingestionStatusDependency     = MMSEngineDBFacade::toIngestionStatus(sStatus);

                                if (MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatusDependency))
                                {
                                    if (dependOnSuccess == 1 && MMSEngineDBFacade::isIngestionStatusFailed(ingestionStatusDependency))
                                    {
                                        ingestionJobToBeManaged = false;

                                        break;
                                    }
                                    else if (dependOnSuccess == 0 && MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatusDependency))
                                    {
                                        ingestionJobToBeManaged = false;

                                        break;
                                    }
                                    // else if (dependOnSuccess == -1)
                                    //      It means OnComplete and we have to do it since it's a final state
                                }
                                else
                                {
                                    ingestionJobToBeManaged = false;

                                    break;
                                }
                            }
                            else
                            {
                                _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
                                    + ", dependOnSuccess: " + to_string(dependOnSuccess)
                                    + ", status: " + "no row"
                                );
                            }
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
                                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                + ", dependOnIngestionJobKey: " + "null"
                            );
                        }
                    }
                    
                    if (!atLeastOneDependencyRowFound)
                    {
						// this is not possible, even an ingestionJob without dependency has a row
						// (with dependOnIngestionJobKey NULL)

                        _logger->error(__FILEREF__ + "No dependency Row for the IngestionJob"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", workspaceKey: " + to_string(workspaceKey)
                            + ", ingestionStatus: " + to_string(static_cast<int>(ingestionStatus))
                            + ", ingestionType: " + to_string(static_cast<int>(ingestionType))
                        );
                    	ingestionJobToBeManaged = false;
                    }

                    if (ingestionJobToBeManaged)
                    {
                        _logger->info(__FILEREF__ + "Adding jobs to be processed"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ingestionStatus: " + toString(ingestionStatus)
                        );
                        
                        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                        tuple<int64_t, shared_ptr<Workspace>, string, IngestionType, IngestionStatus> ingestionToBeManaged
                                = make_tuple(ingestionJobKey, workspace, metaDataContent, ingestionType, ingestionStatus);

                        ingestionsToBeManaged.push_back(ingestionToBeManaged);
                    }
                    else
                    {
                        _logger->debug(__FILEREF__ + "Ingestion job cannot be processed"
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", ingestionStatus: " + toString(ingestionStatus)
                            + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
                            + ", dependOnSuccess: " + to_string(dependOnSuccess)
                            + ", ingestionStatusDependency: " + toString(ingestionStatusDependency)
                        );
                    }
                }
            }
        }

        for (tuple<int64_t, shared_ptr<Workspace>, string, IngestionType, IngestionStatus>& ingestionToBeManaged:
            ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            shared_ptr<Workspace> workspace;
            string metaDataContent;
            string sourceReference;
            MMSEngineDBFacade::IngestionStatus ingestionStatus;
            MMSEngineDBFacade::IngestionType ingestionType;

            tie(ingestionJobKey, workspace, metaDataContent, ingestionType, ingestionStatus) = ingestionToBeManaged;

            if (ingestionStatus == IngestionStatus::SourceMovingInProgress
                    || ingestionStatus == IngestionStatus::SourceCopingInProgress
                    || ingestionStatus == IngestionStatus::SourceUploadingInProgress)
            {
                // let's consider IngestionStatus::SourceUploadingInProgress
                // In this scenarios, the content was already uploaded by the client (sourceBinaryTransferred = 1),
                // if we set startProcessing = NOW() we would not have any difference with endProcessing
                // So, in this scenarios (SourceUploadingInProgress), startProcessing-endProcessing is the time
                // between the client ingested the Workflow and the content completely uploaded.
                // In this case, if the client has to upload 10 contents sequentially, the last one is the sum
                // of all the other contents

                lastSQLCommand = 
                    "update MMS_IngestionJob set processorMMS = ? where ingestionJobKey = ?";                
            }
            else
            {
                lastSQLCommand = 
                    "update MMS_IngestionJob set startProcessing = NOW(), processorMMS = ? where ingestionJobKey = ?";                
            }
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }        
}

shared_ptr<MySQLConnection> MMSEngineDBFacade::beginIngestionJobs ()
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

    return conn;    
}

shared_ptr<MySQLConnection> MMSEngineDBFacade::endIngestionJobs (
    shared_ptr<MySQLConnection> conn, bool commit)
{    
    string      lastSQLCommand;
    
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "endIngestionJobs"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
            + ", commit: " + to_string(commit)
        );

        if (commit)
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        else
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

    return conn;    
}

int64_t MMSEngineDBFacade::addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, string rootType, string rootLabel,
		string metaDataContent
)
{
    int64_t     ingestionRootKey;
    
    string      lastSQLCommand;
    
    try
    {
        {
            {                
                lastSQLCommand = 
                    "insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, type, label, metaDataContent, ingestionDate, lastUpdate, status) "
					"values (NULL, ?, ?, ?, ?, NOW(), NOW(), ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                preparedStatement->setString(queryParameterIndex++, rootType);
                preparedStatement->setString(queryParameterIndex++, rootLabel);
                preparedStatement->setString(queryParameterIndex++, metaDataContent);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionRootStatus::NotCompleted));
 
                preparedStatement->executeUpdate();
            }

            ingestionRootKey = getLastInsertId(conn);           
        }        
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return ingestionRootKey;
}

int64_t MMSEngineDBFacade::addIngestionJob (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
    	int64_t ingestionRootKey, string label, string metadataContent,
        MMSEngineDBFacade::IngestionType ingestionType, 
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess
)
{
    int64_t     ingestionJobKey;
    
    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "select c.isEnabled, c.workspaceType from MMS_Workspace c where c.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int isEnabled = resultSet->getInt("isEnabled");
                int workspaceType = resultSet->getInt("workspaceType");
                
                if (isEnabled != 1)
                {
                    string errorMessage = __FILEREF__ + "Workspace is not enabled"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
                else if (workspaceType != static_cast<int>(WorkspaceType::IngestionAndDelivery) &&
                        workspaceType != static_cast<int>(WorkspaceType::EncodingOnly))
                {
                    string errorMessage = __FILEREF__ + "Workspace is not enabled to ingest content"
                        + ", workspaceKey: " + to_string(workspaceKey);
                        + ", workspaceType: " + to_string(static_cast<int>(workspaceType))
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        IngestionStatus ingestionStatus;
        string errorMessage;
        {
            {
                lastSQLCommand = 
                    "insert into MMS_IngestionJob (ingestionJobKey, ingestionRootKey, label, metaDataContent, ingestionType, startProcessing, endProcessing, downloadingProgress, uploadingProgress, sourceBinaryTransferred, processorMMS, status, errorMessage) values ("
                                                  "NULL,            ?,                ?,     ?,               ?,             NULL,            NULL,         NULL,                NULL,              0,                       NULL,         ?,      NULL)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
                preparedStatement->setString(queryParameterIndex++, label);
                preparedStatement->setString(queryParameterIndex++, metadataContent);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(ingestionType));
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionStatus::Start_TaskQueued));

                preparedStatement->executeUpdate();
            }

            ingestionJobKey = getLastInsertId(conn);
            
            {
                if (dependOnIngestionJobKeys.size() == 0)
                {
                    int orderNumber = 0;

                    lastSQLCommand = 
                        "insert into MMS_IngestionJobDependency (ingestionJobDependencyKey, ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, orderNumber) values ("
                        "NULL, ?, ?, ?, ?)";

                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                    preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                    preparedStatement->setInt(queryParameterIndex++, orderNumber);

                    int rowsInserted = preparedStatement->executeUpdate();
                    
                    _logger->info(__FILEREF__ + "insert into MMS_IngestionJobDependency"
                        + ", getConnectionId: " + to_string(conn->getConnectionId())
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", dependOnSuccess: " + to_string(dependOnSuccess)
                        + ", orderNumber: " + to_string(orderNumber)
                        + ", rowsInserted: " + to_string(rowsInserted)
                    );
                }
                else
                {
                    int orderNumber = 0;
                    for (int64_t dependOnIngestionJobKey: dependOnIngestionJobKeys)
                    {
                        lastSQLCommand = 
                            "insert into MMS_IngestionJobDependency (ingestionJobDependencyKey, ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, orderNumber) values ("
                            "NULL, ?, ?, ?, ?)";

                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                        preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);
                        preparedStatement->setInt64(queryParameterIndex++, dependOnIngestionJobKey);
                        preparedStatement->setInt(queryParameterIndex++, orderNumber);

                        int rowsInserted = preparedStatement->executeUpdate();
                        
                        _logger->info(__FILEREF__ + "insert into MMS_IngestionJobDependency"
                            + ", getConnectionId: " + to_string(conn->getConnectionId())
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", dependOnSuccess: " + to_string(dependOnSuccess)
                            + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
                            + ", orderNumber: " + to_string(orderNumber)
                            + ", rowsInserted: " + to_string(rowsInserted)
                        );
                        
                        orderNumber++;
                    }
                }
            }
        }        
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return ingestionJobKey;
}

void MMSEngineDBFacade::updateIngestionJobMetadataContent (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        string metadataContent
)
{    
    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set metadataContent = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, metadataContent);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", metadataContent: " + metadataContent
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", metadataContent: " + metadataContent
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        updateIngestionJob (conn, ingestionJobKey, newIngestionStatus,
            errorMessage, processorMMS);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

void MMSEngineDBFacade::updateIngestionJob (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    try
    {
        string errorMessageForSQL;
        if (errorMessage.length() >= 1024)
            errorMessageForSQL = errorMessage.substr(0, 1024);
        else
            errorMessageForSQL = errorMessage;
        
        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
			if (processorMMS != "noToBeUpdated")
				lastSQLCommand = 
					"update MMS_IngestionJob set status = ?, endProcessing = NOW(), processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";
			else
				lastSQLCommand = 
					"update MMS_IngestionJob set status = ?, endProcessing = NOW(), errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
			if (processorMMS != "noToBeUpdated")
			{
				if (processorMMS == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, processorMMS);
			}
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

				// it is not so important to block the continuation of this method
				// Also the exception caused a crash of the process
                // throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
			if (processorMMS != "noToBeUpdated")
				lastSQLCommand = 
					"update MMS_IngestionJob set status = ?, processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";
			else
				lastSQLCommand = 
					"update MMS_IngestionJob set status = ?, errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
			if (processorMMS != "noToBeUpdated")
			{
				if (processorMMS == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, processorMMS);
			}
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

				// it is not so important to block the continuation of this method
				// Also the exception caused a crash of the process
                // throw runtime_error(errorMessage);                    
            }
        }
            
		bool updateIngestionRootStatus = true;
		manageIngestionJobStatusUpdate (ingestionJobKey, newIngestionStatus, updateIngestionRootStatus, conn);

        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", processorMMS: " + processorMMS
		);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::manageIngestionJobStatusUpdate (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
		bool updateIngestionRootStatus,
        shared_ptr<MySQLConnection> conn
)
{
    string      lastSQLCommand;
    

    try
    {
        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            int dependOnSuccess;

            if (MMSEngineDBFacade::isIngestionStatusSuccess(newIngestionStatus))
            {
                // set to NotToBeExecuted the tasks depending on this task on failure

                dependOnSuccess = 0;
            }
            else
            {
                // set to NotToBeExecuted the tasks depending on this task on success

                dependOnSuccess = 1;
            }

            // found all the ingestionJobKeys to be set as End_NotToBeExecuted
            string hierarchicalIngestionJobKeysDependencies;
            string ingestionJobKeysToFindDependencies = to_string(ingestionJobKey);
            {
                // all dependencies from ingestionJobKey and
                // all dependencies from the keys dependent from ingestionJobKey
                // and so on recursively
                int maxHierarchicalLevelsManaged = 50;
                for (int hierarchicalLevelIndex = 0; hierarchicalLevelIndex < maxHierarchicalLevelsManaged; hierarchicalLevelIndex++)
                {
                    lastSQLCommand = 
                            "select ingestionJobKey from MMS_IngestionJobDependency where dependOnIngestionJobKey in ("
                            + ingestionJobKeysToFindDependencies
                            + ") and dependOnSuccess = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);

                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                    bool dependenciesFound = false;
                    ingestionJobKeysToFindDependencies = "";
                    while (resultSet->next())
                    {
                        dependenciesFound = true;

                        if (hierarchicalIngestionJobKeysDependencies == "")
                            hierarchicalIngestionJobKeysDependencies = to_string(resultSet->getInt64("ingestionJobKey"));
                        else
                            hierarchicalIngestionJobKeysDependencies += (", " + to_string(resultSet->getInt64("ingestionJobKey")));
                        
                        if (ingestionJobKeysToFindDependencies == "")
                            ingestionJobKeysToFindDependencies = to_string(resultSet->getInt64("ingestionJobKey"));
                        else
                            ingestionJobKeysToFindDependencies += (", " + to_string(resultSet->getInt64("ingestionJobKey")));
                    }
                    
                    if (!dependenciesFound)
                    {
                        _logger->info(__FILEREF__ + "Finished to find dependencies"
                            + ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
                            + ", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        );

                        break;
                    }
                    else if (dependenciesFound && hierarchicalLevelIndex + 1 >= maxHierarchicalLevelsManaged)
                    {
                        string errorMessage = __FILEREF__ + "after maxHierarchicalLevelsManaged we still found dependencies, so maxHierarchicalLevelsManaged has to be increased"
                            + ", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->warn(errorMessage);
                    }            
                }
            }
            
            if (hierarchicalIngestionJobKeysDependencies != "")
            {
                lastSQLCommand = 
                    "update MMS_IngestionJob set status = ?, "
					"startProcessing = IF(startProcessing IS NULL, NOW(), startProcessing), "
					"endProcessing = NOW() where ingestionJobKey in (" + hierarchicalIngestionJobKeysDependencies + ")";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted));

                int rowsUpdated = preparedStatement->executeUpdate();
            }
        }            

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            int64_t ingestionRootKey;
            IngestionRootStatus currentIngestionRootStatus;

            {
                lastSQLCommand = 
                    "select ir.ingestionRootKey, ir.status "
                    "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                    "where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    ingestionRootKey = resultSet->getInt64("ingestionRootKey");
                    currentIngestionRootStatus = MMSEngineDBFacade::toIngestionRootStatus(resultSet->getString("Status"));                
                }
                else
                {
                    string errorMessage = __FILEREF__ + "IngestionJob is not found"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }

            int successStatesCount = 0;
            int failureStatesCount = 0;
            int intermediateStatesCount = 0;

            {
                lastSQLCommand = 
                    "select status from MMS_IngestionJob where ingestionRootKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                while (resultSet->next())
                {                
                    IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));

                    if (!MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatus))
                        intermediateStatesCount++;
                    else
                    {
                        if (MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatus))
                            successStatesCount++;
                        else
                            failureStatesCount++;
                    }
                }
            }

            _logger->info(__FILEREF__ + "Job status"
                + ", ingestionRootKey: " + to_string(ingestionRootKey)
                + ", intermediateStatesCount: " + to_string(intermediateStatesCount)
                + ", successStatesCount: " + to_string(successStatesCount)
                + ", failureStatesCount: " + to_string(failureStatesCount)
            );

            IngestionRootStatus newIngestionRootStatus;

            if (intermediateStatesCount > 0)
                newIngestionRootStatus = IngestionRootStatus::NotCompleted;
            else
            {
                if (failureStatesCount > 0)
                    newIngestionRootStatus = IngestionRootStatus::CompletedWithFailures;
                else
                    newIngestionRootStatus = IngestionRootStatus::CompletedSuccessful;
            }

            if (newIngestionRootStatus != currentIngestionRootStatus)            
            {
                lastSQLCommand = 
                    "update MMS_IngestionRoot set lastUpdate = NOW(), status = ? where ingestionRootKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionRootStatus));
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", ingestionRootKey: " + to_string(ingestionRootKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::setNotToBeExecutedStartingFrom (int64_t ingestionJobKey)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
		_logger->info(__FILEREF__ + "Update IngestionJob"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", IngestionStatus: " + "End_NotToBeExecuted"
			+ ", errorMessage: " + ""
			+ ", processorMMS: " + ""
		);
		updateIngestionJob (conn, ingestionJobKey,
			IngestionStatus::End_NotToBeExecuted,
			"",	// errorMessage,
			"" // processorMMS
		);

		// to set 'not to be executed' to the tasks depending from ingestionJobKey,, we will call manageIngestionJobStatusUpdate
		// simulating the IngestionJob failed, that cause the setting to 'not to be executed'
		// for the onSuccess tasks                                        

		bool updateIngestionRootStatus = false;
		manageIngestionJobStatusUpdate (ingestionJobKey, IngestionStatus::End_IngestionFailure, updateIngestionRootStatus, conn);

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
}

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage)
{
    
    bool        toBeCancelled = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set downloadingProgress = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, downloadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", downloadingPercentage: " + to_string(downloadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand = 
                "select status from MMS_IngestionJob where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));
                
                if (ingestionStatus == IngestionStatus::End_DwlUplOrEncCancelledByUser)
                    toBeCancelled = true;
            }
            else
            {
                string errorMessage = __FILEREF__ + "IngestionJob is not found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    
    return toBeCancelled;
}

bool MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage)
{
    
    bool        toBeCancelled = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set uploadingProgress = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, uploadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", uploadingPercentage: " + to_string(uploadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand = 
                "select status from MMS_IngestionJob where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));
                
                if (ingestionStatus == IngestionStatus::End_DwlUplOrEncCancelledByUser)
                    toBeCancelled = true;
            }
            else
            {
                string errorMessage = __FILEREF__ + "IngestionJob is not found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }

    return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set sourceBinaryTransferred = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, sourceBinaryTransferred ? 1 : 0);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

string MMSEngineDBFacade::getIngestionRootMetaDataContent (
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey
)
{    
    string      lastSQLCommand;
	string		metaDataContent;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                string("select metaDataContent from MMS_IngestionRoot where workspaceKey = ? and ingestionRootKey = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                metaDataContent = static_cast<string>(resultSet->getString("metaDataContent"));
            }
			else
            {
                string errorMessage = __FILEREF__ + "ingestion root not found"
                        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", ingestionRootKey: " + to_string(ingestionRootKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    
    return metaDataContent;
}

Json::Value MMSEngineDBFacade::getIngestionRootsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionRootKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string label, string status, bool asc
)
{    
    string      lastSQLCommand;
    Json::Value statusListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (ingestionRootKey != -1)
            {
                field = "ingestionRootKey";
                requestParametersRoot[field] = ingestionRootKey;
            }
            
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            field = "label";
            requestParametersRoot[field] = label;
            
            field = "status";
            requestParametersRoot[field] = status;
            
            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        if (ingestionRootKey != -1)
            sqlWhere += ("and ingestionRootKey = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (label != "")
            sqlWhere += ("and label like ? ");
        {
            string allStatus = "all";
            // compare case insensitive
            if (!(
                    status.length() != allStatus.length() ? false : 
                        equal(status.begin(), status.end(), allStatus.begin(),
                            [](int c1, int c2){ return toupper(c1) == toupper(c2); })
                ))
            {
                sqlWhere += ("and status = '" + status + "' ");
            }
        }
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionRootKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Json::Value workflowsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                string("select ingestionRootKey, label, status, "
                    "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
                    "DATE_FORMAT(convert_tz(lastUpdate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as lastUpdate "
                    "from MMS_IngestionRoot ")
                    + sqlWhere
                    + "order by ingestionDate"  + (asc ? " asc " : " desc ")
                    + "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionRootKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value workflowRoot;
                
                int64_t currentIngestionRootKey = resultSet->getInt64("ingestionRootKey");
                field = "ingestionRootKey";
                workflowRoot[field] = currentIngestionRootKey;

                field = "label";
                workflowRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "status";
                workflowRoot[field] = static_cast<string>(resultSet->getString("status"));

                field = "ingestionDate";
                workflowRoot[field] = static_cast<string>(resultSet->getString("ingestionDate"));

                field = "lastUpdate";
                workflowRoot[field] = static_cast<string>(resultSet->getString("lastUpdate"));

                Json::Value ingestionJobsRoot(Json::arrayValue);
                {            
                    lastSQLCommand = 
                        "select ingestionJobKey, label, ingestionType, metaDataContent, processorMMS, "
                        "DATE_FORMAT(convert_tz(startProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startProcessing, "
                        "DATE_FORMAT(convert_tz(endProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endProcessing, "
                        "IF(startProcessing is null, NOW(), startProcessing) as newStartProcessing, "
                        "IF(endProcessing is null, NOW(), endProcessing) as newEndProcessing, downloadingProgress, uploadingProgress, "
                        "status, errorMessage from MMS_IngestionJob where ingestionRootKey = ? "
                        "order by ingestionJobKey asc";
                        // "order by newStartProcessing asc, newEndProcessing asc";
                    shared_ptr<sql::PreparedStatement> preparedStatementIngestionJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementIngestionJob->setInt64(queryParameterIndex++, currentIngestionRootKey);
                    shared_ptr<sql::ResultSet> resultSetIngestionJob (preparedStatementIngestionJob->executeQuery());
                    while (resultSetIngestionJob->next())
                    {
                        Json::Value ingestionJobRoot = getIngestionJobRoot(
                                workspace, resultSetIngestionJob, currentIngestionRootKey, conn);

                        ingestionJobsRoot.append(ingestionJobRoot);
                    }
                }

                field = "ingestionJobs";
                workflowRoot[field] = ingestionJobsRoot;

                workflowsRoot.append(workflowRoot);
            }
        }
        
        field = "workflows";
        responseRoot[field] = workflowsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return statusListRoot;
}

Json::Value MMSEngineDBFacade::getIngestionJobsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
		string ingestionType,
        bool asc, string status
)
{    
    string      lastSQLCommand;
    Json::Value statusListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (ingestionJobKey != -1)
            {
                field = "ingestionJobKey";
                requestParametersRoot[field] = ingestionJobKey;
            }
            
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (ingestionType != "")
            {
                field = "ingestionType";
                requestParametersRoot[field] = ingestionType;
            }
            
            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where ir.ingestionRootKey = ij.ingestionRootKey ");
        sqlWhere += ("and ir.workspaceKey = ? ");
        if (ingestionJobKey != -1)
            sqlWhere += ("and ij.ingestionJobKey = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (ingestionType != "")
            sqlWhere += ("and ij.ingestionType = ? ");
        if (status == "completed")
            sqlWhere += ("and ij.status like 'End_%' ");
        else if (status == "notCompleted")
            sqlWhere += ("and ij.status not like 'End_%' ");
            
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (ingestionType != "")
                preparedStatement->setString(queryParameterIndex++, ingestionType);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Json::Value ingestionJobsRoot(Json::arrayValue);
        {            
            lastSQLCommand = 
                "select ir.ingestionRootKey, ij.ingestionJobKey, ij.label, ij.ingestionType, ij.metaDataContent, ij.processorMMS, "
                "DATE_FORMAT(convert_tz(ij.startProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startProcessing, "
                "DATE_FORMAT(convert_tz(ij.endProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endProcessing, "
                "IF(ij.startProcessing is null, NOW(), ij.startProcessing) as newStartProcessing, "
                "IF(ij.endProcessing is null, NOW(), ij.endProcessing) as newEndProcessing, ij.downloadingProgress, ij.uploadingProgress, "
                "ij.status, ij.errorMessage from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                + sqlWhere
                + "order by newStartProcessing" + (asc ? " asc" : " desc") + ", newEndProcessing " + 
                + "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (ingestionType != "")
                preparedStatement->setString(queryParameterIndex++, ingestionType);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value ingestionJobRoot = getIngestionJobRoot(
                        workspace, resultSet, resultSet->getInt64("ingestionRootKey"), conn);

                ingestionJobsRoot.append(ingestionJobRoot);
            }
        }
        
        field = "ingestionJobs";
        responseRoot[field] = ingestionJobsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return statusListRoot;
}

Json::Value MMSEngineDBFacade::getEncodingJobsStatus (
        shared_ptr<Workspace> workspace, int64_t encodingJobKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        bool asc, string status
)
{
    string      lastSQLCommand;
    Json::Value statusListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (encodingJobKey != -1)
            {
                field = "encodingJobKey";
                requestParametersRoot[field] = encodingJobKey;
            }
            
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            field = "status";
            requestParametersRoot[field] = status;

            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey ");
        sqlWhere += ("and ir.workspaceKey = ? ");
        if (encodingJobKey != -1)
            sqlWhere += ("and ej.encodingJobKey = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (status == "All")
            ;
        else if (status == "Completed")
            sqlWhere += ("and ej.status like 'End_%' ");
        else if (status == "Processing")
            sqlWhere += ("and ej.status = 'Processing' ");
        else if (status == "ToBeProcessed")
            sqlWhere += ("and ej.status = 'ToBeProcessed' ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (encodingJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Json::Value encodingJobsRoot(Json::arrayValue);
        {            
            lastSQLCommand = 
                "select ej.encodingJobKey, ej.type, ej.parameters, ej.status, ej.encodingProgress, "
				"ej.transcoder, ej.failuresNumber, ej.encodingPriority, "
                "DATE_FORMAT(convert_tz(ej.encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
                "DATE_FORMAT(convert_tz(ej.encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
                "IF(ij.startProcessing is null, NOW(), ij.startProcessing) as newStartProcessing, "
                "IF(ij.endProcessing is null, NOW(), ij.endProcessing) as newEndProcessing "
                "from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
                + sqlWhere
                + "order by newStartProcessing " + (asc ? "asc" : "desc") + ", newEndProcessing " + (asc ? "asc " : "desc ")
                + "limit ? offset ?";
            shared_ptr<sql::PreparedStatement> preparedStatementEncodingJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncodingJob->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (encodingJobKey != -1)
                preparedStatementEncodingJob->setInt64(queryParameterIndex++, encodingJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatementEncodingJob->setString(queryParameterIndex++, startIngestionDate);
                preparedStatementEncodingJob->setString(queryParameterIndex++, endIngestionDate);
            }
            preparedStatementEncodingJob->setInt(queryParameterIndex++, rows);
            preparedStatementEncodingJob->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSetEncodingJob (preparedStatementEncodingJob->executeQuery());
            while (resultSetEncodingJob->next())
            {
                Json::Value encodingJobRoot;

                int64_t encodingJobKey = resultSetEncodingJob->getInt64("encodingJobKey");
                
                field = "encodingJobKey";
                encodingJobRoot[field] = encodingJobKey;

                field = "type";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("type"));

                {
                    string parameters = resultSetEncodingJob->getString("parameters");

                    Json::Value parametersRoot;
                    if (parameters != "")
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(parameters.c_str(),
                                parameters.c_str() + parameters.size(), 
                                &parametersRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                    + ", encodingJobKey: " + to_string(encodingJobKey)
                                    + ", errors: " + errors
                                    + ", parameters: " + parameters
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }

                    field = "parameters";
                    encodingJobRoot[field] = parametersRoot;
                }

                field = "status";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("status"));
                EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSetEncodingJob->getString("status"));

                field = "progress";
                if (resultSetEncodingJob->isNull("encodingProgress"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingProgress");

                field = "start";
                if (encodingStatus == EncodingStatus::ToBeProcessed)
                    encodingJobRoot[field] = Json::nullValue;
                else
                {
                    if (resultSetEncodingJob->isNull("encodingJobStart"))
                        encodingJobRoot[field] = Json::nullValue;
                    else
                        encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobStart"));
                }

                field = "end";
                if (resultSetEncodingJob->isNull("encodingJobEnd"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobEnd"));

                field = "transcoder";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("transcoder"));

                field = "failuresNumber";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("failuresNumber");  

                field = "encodingPriority";
                encodingJobRoot[field] = toString(static_cast<EncodingPriority>(resultSetEncodingJob->getInt("encodingPriority")));

                field = "encodingPriorityCode";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingPriority");

                field = "maxEncodingPriorityCode";
                encodingJobRoot[field] = workspace->_maxEncodingPriority;

                encodingJobsRoot.append(encodingJobRoot);
            }
        }
        
        field = "encodingJobs";
        responseRoot[field] = encodingJobsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return statusListRoot;
}

Json::Value MMSEngineDBFacade::getIngestionJobRoot(
        shared_ptr<Workspace> workspace,
        shared_ptr<sql::ResultSet> resultSet,
        int64_t ingestionRootKey,
        shared_ptr<MySQLConnection> conn
)
{
    Json::Value ingestionJobRoot;
    string      lastSQLCommand;
    

    try
    {
        int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

        string field = "ingestionType";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("ingestionType"));
        IngestionType ingestionType = toIngestionType(resultSet->getString("ingestionType"));

        field = "ingestionJobKey";
        ingestionJobRoot[field] = ingestionJobKey;

        field = "metaDataContent";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("metaDataContent"));

        field = "processorMMS";
        if (resultSet->isNull("processorMMS"))
            ingestionJobRoot[field] = Json::nullValue;
        else
			ingestionJobRoot[field] = static_cast<string>(resultSet->getString("processorMMS"));

        field = "label";
        if (resultSet->isNull("label"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("label"));

        Json::Value mediaItemsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                "select mediaItemKey, physicalPathKey from MMS_IngestionJobOutput where ingestionJobKey = ? order by mediaItemKey";

            shared_ptr<sql::PreparedStatement> preparedStatementMediaItems (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementMediaItems->setInt64(queryParameterIndex++, ingestionJobKey);
            shared_ptr<sql::ResultSet> resultSetMediaItems (preparedStatementMediaItems->executeQuery());
            while (resultSetMediaItems->next())
            {
                Json::Value mediaItemRoot;

                field = "mediaItemKey";
                int64_t mediaItemKey = resultSetMediaItems->getInt64("mediaItemKey");
                mediaItemRoot[field] = mediaItemKey;

                field = "physicalPathKey";
                int64_t physicalPathKey = resultSetMediaItems->getInt64("physicalPathKey");
                mediaItemRoot[field] = physicalPathKey;

                mediaItemsRoot.append(mediaItemRoot);
            }
        }
        field = "mediaItems";
        ingestionJobRoot[field] = mediaItemsRoot;

        field = "startProcessing";
        if (resultSet->isNull("startProcessing"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("startProcessing"));

        field = "endProcessing";
        if (resultSet->isNull("endProcessing"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("endProcessing"));

        // if (ingestionType == IngestionType::AddContent)
        {
            field = "downloadingProgress";
            if (resultSet->isNull("downloadingProgress"))
                ingestionJobRoot[field] = Json::nullValue;
            else
                ingestionJobRoot[field] = resultSet->getInt64("downloadingProgress");
        }

        // if (ingestionType == IngestionType::AddContent)
        {
            field = "uploadingProgress";
            if (resultSet->isNull("uploadingProgress"))
                ingestionJobRoot[field] = Json::nullValue;
            else
                ingestionJobRoot[field] = resultSet->getInt64("uploadingProgress");
        }

        field = "ingestionRootKey";
        ingestionJobRoot[field] = ingestionRootKey;

        field = "status";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("status"));

        field = "errorMessage";
        if (resultSet->isNull("errorMessage"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("errorMessage"));

        if (ingestionType == IngestionType::Encode 
                || ingestionType == IngestionType::OverlayImageOnVideo
                || ingestionType == IngestionType::OverlayTextOnVideo
                || ingestionType == IngestionType::PeriodicalFrames
                || ingestionType == IngestionType::IFrames
                || ingestionType == IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == IngestionType::MotionJPEGByIFrames
                || ingestionType == IngestionType::Slideshow
                || ingestionType == IngestionType::FaceRecognition
                || ingestionType == IngestionType::FaceIdentification
                || ingestionType == IngestionType::LiveRecorder
                )
        {
            lastSQLCommand = 
                "select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
                "DATE_FORMAT(convert_tz(encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
                "DATE_FORMAT(convert_tz(encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
                "transcoder, failuresNumber from MMS_EncodingJob where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatementEncodingJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncodingJob->setInt64(queryParameterIndex++, ingestionJobKey);
            shared_ptr<sql::ResultSet> resultSetEncodingJob (preparedStatementEncodingJob->executeQuery());
            if (resultSetEncodingJob->next())
            {
                Json::Value encodingJobRoot;

                int64_t encodingJobKey = resultSetEncodingJob->getInt64("encodingJobKey");
                
                field = "encodingJobKey";
                encodingJobRoot[field] = encodingJobKey;

                field = "type";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("type"));

                {
                    string parameters = resultSetEncodingJob->getString("parameters");

                    Json::Value parametersRoot;
                    if (parameters != "")
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(parameters.c_str(),
                                parameters.c_str() + parameters.size(), 
                                &parametersRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                    + ", encodingJobKey: " + to_string(encodingJobKey)
                                    + ", errors: " + errors
                                    + ", parameters: " + parameters
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }

                    field = "parameters";
                    encodingJobRoot[field] = parametersRoot;
                }

                field = "status";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("status"));
                EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSetEncodingJob->getString("status"));

                field = "encodingPriority";
                encodingJobRoot[field] = toString(static_cast<EncodingPriority>(resultSetEncodingJob->getInt("encodingPriority")));

                field = "encodingPriorityCode";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingPriority");

                field = "maxEncodingPriorityCode";
                encodingJobRoot[field] = workspace->_maxEncodingPriority;

                field = "progress";
                if (resultSetEncodingJob->isNull("encodingProgress"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingProgress");

                field = "start";
                if (encodingStatus == EncodingStatus::ToBeProcessed)
                    encodingJobRoot[field] = Json::nullValue;
                else
                {
                    if (resultSetEncodingJob->isNull("encodingJobStart"))
                        encodingJobRoot[field] = Json::nullValue;
                    else
                        encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobStart"));
                }

                field = "end";
                if (resultSetEncodingJob->isNull("encodingJobEnd"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobEnd"));

                field = "transcoder";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("transcoder"));

                field = "failuresNumber";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("failuresNumber");  

                field = "encodingJob";
                ingestionJobRoot[field] = encodingJobRoot;
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return ingestionJobRoot;
}

void MMSEngineDBFacade::manageMainAndBackupOfRunnungLiveRecordingHA()

{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
			// if the ingestionJob is 'just' finished, anyway we have to get the ingestionJob in order to
			// remove the last backup file 
			int toleranceMinutes = 5;
            lastSQLCommand =
				string("select ingestionJobKey from MMS_IngestionJob "
					"where ingestionType = 'Live-Recorder' "
					"and JSON_EXTRACT(metaDataContent, '$.HighAvailability') = true "
					"and (status = 'EncodingQueued' "
					"or (status = 'End_TaskSuccess' and NOW() <= DATE_ADD(endProcessing, INTERVAL ? MINUTE))) "
				);
			// This select returns all the ingestion job key of running HA LiveRecording

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, toleranceMinutes);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

				// get all the couples, main and backup, and set one validated and the other not validated
				{
					_logger->info(__FILEREF__ + "Manage HA LiveRecording, main and backup (couple)"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						);

					lastSQLCommand =
						string("select JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') as utcChunkStartTime "
							"from MMS_MediaItem where JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
							"and retentionInMinutes != 0 group by utcChunkStartTime having count(*) = 2 for update"
						);
					// This select returns all the chunks (media item utcChunkStartTime) that are present two times
					// (because of HA live recording)

					shared_ptr<sql::PreparedStatement> preparedStatementChunkStartTime (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementChunkStartTime->setInt64(queryParameterIndex++, ingestionJobKey);
					shared_ptr<sql::ResultSet> resultSetChunkStartTime (preparedStatementChunkStartTime->executeQuery());
					while (resultSetChunkStartTime->next())
					{
						int64_t utcChunkStartTime = resultSetChunkStartTime->getInt64("utcChunkStartTime");

						_logger->info(__FILEREF__ + "Manage HA LiveRecording Chunk, main and backup (couple)"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

						int64_t mediaItemKeyChunk_1;
						bool mainChunk_1;
						int64_t durationInMilliSecondsChunk_1;                                                   
						int64_t mediaItemKeyChunk_2;
						bool mainChunk_2;
						int64_t durationInMilliSecondsChunk_2;                                                   

						lastSQLCommand =
							string("select mediaItemKey, CAST(JSON_EXTRACT(userData, '$.mmsData.main') as SIGNED INTEGER) as main from MMS_MediaItem "
								"where JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
								"and JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') = ? "
							);
						shared_ptr<sql::PreparedStatement> preparedStatementMediaItemDetails (conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementMediaItemDetails->setInt64(queryParameterIndex++, ingestionJobKey);
						preparedStatementMediaItemDetails->setInt64(queryParameterIndex++, utcChunkStartTime);
						shared_ptr<sql::ResultSet> resultSetMediaItemDetails (preparedStatementMediaItemDetails->executeQuery());
						if (resultSetMediaItemDetails->next())
						{
							mediaItemKeyChunk_1 = resultSetMediaItemDetails->getInt64("mediaItemKey");
							mainChunk_1 = resultSetMediaItemDetails->getInt("main") == 1 ? true : false;

							{
								int videoWidth;
								int videoHeight;
								long bitRate;
								string videoCodecName;
								string videoProfile;
								string videoAvgFrameRate;
								long videoBitRate;
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
									videoDetails = getVideoDetails(mediaItemKeyChunk_1, physicalPathKey);

								tie(durationInMilliSecondsChunk_1, bitRate,
									videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
									audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
							}
						}
						else
						{
							_logger->error(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						if (resultSetMediaItemDetails->next())
						{
							mediaItemKeyChunk_2 = resultSetMediaItemDetails->getInt64("mediaItemKey");
							mainChunk_2 = resultSetMediaItemDetails->getInt("main") == 1 ? true : false;

							{
								int videoWidth;
								int videoHeight;
								long bitRate;
								string videoCodecName;
								string videoProfile;
								string videoAvgFrameRate;
								long videoBitRate;
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
									videoDetails = getVideoDetails(mediaItemKeyChunk_2, physicalPathKey);

								tie(durationInMilliSecondsChunk_2, bitRate,
									videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
									audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;
							}
						}
						else
						{
							_logger->error(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						if (resultSetMediaItemDetails->next())
						{
							_logger->error(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						_logger->info(__FILEREF__ + "Manage HA LiveRecording Chunks, main and backup (couple)"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
							+ ", mainChunk_1: " + to_string(mainChunk_1)
							+ ", durationInMilliSecondsChunk_1: " + to_string(durationInMilliSecondsChunk_1)
							+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
							+ ", mainChunk_2: " + to_string(mainChunk_2)
							+ ", durationInMilliSecondsChunk_2: " + to_string(durationInMilliSecondsChunk_2)
							);

						{
							int64_t mediaItemKeyNotValidated;
							int64_t mediaItemKeyValidated;
							if (durationInMilliSecondsChunk_1 == durationInMilliSecondsChunk_2)
							{
								if (mainChunk_1)
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
								}
								else
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
								}
							}
							else if (durationInMilliSecondsChunk_1 < durationInMilliSecondsChunk_2)
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
							}
							else
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
							}

							_logger->info(__FILEREF__ + "Manage HA LiveRecording, reset of chunk, main and backup (couple)"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
								+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
								+ ", mainChunk_1: " + to_string(mainChunk_1)
								+ ", durationInMilliSecondsChunk_1: " + to_string(durationInMilliSecondsChunk_1)
								+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
								+ ", mainChunk_2: " + to_string(mainChunk_2)
								+ ", durationInMilliSecondsChunk_2: " + to_string(durationInMilliSecondsChunk_2)
								+ ", mediaItemKeyNotValidated: " + to_string(mediaItemKeyNotValidated)
								+ ", mediaItemKeyValidated: " + to_string(mediaItemKeyValidated)
							);

							// mediaItemKeyValidated
							{
								lastSQLCommand = 
									"update MMS_MediaItem set userData = JSON_INSERT(`userData`, '$.mmsData.validated', true) "
									"where mediaItemKey = ?";
								shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
								int queryParameterIndex = 1;
								preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyValidated);

								int rowsUpdated = preparedStatementUpdate->executeUpdate();            
								if (rowsUpdated != 1)
									_logger->error(__FILEREF__ + "It should never happen"
										+ ", mediaItemKeyToBeValidated: " + to_string(mediaItemKeyValidated)
									);
							}

							// mediaItemKeyNotValidated
							{
								lastSQLCommand = 
									"update MMS_MediaItem set retentionInMinutes = 0, userData = JSON_INSERT(`userData`, '$.mmsData.validated', false) "
									"where mediaItemKey = ?";
								shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
								int queryParameterIndex = 1;
								preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyNotValidated);

								int rowsUpdated = preparedStatementUpdate->executeUpdate();            
								if (rowsUpdated != 1)
									_logger->error(__FILEREF__ + "It should never happen"
										+ ", mediaItemKeyNotValidated: " + to_string(mediaItemKeyNotValidated)
									);
							}
						}
					}
				}

				// get all the singles, main or backup, that, after a while, was not marked as validated ot not. Mark them as validated.
				// This is the scenario where just one chunk is generated, main or backup, So it has to be marked as validated
				{
					int chunksToBeManagedWithinSeconds = 60;

					_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single)"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						);

					lastSQLCommand =
						string("select mediaItemKey  from MMS_MediaItem where "
							"JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' "
							"and JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
							"and JSON_EXTRACT(userData, '$.mmsData.validated') is null "
							"and NOW() > DATE_ADD(ingestionDate, INTERVAL ? SECOND) "
							"and retentionInMinutes != 0 for update"
						);

					shared_ptr<sql::PreparedStatement> preparedStatementMediaItemKey (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementMediaItemKey->setInt64(queryParameterIndex++, ingestionJobKey);
					preparedStatementMediaItemKey->setInt(queryParameterIndex++, chunksToBeManagedWithinSeconds);
					shared_ptr<sql::ResultSet> resultSetMediaItemKey (preparedStatementMediaItemKey->executeQuery());
					while (resultSetMediaItemKey->next())
					{
						int64_t mediaItemKeyChunk = resultSetMediaItemKey->getInt64("mediaItemKey");

						_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single), set to validated"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
							);

						{
							lastSQLCommand = 
								"update MMS_MediaItem set userData = JSON_INSERT(`userData`, '$.mmsData.validated', true) "
								"where mediaItemKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyChunk);

							int rowsUpdated = preparedStatementUpdate->executeUpdate();            
							if (rowsUpdated != 1)
								_logger->error(__FILEREF__ + "It should never happen"
									+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
								);
						}
					}
				}
            }
        }
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
}

Json::Value MMSEngineDBFacade::getMediaItemsList (
        int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, string jsonCondition,
		vector<string>& tags,
        string ingestionDateOrder,   // "" or "asc" or "desc"
		string jsonOrderBy,
		bool admin
)
{
    string      lastSQLCommand;
    Json::Value mediaItemsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getMediaItemsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
            + ", startAndEndIngestionDatePresent: " + to_string(startAndEndIngestionDatePresent)
            + ", startIngestionDate: " + (startAndEndIngestionDatePresent ? startIngestionDate : "")
            + ", endIngestionDate: " + (startAndEndIngestionDatePresent ? endIngestionDate : "")
            + ", title: " + title
            + ", liveRecordingChunk: " + to_string(liveRecordingChunk)
            + ", jsonCondition: " + jsonCondition
            + ", ingestionDateOrder: " + ingestionDateOrder
            + ", jsonOrderBy: " + jsonOrderBy
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (mediaItemKey != -1)
            {
                field = "mediaItemKey";
                requestParametersRoot[field] = mediaItemKey;
            }
            
            if (physicalPathKey != -1)
            {
                field = "physicalPathKey";
                requestParametersRoot[field] = physicalPathKey;
            }

            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }
            
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (title != "")
            {
                field = "title";
                requestParametersRoot[field] = title;
            }

            if (liveRecordingChunk != -1)
            {
                field = "liveRecordingChunk";
                requestParametersRoot[field] = liveRecordingChunk;
            }

            if (jsonCondition != "")
            {
                field = "jsonCondition";
                requestParametersRoot[field] = jsonCondition;
            }

            if (ingestionDateOrder != "")
            {
                field = "ingestionDateOrder";
                requestParametersRoot[field] = ingestionDateOrder;
            }

            if (jsonOrderBy != "")
            {
                field = "jsonOrderBy";
                requestParametersRoot[field] = jsonOrderBy;
            }

            field = "requestParameters";
            mediaItemsListRoot[field] = requestParametersRoot;
        }
        
        int64_t newMediaItemKey = mediaItemKey;
        if (mediaItemKey == -1 && physicalPathKey != -1)
        {
            lastSQLCommand = 
                string("select mediaItemKey from MMS_PhysicalPath where physicalPathKey = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                newMediaItemKey = resultSet->getInt64("mediaItemKey");
            }
            else
            {
                string errorMessage (__FILEREF__ + "getMediaItemsList: requested physicalPathKey does not exist");
                _logger->error(errorMessage);

                // throw runtime_error(errorMessage);
				newMediaItemKey = 0;	// let's force a MIK that does not exist
            }
        }
        
        string sqlWhere;
		if (tags.size() > 0)
			sqlWhere = string ("where mi.mediaItemKey = t.mediaItemKey and mi.workspaceKey = ? ");
		else
			sqlWhere = string ("where mi.workspaceKey = ? ");
        if (newMediaItemKey != -1)
            sqlWhere += ("and mi.mediaItemKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and mi.contentType = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and mi.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and mi.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (title != "")
            sqlWhere += ("and LOWER(mi.title) like LOWER(?) ");		// LOWER was used because the column is using utf8_bin that is case sensitive
		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk
		 *  1: look for liveRecordingChunk
		 */
        if (liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
			{
				sqlWhere += ("and (JSON_EXTRACT(userData, '$.mmsData.dataType') is NULL ");
				sqlWhere += ("OR JSON_EXTRACT(userData, '$.mmsData.dataType') != 'liveRecordingChunk') ");
			}
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' ");
		}
        if (jsonCondition != "")
            sqlWhere += ("and " + jsonCondition);
		if (tags.size() > 0)
		{
			string tagsCondition;

			for (int tagIndex = 0; tagIndex < tags.size(); tagIndex++)
			{
				if (tagsCondition == "")
					tagsCondition = ("?");
				else
					tagsCondition.append(", ?");
			}

            sqlWhere += ("and t.name in (" + tagsCondition + ") ");
		}
        
        Json::Value responseRoot;
        {
			if (tags.size() > 0)
			{
				lastSQLCommand = 
					string("select count(*) from MMS_MediaItem mi, MMS_Tag t ")
					+ sqlWhere;
			}
			else
			{
				lastSQLCommand = 
					string("select count(*) from MMS_MediaItem mi ")
					+ sqlWhere;
			}

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (newMediaItemKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, newMediaItemKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (tags.size() > 0)
			{
				for (int tagIndex = 0; tagIndex < tags.size(); tagIndex++)
				{
					string tag = tags[tagIndex];

                			preparedStatement->setString(queryParameterIndex++, tag);
				}
			}
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        {
            int64_t workSpaceUsageInBytes;
            int64_t maxStorageInMB;

            pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB = getWorkspaceUsage(conn, workspaceKey);
            tie(workSpaceUsageInBytes, maxStorageInMB) = workSpaceUsageInBytesAndMaxStorageInMB;
            
            int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;
            
            field = "workSpaceUsageInMB";
            responseRoot[field] = workSpaceUsageInMB;

            field = "maxStorageInMB";
            responseRoot[field] = maxStorageInMB;
        }

        Json::Value mediaItemsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (ingestionDateOrder == "" && jsonOrderBy == "")
			{
				orderByCondition = " ";
			}
			else if (ingestionDateOrder == "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + " ";
			}
			else if (ingestionDateOrder != "" && jsonOrderBy == "")
			{
				orderByCondition = "order by mi.ingestionDate " + ingestionDateOrder + " ";
			}
			else // if (ingestionDateOrder != "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + ", mi.ingestionDate " + ingestionDateOrder + " ";
			}

			if (tags.size() > 0)
			{
            		lastSQLCommand = 
                		string("select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
                    			"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
                    			"DATE_FORMAT(convert_tz(mi.startPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startPublishing, "
                    			"DATE_FORMAT(convert_tz(mi.endPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endPublishing, "
                    			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi, MMS_Tag t ")
                    			+ sqlWhere
                    			+ orderByCondition
                    			+ "limit ? offset ?";
			}
			else
			{
            		lastSQLCommand = 
                		string("select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
                    			"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
                    			"DATE_FORMAT(convert_tz(mi.startPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startPublishing, "
                    			"DATE_FORMAT(convert_tz(mi.endPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endPublishing, "
                    			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi ")
                    			+ sqlWhere
                    			+ orderByCondition
                    			+ "limit ? offset ?";
			}

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (newMediaItemKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, newMediaItemKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (tags.size() > 0)
			{
				for (int tagIndex = 0; tagIndex < tags.size(); tagIndex++)
				{
					string tag = tags[tagIndex];

					preparedStatement->setString(queryParameterIndex++, tag);
				}
			}
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value mediaItemRoot;

                int64_t localMediaItemKey = resultSet->getInt64("mediaItemKey");

                field = "mediaItemKey";
                mediaItemRoot[field] = localMediaItemKey;

                field = "title";
                mediaItemRoot[field] = static_cast<string>(resultSet->getString("title"));

                field = "deliveryFileName";
                if (resultSet->isNull("deliveryFileName"))
                    mediaItemRoot[field] = Json::nullValue;
                else
                    mediaItemRoot[field] = static_cast<string>(resultSet->getString("deliveryFileName"));

                field = "ingester";
                if (resultSet->isNull("ingester"))
                    mediaItemRoot[field] = Json::nullValue;
                else
                    mediaItemRoot[field] = static_cast<string>(resultSet->getString("ingester"));

                field = "userData";
                if (resultSet->isNull("userData"))
                    mediaItemRoot[field] = Json::nullValue;
                else
                    mediaItemRoot[field] = static_cast<string>(resultSet->getString("userData"));

                field = "ingestionDate";
                mediaItemRoot[field] = static_cast<string>(resultSet->getString("ingestionDate"));

                field = "startPublishing";
                mediaItemRoot[field] = static_cast<string>(resultSet->getString("startPublishing"));
                field = "endPublishing";
                mediaItemRoot[field] = static_cast<string>(resultSet->getString("endPublishing"));

                ContentType contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                field = "contentType";
                mediaItemRoot[field] = static_cast<string>(resultSet->getString("contentType"));

                field = "retentionInMinutes";
                mediaItemRoot[field] = resultSet->getInt64("retentionInMinutes");
                
                int64_t contentProviderKey = resultSet->getInt64("contentProviderKey");
                
                {                    
                    lastSQLCommand = 
                        "select name from MMS_ContentProvider where contentProviderKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProvider (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProvider->setInt64(queryParameterIndex++, contentProviderKey);
                    shared_ptr<sql::ResultSet> resultSetProviders (preparedStatementProvider->executeQuery());
                    if (resultSetProviders->next())
                    {
                        field = "providerName";
                        mediaItemRoot[field] = static_cast<string>(resultSetProviders->getString("name"));
                    }
                    else
                    {
                        string errorMessage = string("content provider does not exist")
                            + ", contentProviderKey: " + to_string(contentProviderKey)
                        ;

                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }

                {
                    Json::Value mediaItemTagsRoot(Json::arrayValue);
                    
                    lastSQLCommand = 
                        "select name from MMS_Tag where mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementTags (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementTags->setInt64(queryParameterIndex++, localMediaItemKey);
                    shared_ptr<sql::ResultSet> resultSetTags (preparedStatementTags->executeQuery());
                    while (resultSetTags->next())
                    {
                        mediaItemTagsRoot.append(static_cast<string>(resultSetTags->getString("name")));
                    }
                    
                    field = "tags";
                    mediaItemRoot[field] = mediaItemTagsRoot;
                }

                {
                    Json::Value mediaItemProfilesRoot(Json::arrayValue);
                    
                    lastSQLCommand = 
                        "select physicalPathKey, fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, "
                        "DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                        "from MMS_PhysicalPath where mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfiles (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfiles->setInt64(queryParameterIndex++, localMediaItemKey);
                    shared_ptr<sql::ResultSet> resultSetProfiles (preparedStatementProfiles->executeQuery());
                    while (resultSetProfiles->next())
                    {
                        Json::Value profileRoot;
                        
                        int64_t physicalPathKey = resultSetProfiles->getInt64("physicalPathKey");

                        field = "physicalPathKey";
                        profileRoot[field] = physicalPathKey;


                        field = "fileFormat";
                        string fileName = resultSetProfiles->getString("fileName");
                        size_t extensionIndex = fileName.find_last_of(".");
                        if (extensionIndex == string::npos)
                        {
                            profileRoot[field] = Json::nullValue;
                        }
                        else
                            profileRoot[field] = fileName.substr(extensionIndex + 1);
                        
						if (admin)
						{
							field = "partitionNumber";
							profileRoot[field] = resultSetProfiles->getInt("partitionNumber");

							field = "relativePath";
							profileRoot[field] = static_cast<string>(resultSetProfiles->getString("relativePath"));

							field = "fileName";
							profileRoot[field] = fileName;
						}

                        field = "encodingProfileKey";
                        if (resultSetProfiles->isNull("encodingProfileKey"))
                            profileRoot[field] = Json::nullValue;
                        else
                            profileRoot[field] = resultSetProfiles->getInt64("encodingProfileKey");

                        field = "sizeInBytes";
                        profileRoot[field] = resultSetProfiles->getInt64("sizeInBytes");

                        field = "creationDate";
                        profileRoot[field] = static_cast<string>(resultSetProfiles->getString("creationDate"));

                        if (contentType == ContentType::Video)
                        {
                            int64_t durationInMilliSeconds;
                            int videoWidth;
                            int videoHeight;
                            long bitRate;
                            string videoCodecName;
                            string videoProfile;
                            string videoAvgFrameRate;
                            long videoBitRate;
                            string audioCodecName;
                            long audioSampleRate;
                            int audioChannels;
                            long audioBitRate;

                            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
                                videoDetails = getVideoDetails(localMediaItemKey, physicalPathKey);

                            tie(durationInMilliSeconds, bitRate,
                                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;

                            Json::Value videoDetailsRoot;

                            field = "durationInMilliSeconds";
                            videoDetailsRoot[field] = durationInMilliSeconds;

                            field = "videoWidth";
                            videoDetailsRoot[field] = videoWidth;

                            field = "videoHeight";
                            videoDetailsRoot[field] = videoHeight;

                            field = "bitRate";
                            videoDetailsRoot[field] = (int64_t) bitRate;

                            field = "videoCodecName";
                            videoDetailsRoot[field] = videoCodecName;

                            field = "videoProfile";
                            videoDetailsRoot[field] = videoProfile;

                            field = "videoAvgFrameRate";
                            videoDetailsRoot[field] = videoAvgFrameRate;

                            field = "videoBitRate";
                            videoDetailsRoot[field] = (int64_t) videoBitRate;

                            field = "audioCodecName";
                            videoDetailsRoot[field] = audioCodecName;

                            field = "audioSampleRate";
                            videoDetailsRoot[field] = (int64_t) audioSampleRate;

                            field = "audioChannels";
                            videoDetailsRoot[field] = audioChannels;

                            field = "audioBitRate";
                            videoDetailsRoot[field] = (int64_t) audioBitRate;


                            field = "videoDetails";
                            profileRoot[field] = videoDetailsRoot;
                        }
                        else if (contentType == ContentType::Audio)
                        {
                            int64_t durationInMilliSeconds;
                            string codecName;
                            long bitRate;
                            long sampleRate;
                            int channels;

                            tuple<int64_t,string,long,long,int>
                                audioDetails = getAudioDetails(localMediaItemKey, physicalPathKey);

                            tie(durationInMilliSeconds, codecName, bitRate, sampleRate, channels) 
                                    = audioDetails;

                            Json::Value audioDetailsRoot;

                            field = "durationInMilliSeconds";
                            audioDetailsRoot[field] = durationInMilliSeconds;

                            field = "codecName";
                            audioDetailsRoot[field] = codecName;

                            field = "bitRate";
                            audioDetailsRoot[field] = (int64_t) bitRate;

                            field = "sampleRate";
                            audioDetailsRoot[field] = (int64_t) sampleRate;

                            field = "channels";
                            audioDetailsRoot[field] = channels;


                            field = "audioDetails";
                            profileRoot[field] = audioDetailsRoot;
                        }
                        else if (contentType == ContentType::Image)
                        {
                            int width;
                            int height;
                            string format;
                            int quality;

                            tuple<int,int,string,int>
                                imageDetails = getImageDetails(localMediaItemKey, physicalPathKey);

                            tie(width, height, format, quality) 
                                    = imageDetails;

                            Json::Value imageDetailsRoot;

                            field = "width";
                            imageDetailsRoot[field] = width;

                            field = "height";
                            imageDetailsRoot[field] = height;

                            field = "format";
                            imageDetailsRoot[field] = format;

                            field = "quality";
                            imageDetailsRoot[field] = quality;


                            field = "imageDetails";
                            profileRoot[field] = imageDetailsRoot;
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "ContentType unmanaged"
                                + ", mediaItemKey: " + to_string(localMediaItemKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);  
                        }

                        mediaItemProfilesRoot.append(profileRoot);
                    }
                    
                    field = "physicalPaths";
                    mediaItemRoot[field] = mediaItemProfilesRoot;
                }

                mediaItemsRoot.append(mediaItemRoot);
            }
        }

        field = "mediaItems";
        responseRoot[field] = mediaItemsRoot;

        field = "response";
        mediaItemsListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return mediaItemsListRoot;
}

Json::Value MMSEngineDBFacade::getEncodingProfilesSetList (
        int64_t workspaceKey, int64_t encodingProfilesSetKey,
        bool contentTypePresent, ContentType contentType
)
{    
    string      lastSQLCommand;
    Json::Value contentListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEncodingProfilesSetList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            if (encodingProfilesSetKey != -1)
            {
                field = "encodingProfilesSetKey";
                requestParametersRoot[field] = encodingProfilesSetKey;
            }
            
            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }
            
            field = "requestParameters";
            contentListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        if (encodingProfilesSetKey != -1)
            sqlWhere += ("and encodingProfilesSetKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and contentType = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_EncodingProfilesSet ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodingProfilesSetKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        Json::Value encodingProfilesSetsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                string("select encodingProfilesSetKey, contentType, label from MMS_EncodingProfilesSet ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodingProfilesSetKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value encodingProfilesSetRoot;

                int64_t localEncodingProfilesSetKey = resultSet->getInt64("encodingProfilesSetKey");

                field = "encodingProfilesSetKey";
                encodingProfilesSetRoot[field] = localEncodingProfilesSetKey;

                field = "label";
                encodingProfilesSetRoot[field] = static_cast<string>(resultSet->getString("label"));

                ContentType contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                field = "contentType";
                encodingProfilesSetRoot[field] = static_cast<string>(resultSet->getString("contentType"));

                Json::Value encodingProfilesRoot(Json::arrayValue);
                {                    
                    lastSQLCommand = 
                        "select ep.encodingProfileKey, ep.contentType, ep.label, ep.technology, ep.jsonProfile "
                        "from MMS_EncodingProfilesSetMapping epsm, MMS_EncodingProfile ep "
                        "where epsm.encodingProfileKey = ep.encodingProfileKey and "
                        "epsm.encodingProfilesSetKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfile (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfile->setInt64(queryParameterIndex++, localEncodingProfilesSetKey);
                    shared_ptr<sql::ResultSet> resultSetProfile (preparedStatementProfile->executeQuery());
                    while (resultSetProfile->next())
                    {
                        Json::Value encodingProfileRoot;
                        
                        field = "encodingProfileKey";
                        encodingProfileRoot[field] = resultSetProfile->getInt64("encodingProfileKey");
                
                        field = "label";
                        encodingProfileRoot[field] = static_cast<string>(resultSetProfile->getString("label"));

                        field = "contentType";
                        encodingProfileRoot[field] = static_cast<string>(resultSetProfile->getString("contentType"));
                        
                        field = "technology";
                        encodingProfileRoot[field] = static_cast<string>(resultSetProfile->getString("technology"));

                        {
                            string jsonProfile = resultSetProfile->getString("jsonProfile");
                            
                            Json::Value profileRoot;
                            try
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(jsonProfile.c_str(),
                                        jsonProfile.c_str() + jsonProfile.size(), 
                                        &profileRoot, &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = string("Json metadata failed during the parsing")
                                            + ", errors: " + errors
                                            + ", json data: " + jsonProfile
                                            ;
                                    _logger->error(__FILEREF__ + errorMessage);

                                    continue;
                                }
                            }
                            catch(exception e)
                            {
                                string errorMessage = string("Json metadata failed during the parsing"
                                        ", json data: " + jsonProfile
                                        );
                                _logger->error(__FILEREF__ + errorMessage);

                                continue;
                            }
                            
                            field = "profile";
                            encodingProfileRoot[field] = profileRoot;
                        }
                        
                        encodingProfilesRoot.append(encodingProfileRoot);
                    }
                }

                field = "encodingProfiles";
                encodingProfilesSetRoot[field] = encodingProfilesRoot;
                
                encodingProfilesSetsRoot.append(encodingProfilesSetRoot);
            }
        }

        field = "encodingProfilesSets";
        responseRoot[field] = encodingProfilesSetsRoot;

        field = "response";
        contentListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return contentListRoot;
}

Json::Value MMSEngineDBFacade::getEncodingProfileList (
        int64_t workspaceKey, int64_t encodingProfileKey,
        bool contentTypePresent, ContentType contentType
)
{    
    string      lastSQLCommand;
    Json::Value contentListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEncodingProfileList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            if (encodingProfileKey != -1)
            {
                field = "encodingProfileKey";
                requestParametersRoot[field] = encodingProfileKey;
            }
            
            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }
            
            field = "requestParameters";
            contentListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where (workspaceKey = ? or workspaceKey is null) ");
        if (encodingProfileKey != -1)
            sqlWhere += ("and encodingProfileKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and contentType = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_EncodingProfile ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodingProfileKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        Json::Value encodingProfilesRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select encodingProfileKey, label, contentType, technology, jsonProfile from MMS_EncodingProfile ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodingProfileKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value encodingProfileRoot;

                field = "encodingProfileKey";
                encodingProfileRoot[field] = resultSet->getInt64("encodingProfileKey");

                field = "label";
                encodingProfileRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "contentType";
                encodingProfileRoot[field] = static_cast<string>(resultSet->getString("contentType"));

                field = "technology";
                encodingProfileRoot[field] = static_cast<string>(resultSet->getString("technology"));

                {
                    string jsonProfile = resultSet->getString("jsonProfile");

                    Json::Value profileRoot;
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(jsonProfile.c_str(),
                                jsonProfile.c_str() + jsonProfile.size(), 
                                &profileRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = string("Json metadata failed during the parsing")
                                    + ", errors: " + errors
                                    + ", json data: " + jsonProfile
                                    ;
                            _logger->error(__FILEREF__ + errorMessage);

                            continue;
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Json metadata failed during the parsing"
                                ", json data: " + jsonProfile
                                );
                        _logger->error(__FILEREF__ + errorMessage);

                        continue;
                    }

                    field = "profile";
                    encodingProfileRoot[field] = profileRoot;
                }

                encodingProfilesRoot.append(encodingProfileRoot);
            }
        }

        field = "encodingProfiles";
        responseRoot[field] = encodingProfilesRoot;

        field = "response";
        contentListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return contentListRoot;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
    int64_t referenceMediaItemKey, int64_t encodingProfileKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    int64_t physicalPathKey = -1;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceMediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                physicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/encodingProfileKey are not found"
                    + ", mediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    
    return physicalPathKey;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
        int64_t workspaceKey, 
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    int64_t physicalPathKey = -1;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t encodingProfileKey = -1;
        {
            lastSQLCommand = 
                "select encodingProfileKey from MMS_EncodingProfile where (workspaceKey = ? or workspaceKey is null) and contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, toString(contentType));
            preparedStatement->setString(queryParameterIndex++, encodingProfileLabel);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfileKey = resultSet->getInt64("encodingProfileKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey is not found"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", contentType: " + toString(contentType)
                    + ", encodingProfileLabel: " + encodingProfileLabel
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                physicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/encodingProfileKey are not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    
    return physicalPathKey;
}

tuple<MMSEngineDBFacade::ContentType,string,string,string> MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t mediaItemKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<MMSEngineDBFacade::ContentType,string,string,string>
		contentTypeTitleUserDataAndIngestionDate;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select contentType, title, userData, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));

                string userData;
                if (!resultSet->isNull("userData"))
                    userData = resultSet->getString("userData");

                string title;
                if (!resultSet->isNull("title"))
                    title = resultSet->getString("title");
                
                string ingestionDate;
                if (!resultSet->isNull("ingestionDate"))
                    ingestionDate = resultSet->getString("ingestionDate");
                
                contentTypeTitleUserDataAndIngestionDate = make_tuple(contentType, title, userData, ingestionDate);
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }             
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return contentTypeTitleUserDataAndIngestionDate;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string> MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(
    int64_t physicalPathKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string>
		mediaItemKeyContentTypeTitleUserDataAndIngestionDate;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
                "where mi.mediaItemKey = p.mediaItemKey and p.physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));

                string userData;
                if (!resultSet->isNull("userData"))
                    userData = resultSet->getString("userData");
                
                string title;
                if (!resultSet->isNull("title"))
                    title = resultSet->getString("title");

                string ingestionDate;
                if (!resultSet->isNull("ingestionDate"))
                    ingestionDate = resultSet->getString("ingestionDate");

                mediaItemKeyContentTypeTitleUserDataAndIngestionDate =
					make_tuple(mediaItemKey, contentType, title, userData, ingestionDate);                
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", physicalPathKey: " + to_string(physicalPathKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
        
        return mediaItemKeyContentTypeTitleUserDataAndIngestionDate;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

void MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey(
    int64_t referenceIngestionJobKey, 
        vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>>& mediaItemsDetails,
        bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            // order by in the next select is important  to have the right order in case of dependency in a workflow
            lastSQLCommand = 
                "select mediaItemKey, physicalPathKey from MMS_IngestionJobOutput where ingestionJobKey = ? order by mediaItemKey";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceIngestionJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                int64_t physicalPathKey = resultSet->getInt64("physicalPathKey");

                ContentType contentType;
                {
                    lastSQLCommand = 
                        "select contentType from MMS_MediaItem where mediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementMediaItem (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementMediaItem->setInt64(queryParameterIndex++, mediaItemKey);

                    shared_ptr<sql::ResultSet> resultSetMediaItem (preparedStatementMediaItem->executeQuery());
                    if (resultSetMediaItem->next())
                    {
                        contentType = MMSEngineDBFacade::toContentType(resultSetMediaItem->getString("contentType"));
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                            + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                            + ", mediaItemKey: " + to_string(mediaItemKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        if (warningIfMissing)
                            _logger->warn(errorMessage);
                        else
                            _logger->error(errorMessage);

                        throw MediaItemKeyNotFound(errorMessage);                    
                    }            
                }

                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType 
                        = make_tuple(mediaItemKey, physicalPathKey, contentType);
                mediaItemsDetails.push_back(mediaItemKeyPhysicalPathKeyAndContentType);
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "MediaItemKeyNotFound SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "MediaItemKeyNotFound SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

pair<int64_t,MMSEngineDBFacade::ContentType> MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
    int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType from MMS_MediaItem mi, MMS_ExternalUniqueName eun"
                "where mi.mediaItemKey == eun.mediaItemKey "
                "and eun.workspaceKey = ? and eun.uniqueName = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, referenceUniqueName);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                mediaItemKeyAndContentType.first = resultSet->getInt64("mediaItemKey");
                mediaItemKeyAndContentType.second = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", referenceUniqueName: " + referenceUniqueName
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    
    return mediaItemKeyAndContentType;
}

tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> MMSEngineDBFacade::getVideoDetails(
    int64_t mediaItemKey, int64_t physicalPathKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
        int64_t durationInMilliSeconds;
        long bitRate;
        string videoCodecName;
        string videoProfile;
        int videoWidth;
        int videoHeight;
        string videoAvgFrameRate;
        long videoBitRate;
        string audioCodecName;
        long audioSampleRate;
        int audioChannels;
        long audioBitRate;

        {
            lastSQLCommand = 
                "select durationInMilliSeconds, bitRate, width, height, avgFrameRate, "
                "videoCodecName, videoProfile, videoBitRate, "
                "audioCodecName, audioSampleRate, audioChannels, audioBitRate "
                "from MMS_VideoItemProfile where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                bitRate = resultSet->getInt("bitRate");
                videoCodecName = resultSet->getString("videoCodecName");
                videoProfile = resultSet->getString("videoProfile");
                videoWidth = resultSet->getInt("width");
                videoHeight = resultSet->getInt("height");
                videoAvgFrameRate = resultSet->getString("avgFrameRate");
                videoBitRate = resultSet->getInt("videoBitRate");
                audioCodecName = resultSet->getString("audioCodecName");
                audioSampleRate = resultSet->getInt("audioSampleRate");
                audioChannels = resultSet->getInt("audioChannels");
                audioBitRate = resultSet->getInt("audioBitRate");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
        
        return make_tuple(durationInMilliSeconds, bitRate,
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

tuple<int64_t,string,long,long,int> MMSEngineDBFacade::getAudioDetails(
    int64_t mediaItemKey, int64_t physicalPathKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
        int64_t durationInMilliSeconds;
        string codecName;
        long bitRate;
        long sampleRate;
        int channels;

        {
            lastSQLCommand = 
                "select durationInMilliSeconds, codecName, bitRate, sampleRate, channels "
                "from MMS_AudioItemProfile where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                codecName = resultSet->getString("codecName");
                bitRate = resultSet->getInt("bitRate");
                sampleRate = resultSet->getInt("sampleRate");
                channels = resultSet->getInt("channels");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
        
        return make_tuple(durationInMilliSeconds, codecName, bitRate, sampleRate, channels);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

tuple<int,int,string,int> MMSEngineDBFacade::getImageDetails(
    int64_t mediaItemKey, int64_t physicalPathKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
        int width;
        int height;
        string format;
        int quality;

        {
            lastSQLCommand = 
                "select width, height, format, quality "
                "from MMS_ImageItemProfile where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                width = resultSet->getInt("width");
                height = resultSet->getInt("height");
                format = resultSet->getString("format");
                quality = resultSet->getInt("quality");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
        
        return make_tuple(width, height, format, quality);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }    
}

void MMSEngineDBFacade::getEncodingJobs(
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems,
		int maxEncodingsNumber
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
            int retentionDaysToReset = 7;
            
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ? "
                "and DATE_ADD(encodingJobStart, INTERVAL ? DAY) <= NOW()";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
            preparedStatement->setInt(queryParameterIndex++, retentionDaysToReset);

            int rowsExpired = preparedStatement->executeUpdate();            
            if (rowsExpired > 0)
                _logger->warn(__FILEREF__ + "Rows (MMS_EncodingJob) that were expired"
                    + ", rowsExpired: " + to_string(rowsExpired)
                );
        }
        
        {
            lastSQLCommand = 
                "select encodingJobKey, ingestionJobKey, type, parameters, encodingPriority from MMS_EncodingJob " 
                "where processorMMS is null and status = ? and encodingJobStart <= NOW() "
                "order by encodingPriority desc, encodingJobStart asc, failuresNumber asc limit ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatementEncoding->setInt(queryParameterIndex++, maxEncodingsNumber);

            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();
                
                encodingItem->_encodingJobKey = encodingResultSet->getInt64("encodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(encodingResultSet->getInt("encodingPriority"));
                
                if (encodingItem->_encodingParameters == "")
                {
                    string errorMessage = __FILEREF__ + "encodingItem->_encodingParameters is empty"
                            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                            ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                {
                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    bool parsingSuccessful = reader->parse((encodingItem->_encodingParameters).c_str(),
                            (encodingItem->_encodingParameters).c_str() + (encodingItem->_encodingParameters).size(), 
                            &(encodingItem->_parametersRoot), &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", errors: " + errors
                                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                                ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                int64_t workspaceKey;
                {
                    lastSQLCommand = 
                        "select ir.workspaceKey "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey "
                        "and ij.ingestionJobKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementWorkspace (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementWorkspace->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                    shared_ptr<sql::ResultSet> workspaceResultSet (preparedStatementWorkspace->executeQuery());
                    if (workspaceResultSet->next())
                    {
                        encodingItem->_workspace = getWorkspace(workspaceResultSet->getInt64("workspaceKey"));
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                if (encodingItem->_encodingType == EncodingType::EncodeVideoAudio
                        || encodingItem->_encodingType == EncodingType::EncodeImage)
                {
                    encodingItem->_encodeData = make_shared<EncodingItem::EncodeData>();
                            
                    string field = "sourcePhysicalPathKey";
                    int64_t sourcePhysicalPathKey = encodingItem->_parametersRoot.get(field, 0).asInt64();
                                        
                    field = "encodingProfileKey";
                    int64_t encodingProfileKey = encodingItem->_parametersRoot.get(field, 0).asInt64();

                    {
                        lastSQLCommand = 
                            "select m.contentType, p.partitionNumber, p.mediaItemKey, p.fileName, p.relativePath "
                            "from MMS_MediaItem m, MMS_PhysicalPath p where m.mediaItemKey = p.mediaItemKey and p.physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourcePhysicalPathKey);

                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_encodeData->_contentType = MMSEngineDBFacade::toContentType(physicalPathResultSet->getString("contentType"));
                            encodingItem->_encodeData->_mmsPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_encodeData->_mediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                            encodingItem->_encodeData->_fileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_encodeData->_relativePath = physicalPathResultSet->getString("relativePath");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    if (encodingItem->_encodeData->_contentType == ContentType::Video)
                    {
                        lastSQLCommand = 
                            "select durationInMilliSeconds from MMS_VideoItemProfile where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementVideo->setInt64(queryParameterIndex++, sourcePhysicalPathKey);

                        shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                        if (videoResultSet->next())
                        {
                            encodingItem->_encodeData->_durationInMilliSeconds = videoResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    else if (encodingItem->_encodeData->_contentType == ContentType::Audio)
                    {
                        lastSQLCommand = 
                            "select durationInMilliSeconds from MMS_AudioItemProfile where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementAudio (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementAudio->setInt64(queryParameterIndex++, sourcePhysicalPathKey);

                        shared_ptr<sql::ResultSet> audioResultSet (preparedStatementAudio->executeQuery());
                        if (audioResultSet->next())
                        {
                            encodingItem->_encodeData->_durationInMilliSeconds = audioResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select technology, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementEncodingProfile->setInt64(queryParameterIndex++, encodingProfileKey);

                        shared_ptr<sql::ResultSet> encodingProfilesResultSet (preparedStatementEncodingProfile->executeQuery());
                        if (encodingProfilesResultSet->next())
                        {
                            encodingItem->_encodeData->_encodingProfileTechnology = static_cast<EncodingTechnology>(encodingProfilesResultSet->getInt("technology"));
                            encodingItem->_encodeData->_jsonProfile = encodingProfilesResultSet->getString("jsonProfile");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed"
                                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::OverlayImageOnVideo)
                {
                    encodingItem->_overlayImageOnVideoData = make_shared<EncodingItem::OverlayImageOnVideoData>();
                    
                    int64_t sourceVideoPhysicalPathKey;
                    int64_t sourceImagePhysicalPathKey;    

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = encodingItem->_parametersRoot.get(field, 0).asInt64();

                        field = "sourceImagePhysicalPathKey";
                        sourceImagePhysicalPathKey = encodingItem->_parametersRoot.get(field, 0).asInt64();
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_overlayImageOnVideoData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayImageOnVideoData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayImageOnVideoData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
                            
                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select durationInMilliSeconds from MMS_VideoItemProfile where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementVideo->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                        if (videoResultSet->next())
                        {
                            encodingItem->_overlayImageOnVideoData->_videoDurationInMilliSeconds = videoResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select partitionNumber, fileName, relativePath "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceImagePhysicalPathKey);

                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_overlayImageOnVideoData->_mmsImagePartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayImageOnVideoData->_imageFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayImageOnVideoData->_imageRelativePath = physicalPathResultSet->getString("relativePath");                            
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceImagePhysicalPathKey: " + to_string(sourceImagePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string overlayParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(overlayParameters.c_str(),
                                        overlayParameters.c_str() + overlayParameters.size(), 
                                        &(encodingItem->_overlayImageOnVideoData->_overlayParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", overlayParameters: " + overlayParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::OverlayTextOnVideo)
                {
                    encodingItem->_overlayTextOnVideoData = make_shared<EncodingItem::OverlayTextOnVideoData>();
                    
                    int64_t sourceVideoPhysicalPathKey;

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = encodingItem->_parametersRoot.get(field, 0).asInt64();
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_overlayTextOnVideoData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayTextOnVideoData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayTextOnVideoData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
                            
                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select durationInMilliSeconds from MMS_VideoItemProfile where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementVideo->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                        if (videoResultSet->next())
                        {
                            encodingItem->_overlayTextOnVideoData->_videoDurationInMilliSeconds = videoResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string overlayTextParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(overlayTextParameters.c_str(),
                                        overlayTextParameters.c_str() + overlayTextParameters.size(), 
                                        &(encodingItem->_overlayTextOnVideoData->_overlayTextParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", overlayTextParameters: " + overlayTextParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::GenerateFrames)
                {
                    encodingItem->_generateFramesData = make_shared<EncodingItem::GenerateFramesData>();
                    
                    int64_t sourceVideoPhysicalPathKey;

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = encodingItem->_parametersRoot.get(field, 0).asInt64();
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_generateFramesData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_generateFramesData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_generateFramesData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
                            
                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select durationInMilliSeconds from MMS_VideoItemProfile where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementVideo->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

                        shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                        if (videoResultSet->next())
                        {
                            encodingItem->_generateFramesData->_videoDurationInMilliSeconds = videoResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string generateFramesParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(generateFramesParameters.c_str(),
                                        generateFramesParameters.c_str() + generateFramesParameters.size(), 
                                        &(encodingItem->_generateFramesData->_generateFramesParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", generateFramesParameters: " + generateFramesParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::SlideShow)
                {
                    encodingItem->_slideShowData = make_shared<EncodingItem::SlideShowData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string slideShowParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(slideShowParameters.c_str(),
                                        slideShowParameters.c_str() + slideShowParameters.size(), 
                                        &(encodingItem->_slideShowData->_slideShowParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", slideShowParameters: " + slideShowParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::FaceRecognition)
                {
                    encodingItem->_faceRecognitionData = make_shared<EncodingItem::FaceRecognitionData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string faceRecognitionParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(faceRecognitionParameters.c_str(),
                                        faceRecognitionParameters.c_str() + faceRecognitionParameters.size(), 
                                        &(encodingItem->_faceRecognitionData->_faceRecognitionParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", faceRecognitionParameters: " + faceRecognitionParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::FaceIdentification)
                {
                    encodingItem->_faceIdentificationData = make_shared<EncodingItem::FaceIdentificationData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string faceIdentificationParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(faceIdentificationParameters.c_str(),
                                        faceIdentificationParameters.c_str() + faceIdentificationParameters.size(), 
                                        &(encodingItem->_faceIdentificationData->_faceIdentificationParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", faceIdentificationParameters: " + faceIdentificationParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::LiveRecorder)
                {
                    encodingItem->_liveRecorderData = make_shared<EncodingItem::LiveRecorderData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
                        if (imgestionResultSet->next())
                        {
                            string liveRecorderParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(liveRecorderParameters.c_str(),
                                        liveRecorderParameters.c_str() + liveRecorderParameters.size(), 
                                        &(encodingItem->_liveRecorderData->_liveRecorderParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", liveRecorderParameters: " + liveRecorderParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingType is wrong"
                            + ", EncodingType: " + toString(encodingItem->_encodingType)
                    ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                encodingItems.push_back(encodingItem);
                
                {
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ?, encodingJobStart = NULL where encodingJobKey = ? and processorMMS is null";
                    shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                    preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                    int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", processorMMS: " + processorMMS
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetKey(
    int64_t workspaceKey,
    int64_t encodingProfilesSetKey)
{
    vector<int64_t> encodingProfilesSetKeys;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int64_t localWorkspaceKey     = resultSet->getInt64("workspaceKey");
                if (localWorkspaceKey != workspaceKey)
                {
                    string errorMessage = __FILEREF__ + "WorkspaceKey does not match "
                            + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
                            + ", workspaceKey: " + to_string(workspaceKey)
                            + ", localWorkspaceKey: " + to_string(localWorkspaceKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "WorkspaceKey was not found "
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand =
                "select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                encodingProfilesSetKeys.push_back(resultSet->getInt64("encodingProfileKey"));
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }       
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }       
    
    return encodingProfilesSetKeys;
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetLabel(
    int64_t workspaceKey,
    string label)
{
    vector<int64_t> encodingProfilesSetKeys;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t encodingProfilesSetKey;
        {
            lastSQLCommand = 
                "select encodingProfilesSetKey from MMS_EncodingProfilesSet where workspaceKey = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey     = resultSet->getInt64("encodingProfilesSetKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "WorkspaceKey/encodingProfilesSetLabel was not found "
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", label: " + label
                        // + ", lastSQLCommand: " + lastSQLCommand    It will be added in catch
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand =
                "select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                encodingProfilesSetKeys.push_back(resultSet->getInt64("encodingProfileKey"));
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }       
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }       
    
    return encodingProfilesSetKeys;
}

int MMSEngineDBFacade::addEncodingJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    string destEncodingProfileLabel,
    int64_t sourceMediaItemKey,
    int64_t sourcePhysicalPathKey,
    EncodingPriority encodingPriority
)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        string contentType;
        {
            lastSQLCommand = 
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType = static_cast<string>(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "ContentType not found "
                        + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t destEncodingProfileKey;
        {
            lastSQLCommand = 
                "select encodingProfileKey from MMS_EncodingProfile where (workspaceKey = ? or workspaceKey is null) and contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setString(queryParameterIndex++, contentType);
            preparedStatement->setString(queryParameterIndex++, destEncodingProfileLabel);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                destEncodingProfileKey = resultSet->getInt64("encodingProfileKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey not found "
                        + ", workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", contentType: " + contentType
                        + ", destEncodingProfileLabel: " + destEncodingProfileLabel
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        addEncodingJob (
            workspace,
            ingestionJobKey,
            destEncodingProfileKey,
            sourceMediaItemKey,
            sourcePhysicalPathKey,
            encodingPriority);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

int MMSEngineDBFacade::addEncodingJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    int64_t destEncodingProfileKey,
    int64_t sourceMediaItemKey,
    int64_t sourcePhysicalPathKey,
    EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        ContentType contentType;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t localSourcePhysicalPathKey = sourcePhysicalPathKey;
        if (sourcePhysicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localSourcePhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        EncodingType encodingType;
        if (contentType == ContentType::Image)
            encodingType = EncodingType::EncodeImage;
        else
            encodingType = EncodingType::EncodeVideoAudio;
        
        string parameters = string()
                + "{ "
                + "\"encodingProfileKey\": " + to_string(destEncodingProfileKey)
                + ", \"sourcePhysicalPathKey\": " + to_string(localSourcePhysicalPathKey)
                + "} ";        
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {     
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;
            
            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

int MMSEngineDBFacade::addEncoding_OverlayImageOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    int64_t mediaItemKey_1, int64_t physicalPathKey_1,
    int64_t mediaItemKey_2, int64_t physicalPathKey_2,
    string imagePosition_X_InPixel, string imagePosition_Y_InPixel,
    EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        ContentType contentType_1;
        {
            lastSQLCommand =
                "select workspaceKey, contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_1);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType_1 = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey_1: " + to_string(mediaItemKey_1)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        ContentType contentType_2;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_2);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType_2 = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey_2: " + to_string(mediaItemKey_2)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        int64_t localSourcePhysicalPathKey_1 = physicalPathKey_1;
        if (physicalPathKey_1 == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_1);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localSourcePhysicalPathKey_1 = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey_1: " + to_string(mediaItemKey_1)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        int64_t localSourcePhysicalPathKey_2 = physicalPathKey_2;
        if (physicalPathKey_2 == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_2);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                localSourcePhysicalPathKey_2 = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey_2: " + to_string(mediaItemKey_2)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t sourceVideoPhysicalPathKey;
        int64_t sourceImagePhysicalPathKey;
        if (contentType_1 == ContentType::Video && contentType_2 == ContentType::Image)
        {
            sourceVideoPhysicalPathKey = localSourcePhysicalPathKey_1;
            sourceImagePhysicalPathKey = localSourcePhysicalPathKey_2;
        }
        else if (contentType_1 == ContentType::Image && contentType_2 == ContentType::Video)
        {
            sourceVideoPhysicalPathKey = localSourcePhysicalPathKey_2;
            sourceImagePhysicalPathKey = localSourcePhysicalPathKey_1;
        }
        else
        {
            string errorMessage = __FILEREF__ + "addOverlayImageOnVideoJob is not receiving one Video and one Image"
                    + ", contentType_1: " + toString(contentType_1)
                    + ", contentType_2: " + toString(contentType_2)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }
        
        EncodingType encodingType = EncodingType::OverlayImageOnVideo;
        
        string parameters = string()
                + "{ "
                + "\"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)
                + ", \"sourceImagePhysicalPathKey\": " + to_string(sourceImagePhysicalPathKey)
                + ", \"imagePosition_X_InPixel\": \"" + imagePosition_X_InPixel + "\""
                + ", \"imagePosition_Y_InPixel\": \"" + imagePosition_Y_InPixel + "\""
                + "} ";
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

int MMSEngineDBFacade::addEncoding_OverlayTextOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,
        
    int64_t mediaItemKey, int64_t physicalPathKey,
    string text,
    string textPosition_X_InPixel,
    string textPosition_Y_InPixel,
    string fontType,
    int fontSize,
    string fontColor,
    int textPercentageOpacity,
    bool boxEnable,
    string boxColor,
    int boxPercentageOpacity
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        ContentType contentType;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t sourceVideoPhysicalPathKey = physicalPathKey;
        if (physicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                sourceVideoPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                
        EncodingType encodingType = EncodingType::OverlayTextOnVideo;
        
        string parameters = string()
                + "{ "
                + "\"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)

                + ", \"text\": \"" + text + "\""
                + ", \"textPosition_X_InPixel\": \"" + textPosition_X_InPixel + "\""
                + ", \"textPosition_Y_InPixel\": \"" + textPosition_Y_InPixel + "\""
                + ", \"fontType\": \"" + fontType + "\""
                + ", \"fontSize\": " + to_string(fontSize)
                + ", \"fontColor\": \"" + fontColor + "\""
                + ", \"textPercentageOpacity\": " + to_string(textPercentageOpacity)
                + ", \"boxEnable\": " + (boxEnable ? "true" : "false")
                + ", \"boxColor\": \"" + boxColor + "\""
                + ", \"boxPercentageOpacity\": " + to_string(boxPercentageOpacity)

                + "} ";
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
}

int MMSEngineDBFacade::addEncoding_GenerateFramesJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,
        
    string imageDirectory,
    double startTimeInSeconds, int maxFramesNumber, 
    string videoFilter, int periodInSeconds, 
    bool mjpeg, int imageWidth, int imageHeight,
    int64_t sourceVideoPhysicalPathKey,
    int64_t videoDurationInMilliSeconds
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_GenerateFramesJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingPriority: " + toString(encodingPriority)
            + ", imageDirectory: " + imageDirectory
            + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
            + ", maxFramesNumber: " + to_string(maxFramesNumber)
            + ", videoFilter: " + videoFilter
            + ", periodInSeconds: " + to_string(periodInSeconds)
            + ", mjpeg: " + to_string(mjpeg)
            + ", imageWidth: " + to_string(imageWidth)
            + ", imageHeight: " + to_string(imageHeight)
            + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
            + ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
                        
        EncodingType encodingType = EncodingType::GenerateFrames;
        
        string parameters = string()
                + "{ "
                + "\"imageDirectory\": \"" + imageDirectory + "\""

                + ", \"startTimeInSeconds\": " + to_string(startTimeInSeconds)
                + ", \"maxFramesNumber\": " + to_string(maxFramesNumber)
                + ", \"videoFilter\": \"" + videoFilter + "\""
                + ", \"periodInSeconds\": " + to_string(periodInSeconds)
                + ", \"mjpeg\": " + (mjpeg ? "true" : "false")
                + ", \"imageWidth\": " + to_string(imageWidth)
                + ", \"imageHeight\": " + to_string(imageHeight)
                + ", \"ingestionJobKey\": " + to_string(ingestionJobKey)
                + ", \"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)
                + ", \"videoDurationInMilliSeconds\": " + to_string(videoDurationInMilliSeconds)

                + "} ";
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
}

int MMSEngineDBFacade::addEncoding_SlideShowJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    vector<string>& sourcePhysicalPaths,
    double durationOfEachSlideInSeconds,
    int outputFrameRate,
    EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        EncodingType encodingType = EncodingType::SlideShow;
        
        string parameters = string()
                + "{ "
                + "\"durationOfEachSlideInSeconds\": " + to_string(durationOfEachSlideInSeconds)
                + ", \"outputFrameRate\": " + to_string(outputFrameRate)
                + ", \"sourcePhysicalPaths\": [ ";
        bool firstSourcePhysicalPath = true;
        for (string sourcePhysicalPath: sourcePhysicalPaths)
        {
            if (!firstSourcePhysicalPath)
                parameters += ", ";
            parameters += ("\"" + sourcePhysicalPath + "\"");
            
            if (firstSourcePhysicalPath)
                firstSourcePhysicalPath = false;
        }
        parameters += (
                string(" ] ")
                + "} "
                );

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

int MMSEngineDBFacade::addEncoding_FaceRecognitionJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string sourcePhysicalPath,
	string faceRecognitionCascadeName,
	string faceRecognitionOutput,
	EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        EncodingType encodingType = EncodingType::FaceRecognition;
        
        string parameters = string()
                + "{ "
                + "\"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\""
                + ", \"faceRecognitionCascadeName\": \"" + faceRecognitionCascadeName + "\""
                + ", \"faceRecognitionOutput\": \"" + faceRecognitionOutput + "\""
                + "} "
                ;

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

int MMSEngineDBFacade::addEncoding_FaceIdentificationJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string sourcePhysicalPath,
	string faceIdentificationCascadeName,
	string jsonDeepLearnedModelTags,
	EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        EncodingType encodingType = EncodingType::FaceIdentification;
        
        string parameters = string()
                + "{ "
                + "\"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\""
                + ", \"faceIdentificationCascadeName\": \"" + faceIdentificationCascadeName + "\""
                + ", \"deepLearnedModelTags\": " + jsonDeepLearnedModelTags + ""
                + "} "
                ;

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

int MMSEngineDBFacade::addEncoding_LiveRecorderJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	bool highAvailability,
	bool main,
	string liveURL,
	time_t utcRecordingPeriodStart,
	time_t utcRecordingPeriodEnd,
	bool autoRenew,
	int segmentDurationInSeconds,
	string outputFileFormat,
	EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveRecorderJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", highAvailability: " + to_string(highAvailability)
            + ", main: " + to_string(main)
            + ", liveURL: " + liveURL
            + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
            + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
            + ", autoRenew: " + to_string(autoRenew)
            + ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
            + ", outputFileFormat: " + outputFileFormat
            + ", encodingPriority: " + toString(encodingPriority)
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        EncodingType encodingType = EncodingType::LiveRecorder;
        
        string parameters = string()
                + "{ "
                + "\"highAvailability\": " + to_string(highAvailability) + ""
                + ", \"main\": " + to_string(main) + ""
                + ", \"liveURL\": \"" + liveURL + "\""
                + ", \"utcRecordingPeriodStart\": " + to_string(utcRecordingPeriodStart) + ""
                + ", \"utcRecordingPeriodEnd\": " + to_string(utcRecordingPeriodEnd) + ""
                + ", \"autoRenew\": " + to_string(autoRenew) + ""
                + ", \"segmentDurationInSeconds\": " + to_string(segmentDurationInSeconds) + ""
                + ", \"outputFileFormat\": \"" + outputFileFormat + "\""
                + "} "
                ;

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
		if (main)
        {
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }        
}

void MMSEngineDBFacade::updateEncodingLiveRecordingPeriod (
		int64_t encodingJobKey,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_EncodingJob set "
				"parameters = JSON_SET(parameters, '$.utcRecordingPeriodStart', ?)"
				", parameters = JSON_SET(parameters, '$.utcRecordingPeriodEnd', ?) "
				"where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodStart);
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodEnd);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
                        + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
}

int MMSEngineDBFacade::updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t mediaItemKey,
        int64_t encodedPhysicalPathKey,
        int64_t ingestionJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    int encodingFailureNumber = -1;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        EncodingStatus newEncodingStatus;
        if (encodingError == EncodingError::PunctualError)
        {
            {
                lastSQLCommand = 
                    "select failuresNumber from MMS_EncodingJob where encodingJobKey = ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingFailureNumber = resultSet->getInt(1);
                }
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingJob not found"
                            + ", EncodingJobKey: " + to_string(encodingJobKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
			string transcoderUpdate;
            if (encodingFailureNumber + 1 >= _maxEncodingFailures)
			{
                newEncodingStatus          = EncodingStatus::End_Failed;

				_logger->info(__FILEREF__ + "update EncodingJob"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
            else
            {
                newEncodingStatus          = EncodingStatus::ToBeProcessed;
                encodingFailureNumber++;

				transcoderUpdate = ", transcoder = NULL";
            }

            {
                lastSQLCommand = 
                    string("update MMS_EncodingJob set status = ?, processorMMS = NULL") + transcoderUpdate + ", failuresNumber = ?, encodingProgress = NULL where encodingJobKey = ? and status = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
                preparedStatement->setInt(queryParameterIndex++, encodingFailureNumber);
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        else if (encodingError == EncodingError::MaxCapacityReached || encodingError == EncodingError::ErrorBeforeEncoding)
        {
            newEncodingStatus       = EncodingStatus::ToBeProcessed;
            
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, transcoder = NULL, encodingProgress = NULL where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        else if (encodingError == EncodingError::KilledByUser)
        {
            newEncodingStatus       = EncodingStatus::End_KilledByUser;
            
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW() "
				"where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        else    // success
        {
            newEncodingStatus       = EncodingStatus::End_Success;

            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW(), encodingProgress = 100 "
                "where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        
        if (newEncodingStatus == EncodingStatus::End_Success)
        {
            // In the Generate-Frames scenario we will have mediaItemKey and encodedPhysicalPathKey set to -1.
            // In this case we do not have to update the IngestionJob because this is done when all the images (the files generated)
            // will be ingested
            if (mediaItemKey != -1 && encodedPhysicalPathKey != -1 && ingestionJobKey != -1)
            {
                IngestionStatus newIngestionStatus = IngestionStatus::End_TaskSuccess;

                string errorMessage;
                string processorMMS;
                _logger->info(__FILEREF__ + "Update IngestionJob"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", IngestionStatus: " + toString(newIngestionStatus)
                    + ", errorMessage: " + errorMessage
                    + ", processorMMS: " + processorMMS
                );                            
                updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
            
                lastSQLCommand = 
                    "insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setInt64(queryParameterIndex++, encodedPhysicalPathKey);

                preparedStatement->executeUpdate();
            }
        }
        else if (newEncodingStatus == EncodingStatus::End_Failed && ingestionJobKey != -1)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_IngestionFailure;
            string errorMessage;
            string processorMMS;
            int64_t physicalPathKey = -1;

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(ingestionStatus)
                + ", physicalPathKey: " + to_string(physicalPathKey)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
        }
        else if (newEncodingStatus == EncodingStatus::End_KilledByUser && ingestionJobKey != -1)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_DwlUplOrEncCancelledByUser;
            string errorMessage;
            string processorMMS;
            int64_t physicalPathKey = -1;

            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(ingestionStatus)
                + ", physicalPathKey: " + to_string(physicalPathKey)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    
    return encodingFailureNumber;
}

void MMSEngineDBFacade::updateEncodingJobPriority (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey,
    EncodingPriority newEncodingPriority)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        EncodingStatus currentEncodingStatus;
        EncodingPriority currentEncodingPriority;
        {
            lastSQLCommand = 
                "select status, encodingPriority from MMS_EncodingJob where encodingJobKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                currentEncodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSet->getString("status"));
                currentEncodingPriority = static_cast<EncodingPriority>(resultSet->getInt("encodingPriority"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        if (currentEncodingStatus != EncodingStatus::ToBeProcessed)
        {
            string errorMessage = __FILEREF__ + "EncodingJob cannot change EncodingPriority because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        if (currentEncodingPriority == newEncodingPriority)
        {
            string errorMessage = __FILEREF__ + "EncodingJob has already the same status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        if (static_cast<int>(currentEncodingPriority) > workspace->_maxEncodingPriority)
        {
            string errorMessage = __FILEREF__ + "EncodingJob cannot be changed to an higher priority"
                    + ", currentEncodingPriority: " + toString(currentEncodingPriority)
                    + ", maxEncodingPriority: " + toString(static_cast<EncodingPriority>(workspace->_maxEncodingPriority))
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        {
            lastSQLCommand = 
                "update MMS_EncodingJob set encodingPriority = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newEncodingPriority));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingPriority: " + toString(newEncodingPriority)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", newEncodingPriority: " + toString(newEncodingPriority)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }    
}

void MMSEngineDBFacade::updateEncodingJobTryAgain (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        EncodingStatus currentEncodingStatus;
        {
            lastSQLCommand = 
                "select status from MMS_EncodingJob where encodingJobKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                currentEncodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSet->getString("status"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        if (currentEncodingStatus != EncodingStatus::End_Failed)
        {
            string errorMessage = __FILEREF__ + "EncodingJob cannot be encoded again because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        EncodingStatus newEncodingStatus = EncodingStatus::ToBeProcessed;
        {            
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingStatus: " + toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", newEncodingStatus: " + toString(newEncodingStatus)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }    
}

void MMSEngineDBFacade::updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_EncodingJob set encodingProgress = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, encodingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingPercentage: " + to_string(encodingPercentage)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

void MMSEngineDBFacade::updateEncodingJobTranscoder (
        int64_t encodingJobKey,
        string transcoder)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_EncodingJob set transcoder = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, transcoder);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", transcoder: " + transcoder
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

string MMSEngineDBFacade::getEncodingJobDetails (
        int64_t encodingJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		string      transcoder;
        {
            lastSQLCommand = 
                "select transcoder from MMS_EncodingJob where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                transcoder = resultSet->getString("transcoder");
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return transcoder;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

void MMSEngineDBFacade::checkWorkspaceMaxIngestionNumber (
    int64_t workspaceKey
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        int maxIngestionsNumber;
        int currentIngestionsNumber;
        EncodingPeriod encodingPeriod;
        string periodStartDateTime;
        string periodEndDateTime;

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select c.maxIngestionsNumber, cmi.currentIngestionsNumber, c.encodingPeriod, "
                    "DATE_FORMAT(cmi.startDateTime, '%Y-%m-%d %H:%i:%s') as LocalStartDateTime, "
                    "DATE_FORMAT(cmi.endDateTime, '%Y-%m-%d %H:%i:%s') as LocalEndDateTime "
                "from MMS_Workspace c, MMS_WorkspaceMoreInfo cmi where c.workspaceKey = cmi.workspaceKey and c.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                maxIngestionsNumber = resultSet->getInt("maxIngestionsNumber");
                currentIngestionsNumber = resultSet->getInt("currentIngestionsNumber");
                encodingPeriod = toEncodingPeriod(resultSet->getString("encodingPeriod"));
                periodStartDateTime = resultSet->getString("LocalStartDateTime");
                periodEndDateTime = resultSet->getString("LocalEndDateTime");                
            }
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        // check maxStorage first        
        {
            int64_t workSpaceUsageInBytes;
            int64_t maxStorageInMB;

            pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB = getWorkspaceUsage(conn, workspaceKey);
            tie(workSpaceUsageInBytes, maxStorageInMB) = workSpaceUsageInBytesAndMaxStorageInMB;
            
            int64_t totalSizeInMB = workSpaceUsageInBytes / 1000000;
            if (totalSizeInMB >= maxStorageInMB)
            {
                string errorMessage = __FILEREF__ + "Reached the max storage dedicated for your Workspace"
                    + ", maxStorageInMB: " + to_string(maxStorageInMB)
                    + ". It is needed to increase Workspace capacity."
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        bool ingestionsAllowed = true;
        bool periodExpired = false;
        char newPeriodStartDateTime [64];
        char newPeriodEndDateTime [64];

        {
            char                strDateTimeNow [64];
            tm                  tmDateTimeNow;
            chrono::system_clock::time_point now = chrono::system_clock::now();
            time_t utcTimeNow = chrono::system_clock::to_time_t(now);
            localtime_r (&utcTimeNow, &tmDateTimeNow);

            sprintf (strDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d",
                tmDateTimeNow. tm_year + 1900,
                tmDateTimeNow. tm_mon + 1,
                tmDateTimeNow. tm_mday,
                tmDateTimeNow. tm_hour,
                tmDateTimeNow. tm_min,
                tmDateTimeNow. tm_sec);

            if (periodStartDateTime.compare(strDateTimeNow) <= 0 && periodEndDateTime.compare(strDateTimeNow) >= 0)
            {
                // Period not expired

                // periodExpired = false; already initialized
                
                if (currentIngestionsNumber >= maxIngestionsNumber)
                {
                    // no more ingestions are allowed for this workspace

                    ingestionsAllowed = false;
                }
            }
            else
            {
                // Period expired

                periodExpired = true;
                
                if (encodingPeriod == EncodingPeriod::Daily)
                {
                    sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            0,  // tmDateTimeNow. tm_hour,
                            0,  // tmDateTimeNow. tm_min,
                            0  // tmDateTimeNow. tm_sec
                    );
                    sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            23,  // tmCurrentDateTime. tm_hour,
                            59,  // tmCurrentDateTime. tm_min,
                            59  // tmCurrentDateTime. tm_sec
                    );
                }
                else if (encodingPeriod == EncodingPeriod::Weekly)
                {
                    // from monday to sunday
                    // monday
                    {
                        int daysToHavePreviousMonday;

                        if (tmDateTimeNow.tm_wday == 0)  // Sunday
                            daysToHavePreviousMonday = 6;
                        else
                            daysToHavePreviousMonday = tmDateTimeNow.tm_wday - 1;

                        chrono::system_clock::time_point mondayOfCurrentWeek;
                        if (daysToHavePreviousMonday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHavePreviousMonday);
                            mondayOfCurrentWeek = now - days;
                        }
                        else
                            mondayOfCurrentWeek = now;

                        tm                  tmMondayOfCurrentWeek;
                        time_t utcTimeMondayOfCurrentWeek = chrono::system_clock::to_time_t(mondayOfCurrentWeek);
                        localtime_r (&utcTimeMondayOfCurrentWeek, &tmMondayOfCurrentWeek);

                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmMondayOfCurrentWeek. tm_year + 1900,
                                tmMondayOfCurrentWeek. tm_mon + 1,
                                tmMondayOfCurrentWeek. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // sunday
                    {
                        int daysToHaveNextSunday;

                        daysToHaveNextSunday = 7 - tmDateTimeNow.tm_wday;

                        chrono::system_clock::time_point sundayOfCurrentWeek;
                        if (daysToHaveNextSunday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHaveNextSunday);
                            sundayOfCurrentWeek = now + days;
                        }
                        else
                            sundayOfCurrentWeek = now;

                        tm                  tmSundayOfCurrentWeek;
                        time_t utcTimeSundayOfCurrentWeek = chrono::system_clock::to_time_t(sundayOfCurrentWeek);
                        localtime_r (&utcTimeSundayOfCurrentWeek, &tmSundayOfCurrentWeek);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmSundayOfCurrentWeek. tm_year + 1900,
                                tmSundayOfCurrentWeek. tm_mon + 1,
                                tmSundayOfCurrentWeek. tm_mday,
                                23,  // tmSundayOfCurrentWeek. tm_hour,
                                59,  // tmSundayOfCurrentWeek. tm_min,
                                59  // tmSundayOfCurrentWeek. tm_sec
                        );
                    }
                }
                else if (encodingPeriod == EncodingPeriod::Monthly)
                {
                    // first day of the month
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        tm                  tmLastDayOfCurrentMonth = tmDateTimeNow;

                        tmLastDayOfCurrentMonth.tm_mday = 1;

                        // Next month 0=Jan
                        if (tmLastDayOfCurrentMonth.tm_mon == 11)    // Dec
                        {
                            tmLastDayOfCurrentMonth.tm_mon = 0;
                            tmLastDayOfCurrentMonth.tm_year++;
                        }
                        else
                        {
                            tmLastDayOfCurrentMonth.tm_mon++;
                        }

                        // Get the first day of the next month
                        time_t utcTimeLastDayOfCurrentMonth = mktime (&tmLastDayOfCurrentMonth);

                        // Subtract 1 day
                        utcTimeLastDayOfCurrentMonth -= 86400;

                        // Convert back to date and time
                        localtime_r (&utcTimeLastDayOfCurrentMonth, &tmLastDayOfCurrentMonth);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmLastDayOfCurrentMonth. tm_year + 1900,
                                tmLastDayOfCurrentMonth. tm_mon + 1,
                                tmLastDayOfCurrentMonth. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
                else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
                {
                    // first day of the year
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                1,  // tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                12, // tmDateTimeNow. tm_mon + 1,
                                31, // tmDateTimeNow. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
            }
        }
        
        if (periodExpired)
        {
            lastSQLCommand = 
                "update MMS_WorkspaceMoreInfo set currentIngestionsNumber = 0, "
                "startDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), endDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S') "
                "where workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, newPeriodStartDateTime);
            preparedStatement->setString(queryParameterIndex++, newPeriodEndDateTime);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newPeriodStartDateTime: " + newPeriodStartDateTime
                        + ", newPeriodEndDateTime: " + newPeriodEndDateTime
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        if (!ingestionsAllowed)
        {
            string errorMessage = __FILEREF__ + "Reached the max number of Ingestions in your period"
                + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
                + ", encodingPeriod: " + toString(encodingPeriod)
                    + ". It is needed to increase Workspace capacity."
            ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
                
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "exception"
            + ", e.what: " + e.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

pair<int64_t,int64_t> MMSEngineDBFacade::getWorkspaceUsage(
        shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey)
{
    int64_t         totalSizeInBytes;
    int64_t         maxStorageInMB;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "select SUM(pp.sizeInBytes) as totalSizeInBytes from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                if (resultSet->isNull("totalSizeInBytes"))
                    totalSizeInBytes = -1;
                else
                    totalSizeInBytes = resultSet->getInt64("totalSizeInBytes");
            }
        }
        
        {
            lastSQLCommand = 
                "select maxStorageInMB from MMS_Workspace where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                maxStorageInMB = resultSet->getInt("maxStorageInMB");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        return make_pair(totalSizeInBytes, maxStorageInMB);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

string MMSEngineDBFacade::nextRelativePathToBeUsed (
    int64_t workspaceKey
)
{
    string      relativePathToBeUsed;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select currentDirLevel1, currentDirLevel2, currentDirLevel3 from MMS_WorkspaceMoreInfo where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                currentDirLevel1 = resultSet->getInt("currentDirLevel1");
                currentDirLevel2 = resultSet->getInt("currentDirLevel2");
                currentDirLevel3 = resultSet->getInt("currentDirLevel3");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        {
            char pCurrentRelativePath [64];
            
            sprintf (pCurrentRelativePath, "/%03d/%03d/%03d/", 
                currentDirLevel1, currentDirLevel2, currentDirLevel3);
            
            relativePathToBeUsed = pCurrentRelativePath;
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    
    return relativePathToBeUsed;
}

pair<int64_t,int64_t> MMSEngineDBFacade::saveIngestedContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,        
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,

        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
        )
{
    pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        _logger->info(__FILEREF__ + "Retrieving contentProviderKey");
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
            if (isMetadataPresent(parametersRoot, "ContentProviderName"))
                contentProviderName = parametersRoot.get("ContentProviderName", "").asString();
            else
                contentProviderName = _defaultContentProviderName;

            lastSQLCommand = 
                "select contentProviderKey from MMS_ContentProvider where workspaceKey = ? and name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setString(queryParameterIndex++, contentProviderName);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentProviderKey = resultSet->getInt64("contentProviderKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "ContentProvider is not present"
                    + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                    + ", contentProviderName: " + contentProviderName
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->info(__FILEREF__ + "Insert into MMS_MediaItem");
        // int64_t encodingProfileSetKey;
        {
            string title = "";
            string ingester = "";
            string userData = "";
            string deliveryFileName = "";
            string sContentType;
            int retentionInMinutes = _contentRetentionInMinutesDefaultValue;
            // string encodingProfilesSet;

            string field = "Title";
            title = parametersRoot.get(field, "").asString();
            
            field = "Ingester";
            if (isMetadataPresent(parametersRoot, field))
                ingester = parametersRoot.get(field, "").asString();

            field = "UserData";
            if (isMetadataPresent(parametersRoot, field))
            {
                Json::StreamWriterBuilder wbuilder;

                userData = Json::writeString(wbuilder, parametersRoot[field]);                        
            }

            field = "DeliveryFileName";
            if (isMetadataPresent(parametersRoot, field))
                deliveryFileName = parametersRoot.get(field, "").asString();

            field = "Retention";
            if (isMetadataPresent(parametersRoot, field))
            {
                string retention = parametersRoot.get(field, "1d").asString();
                if (retention == "0")
                    retentionInMinutes = 0;
                else if (retention.length() > 1)
                {
                    switch (retention.back())
                    {
                        case 's':   // seconds
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1)) / 60;
                            
                            break;
                        case 'm':   // minutes
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1));
                            
                            break;
                        case 'h':   // hours
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1)) * 60;
                            
                            break;
                        case 'd':   // days
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1)) * 1440;
                            
                            break;
                        case 'M':   // month
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1)) * (1440 * 30);
                            
                            break;
                        case 'y':   // year
                            retentionInMinutes = stol(retention.substr(0, retention.length() - 1)) * (1440 * 365);
                            
                            break;
                    }
                }
            }

            string startPublishing = "NOW";
            string endPublishing = "FOREVER";
            {
                field = "Publishing";
                if (isMetadataPresent(parametersRoot, field))
                {
                    Json::Value publishingRoot = parametersRoot[field];

                    field = "startPublishing";
                    if (isMetadataPresent(publishingRoot, field))
                        startPublishing = publishingRoot.get(field, "").asString();

                    field = "endPublishing";
                    if (isMetadataPresent(publishingRoot, field))
                        endPublishing = publishingRoot.get(field, "").asString();
                }
                
                if (startPublishing == "NOW")
                {
                    tm          tmDateTime;
                    char        strDateTime [64];

                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    time_t utcTime = chrono::system_clock::to_time_t(now);

                	gmtime_r (&utcTime, &tmDateTime);

                    sprintf (strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);

                    startPublishing = strDateTime;
                }

                if (endPublishing == "FOREVER")
                {
                    tm          tmDateTime;
                    char        strDateTime [64];

                    chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 10);

                    time_t utcTime = chrono::system_clock::to_time_t(forever);

                	gmtime_r (&utcTime, &tmDateTime);

                    sprintf (strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);

                    endPublishing = strDateTime;
                }
            }
            
            lastSQLCommand = 
                "insert into MMS_MediaItem (mediaItemKey, workspaceKey, contentProviderKey, title, ingester, userData, " 
                "deliveryFileName, ingestionJobKey, ingestionDate, contentType, startPublishing, endPublishing, retentionInMinutes, processorMMSForRetention) values ("
                "NULL, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, "
                "convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone), "
                "convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone), "
                "?, NULL)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, contentProviderKey);
            preparedStatement->setString(queryParameterIndex++, title);
            if (ingester == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, ingester);
            if (userData == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, userData);
            if (deliveryFileName == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryFileName);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setString(queryParameterIndex++, startPublishing);
            preparedStatement->setString(queryParameterIndex++, endPublishing);
            preparedStatement->setInt(queryParameterIndex++, retentionInMinutes);

            preparedStatement->executeUpdate();
        }
        
        int64_t mediaItemKey = getLastInsertId(conn);

	// tags
        {
		string field = "Tags";
		if (isMetadataPresent(parametersRoot, field))
		{
			for (int tagIndex = 0; tagIndex < parametersRoot[field].size(); tagIndex++)
			{
				string tag = parametersRoot[field][tagIndex].asString();

               			lastSQLCommand = 
                    			"insert into MMS_Tag (mediaItemKey, name) values ("
                    			"?, ?)";

               			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
               			int queryParameterIndex = 1;
               			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
               			preparedStatement->setString(queryParameterIndex++, tag);

               			preparedStatement->executeUpdate();
			}
		}
        }

        {
            string uniqueName;
            if (isMetadataPresent(parametersRoot, "UniqueName"))
                uniqueName = parametersRoot.get("UniqueName", "").asString();

            if (uniqueName != "")
            {
                lastSQLCommand = 
                    "insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setString(queryParameterIndex++, uniqueName);

                preparedStatement->executeUpdate();
            }
        }
        
        /*
        // territories
        {
            string field = "Territories";
            if (isMetadataPresent(parametersRoot, field))
            {
                const Json::Value territories = parametersRoot[field];
                
                lastSQLCommand = 
                    "select territoryKey, name from MMS_Territory where workspaceKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatementTerrirories (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatementTerrirories->setInt64(queryParameterIndex++, workspace->_workspaceKey);

                shared_ptr<sql::ResultSet> resultSetTerritories (preparedStatementTerrirories->executeQuery());
                while (resultSetTerritories->next())
                {
                    int64_t territoryKey = resultSetTerritories->getInt64("territoryKey");
                    string territoryName = resultSetTerritories->getString("name");

                    string startPublishing = "NOW";
                    string endPublishing = "FOREVER";
                    if (isMetadataPresent(territories, territoryName))
                    {
                        Json::Value territory = territories[territoryName];
                        
                        field = "startPublishing";
                        if (isMetadataPresent(territory, field))
                            startPublishing = territory.get(field, "XXX").asString();

                        field = "endPublishing";
                        if (isMetadataPresent(territory, field))
                            endPublishing = territory.get(field, "XXX").asString();
                    }
                    
                    if (startPublishing == "NOW")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point now = chrono::system_clock::now();
                        time_t utcTime = chrono::system_clock::to_time_t(now);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        startPublishing = strDateTime;
                    }
                    
                    if (endPublishing == "FOREVER")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 20);
                        
                        time_t utcTime = chrono::system_clock::to_time_t(forever);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        endPublishing = strDateTime;
                    }
                    
                    {
                        lastSQLCommand = 
                            "insert into MMS_DefaultTerritoryInfo(defaultTerritoryInfoKey, mediaItemKey, territoryKey, startPublishing, endPublishing) values ("
                            "NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";

                        shared_ptr<sql::PreparedStatement> preparedStatementDefaultTerritory (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementDefaultTerritory->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatementDefaultTerritory->setInt(queryParameterIndex++, territoryKey);
                        preparedStatementDefaultTerritory->setString(queryParameterIndex++, startPublishing);
                        preparedStatementDefaultTerritory->setString(queryParameterIndex++, endPublishing);

                        preparedStatementDefaultTerritory->executeUpdate();
                    }
                }                
            }
        }
        */

        int64_t encodingProfileKey = -1;
        int64_t physicalPathKey = saveEncodedContentMetadata(
            conn,
                
            workspace->_workspaceKey,
            mediaItemKey,
            mediaSourceFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,

            // video-audio
            durationInMilliSeconds,
            bitRate,
            videoCodecName,
            videoProfile,
            videoWidth,
            videoHeight,
            videoAvgFrameRate,
            videoBitRate,
            audioCodecName,
            audioSampleRate,
            audioChannels,
            audioBitRate,

            // image
            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
        );

        {
            int currentDirLevel1;
            int currentDirLevel2;
            int currentDirLevel3;

            {
                lastSQLCommand = 
                    "select currentDirLevel1, currentDirLevel2, currentDirLevel3 "
                    "from MMS_WorkspaceMoreInfo where workspaceKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    currentDirLevel1 = resultSet->getInt("currentDirLevel1");
                    currentDirLevel2 = resultSet->getInt("currentDirLevel2");
                    currentDirLevel3 = resultSet->getInt("currentDirLevel3");
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }

            if (currentDirLevel3 >= 999)
            {
                currentDirLevel3		= 0;

                if (currentDirLevel2 >= 999)
                {
                    currentDirLevel2		= 0;

                    if (currentDirLevel1 >= 999)
                    {
                        currentDirLevel1		= 0;
                    }
                    else
                    {
                        currentDirLevel1++;
                    }
                }
                else
                {
                    currentDirLevel2++;
                }
            }
            else
            {
                currentDirLevel3++;
            }

            {
                lastSQLCommand = 
                    "update MMS_WorkspaceMoreInfo set currentDirLevel1 = ?, currentDirLevel2 = ?, "
                    "currentDirLevel3 = ?, currentIngestionsNumber = currentIngestionsNumber + 1 "
                    "where workspaceKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel1);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel2);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel3);
                preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", currentDirLevel1: " + to_string(currentDirLevel1)
                            + ", currentDirLevel2: " + to_string(currentDirLevel2)
                            + ", currentDirLevel3: " + to_string(currentDirLevel3)
                            + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
        }
        
        {
            {
                lastSQLCommand = 
                    "insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

                int rowsUpdated = preparedStatement->executeUpdate();

                _logger->info(__FILEREF__ + "insert into MMS_IngestionJobOutput"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", physicalPathKey: " + to_string(physicalPathKey)
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );                            
            }

            if (ingestionRowToBeUpdatedAsSuccess)
            {
                // we can have two scenarios:
                //  1. this ingestion will generate just one output file (most of the cases)
                //      in this case we will 
                //          update the ingestionJobKey with the status
                //          will add the row in MMS_IngestionJobOutput
                //          will call manageIngestionJobStatusUpdate
                //  2. this ingestion will generate multiple files (i.e. Periodical-Frames task)
                IngestionStatus newIngestionStatus = IngestionStatus::End_TaskSuccess;
            
                string errorMessage;
                string processorMMS;
                _logger->info(__FILEREF__ + "Update IngestionJob"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", IngestionStatus: " + toString(newIngestionStatus)
                    + ", errorMessage: " + errorMessage
                    + ", processorMMS: " + processorMMS
                );                            
                updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
        
        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    
    return mediaItemKeyAndPhysicalPathKey;
}

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
)
{
    int64_t     physicalPathKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        physicalPathKey = saveEncodedContentMetadata(
            conn,

            workspaceKey,
            mediaItemKey,
            encodedFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,

            // video-audio
            durationInMilliSeconds,
            bitRate,
            videoCodecName,
            videoProfile,
            videoWidth,
            videoHeight,
            videoAvgFrameRate,
            videoBitRate,
            audioCodecName,
            audioSampleRate,
            audioChannels,
            audioBitRate,

            // image
            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
        );

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }
    
    return physicalPathKey;
}

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        shared_ptr<MySQLConnection> conn,
        
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
        int64_t durationInMilliSeconds,
        long bitRate,
        string videoCodecName,
        string videoProfile,
        int videoWidth,
        int videoHeight,
        string videoAvgFrameRate,
        long videoBitRate,
        string audioCodecName,
        long audioSampleRate,
        int audioChannels,
        long audioBitRate,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
)
{
    int64_t     physicalPathKey;
    string      lastSQLCommand;
    
    try
    {
        MMSEngineDBFacade::ContentType contentType;
        {
            lastSQLCommand = 
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "no ContentType returned"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        {
            int drm = 0;

            lastSQLCommand = 
                "insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, creationDate) values ("
                "NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, encodedFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, mmsPartitionIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
            if (encodingProfileKey == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
            else
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            preparedStatement->executeUpdate();
        }

        physicalPathKey = getLastInsertId(conn);

        {
            if (contentType == ContentType::Video)
            {
                lastSQLCommand = 
                    "insert into MMS_VideoItemProfile (physicalPathKey, durationInMilliSeconds, bitRate, width, height, avgFrameRate, "
                    "videoCodecName, videoProfile, videoBitRate, "
                    "audioCodecName, audioSampleRate, audioChannels, audioBitRate) values ("
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
                if (durationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, durationInMilliSeconds);
                if (bitRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, bitRate);
                if (videoWidth == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, videoWidth);
                if (videoHeight == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, videoHeight);
                if (videoAvgFrameRate == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, videoAvgFrameRate);
                if (videoCodecName == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, videoCodecName);
                if (videoProfile == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, videoProfile);
                if (videoBitRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, videoBitRate);
                if (audioCodecName == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, audioCodecName);
                if (audioSampleRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioSampleRate);
                if (audioChannels == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioChannels);
                if (audioBitRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioBitRate);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Audio)
            {
                lastSQLCommand = 
                    "insert into MMS_AudioItemProfile (physicalPathKey, durationInMilliSeconds, codecName, bitRate, sampleRate, channels) values ("
                    "?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
                if (durationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, durationInMilliSeconds);
                if (audioCodecName == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, audioCodecName);
                if (audioBitRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioBitRate);
                if (audioSampleRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioSampleRate);
                if (audioChannels == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioChannels);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Image)
            {
                lastSQLCommand = 
                    "insert into MMS_ImageItemProfile (physicalPathKey, width, height, format, quality) values ("
                    "?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
                preparedStatement->setInt64(queryParameterIndex++, imageWidth);
                preparedStatement->setInt64(queryParameterIndex++, imageHeight);
                preparedStatement->setString(queryParameterIndex++, imageFormat);
                preparedStatement->setInt(queryParameterIndex++, imageQuality);

                preparedStatement->executeUpdate();
            }
            else
            {
                string errorMessage = __FILEREF__ + "ContentType is wrong"
                    + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }        
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );
        
        throw e;
    }
    
    return physicalPathKey;
}

void MMSEngineDBFacade::removePhysicalPath (
        int64_t physicalPathKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "delete from MMS_PhysicalPath where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

void MMSEngineDBFacade::removeMediaItem (
        int64_t mediaItemKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "delete from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
}

tuple<int,shared_ptr<Workspace>,string,string,string,string,int64_t> MMSEngineDBFacade::getStorageDetails(
        int64_t physicalPathKey
)
{
        
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t workspaceKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
        string deliveryFileName;
        string title;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.title, mi.deliveryFileName, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and pp.physicalPathKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getInt64("sizeInBytes");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey is not present"
                    + ", physicalPathKey: " + to_string(physicalPathKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> MMSEngineDBFacade::getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey
)
{
        
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t workspaceKey;
        int64_t physicalPathKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
        string deliveryFileName;
        string title;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.title, mi.deliveryFileName, pp.physicalPathKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? "
                "and pp.encodingProfileKey " + (encodingProfileKey == -1 ? "is null" : "= ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            if (encodingProfileKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
                physicalPathKey = resultSet->getInt64("physicalPathKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getInt64("sizeInBytes");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/EncodingProfileKey are not present"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

void MMSEngineDBFacade::getAllStorageDetails(
        int64_t mediaItemKey, vector<tuple<int,string,string,string>>& allStorageDetails
)
{
        
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t workspaceKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, pp.partitionNumber, pp.relativePath, pp.fileName "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");

                shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);
        
                tuple<int,string,string,string> storageDetails =
                    make_tuple(mmsPartitionNumber, workspace->_directoryName, relativePath, fileName);
                
                allStorageDetails.push_back(storageDetails);
            }
        }


        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

int64_t MMSEngineDBFacade::createDeliveryAuthorization(
    int64_t userKey,
    string clientIPAddress,
    int64_t physicalPathKey,
    string deliveryURI,
    int ttlInSeconds,
    int maxRetries)
{
    int64_t     deliveryAuthorizationKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, physicalPathKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
                "NULL, ?, ?, ?, ?, ?, 0, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            if (clientIPAddress == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, clientIPAddress);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            preparedStatement->setString(queryParameterIndex++, deliveryURI);
            preparedStatement->setInt(queryParameterIndex++, ttlInSeconds);
            preparedStatement->setInt(queryParameterIndex++, maxRetries);

            preparedStatement->executeUpdate();
        }
                    
        deliveryAuthorizationKey = getLastInsertId(conn);
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        

    return deliveryAuthorizationKey;
}

bool MMSEngineDBFacade::checkDeliveryAuthorization(
        int64_t deliveryAuthorizationKey,
        string contentURI)
{
    bool        authorizationOK = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
    
        {
            lastSQLCommand =
                "select deliveryURI, currentRetriesNumber, maxRetries, "
                "(DATE_ADD(authorizationTimestamp, INTERVAL ttlInSeconds SECOND) - NOW()) as timeToLiveAvailable "
                "from MMS_DeliveryAuthorization "
                "where deliveryAuthorizationKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, deliveryAuthorizationKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                string deliveryURI = resultSet->getString("deliveryURI");
                int currentRetriesNumber = resultSet->getInt("currentRetriesNumber");
                int maxRetries = resultSet->getInt("maxRetries");
                int timeToLiveAvailable = resultSet->getInt("timeToLiveAvailable");
                
                if (contentURI != deliveryURI)
                {
                    string errorMessage = __FILEREF__ + "contentURI and deliveryURI are different"
                        + ", contentURI: " + contentURI
                        + ", deliveryURI: " + deliveryURI
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
                else if (currentRetriesNumber >= maxRetries)
                {
                    string errorMessage = __FILEREF__ + "maxRetries is already reached"
                        + ", currentRetriesNumber: " + to_string(currentRetriesNumber)
                        + ", maxRetries: " + to_string(maxRetries)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
                else if (timeToLiveAvailable < 0)
                {
                    string errorMessage = __FILEREF__ + "TTL expired"
                        + ", timeToLiveAvailable: " + to_string(timeToLiveAvailable)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "deliveryAuthorizationKey not found"
                    + ", deliveryAuthorizationKey: " + to_string(deliveryAuthorizationKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            
            authorizationOK = true;
        }
        
        if (authorizationOK)
        {
            lastSQLCommand = 
                "update MMS_DeliveryAuthorization set currentRetriesNumber = currentRetriesNumber + 1 "
                "where deliveryAuthorizationKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, deliveryAuthorizationKey);

            preparedStatement->executeUpdate();
        }
                    
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        // throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        

    return authorizationOK;
}

int64_t MMSEngineDBFacade::addYouTubeConf(
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_YouTube(workspaceKey, label, refreshToken) values ("
                "?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, refreshToken);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyYouTubeConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "update MMS_Conf_YouTube set label = ?, refreshToken = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, refreshToken);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeYouTubeConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Conf_YouTube where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getYouTubeConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value youTubeConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getYouTubeConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;
            }
            
            field = "requestParameters";
            youTubeConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_YouTube ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value youTubeRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, refreshToken from MMS_Conf_YouTube ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value youTubeConfRoot;

                field = "confKey";
                youTubeConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                youTubeConfRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "refreshToken";
                youTubeConfRoot[field] = static_cast<string>(resultSet->getString("refreshToken"));

                youTubeRoot.append(youTubeConfRoot);
            }
        }

        field = "youTubeConf";
        responseRoot[field] = youTubeRoot;

        field = "response";
        youTubeConfListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return youTubeConfListRoot;
}

string MMSEngineDBFacade::getYouTubeRefreshTokenByConfigurationLabel(
    int64_t workspaceKey, string youTubeConfigurationLabel
)
{
    string      lastSQLCommand;
    string      youTubeRefreshToken;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getYouTubeRefreshTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select refreshToken from MMS_Conf_YouTube where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, youTubeConfigurationLabel);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_YouTube failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            youTubeRefreshToken = resultSet->getString("refreshToken");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return youTubeRefreshToken;
}

int64_t MMSEngineDBFacade::addFacebookConf(
    int64_t workspaceKey,
    string label,
    string pageToken)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_Facebook(workspaceKey, label, pageToken) values ("
                "?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, pageToken);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyFacebookConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string pageToken)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "update MMS_Conf_Facebook set label = ?, pageToken = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, pageToken);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeFacebookConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Conf_Facebook where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getFacebookConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value facebookConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getFacebookConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;
            }
            
            field = "requestParameters";
            facebookConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_Facebook ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value facebookRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, pageToken from MMS_Conf_Facebook ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value facebookConfRoot;

                field = "confKey";
                facebookConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                facebookConfRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "pageToken";
                facebookConfRoot[field] = static_cast<string>(resultSet->getString("pageToken"));

                facebookRoot.append(facebookConfRoot);
            }
        }

        field = "facebookConf";
        responseRoot[field] = facebookRoot;

        field = "response";
        facebookConfListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return facebookConfListRoot;
}

string MMSEngineDBFacade::getFacebookPageTokenByConfigurationLabel(
    int64_t workspaceKey, string facebookConfigurationLabel
)
{
    string      lastSQLCommand;
    string      facebookPageToken;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getFacebookPageTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", facebookConfigurationLabel: " + facebookConfigurationLabel
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select pageToken from MMS_Conf_Facebook where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, facebookConfigurationLabel);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Facebook failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", facebookConfigurationLabel: " + facebookConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            facebookPageToken = resultSet->getString("pageToken");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return facebookPageToken;
}

int64_t MMSEngineDBFacade::addLiveURLConf(
    int64_t workspaceKey,
    string label,
    string liveURL)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_LiveURL(workspaceKey, label, liveURL) values ("
                "?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, liveURL);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyLiveURLConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string liveURL)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "update MMS_Conf_LiveURL set label = ?, liveURL = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, liveURL);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeLiveURLConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Conf_LiveURL where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getLiveURLConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value liveURLConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getLiveURLConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;
            }
            
            field = "requestParameters";
            liveURLConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_LiveURL ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value liveURLRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, liveURL from MMS_Conf_LiveURL ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value liveURLConfRoot;

                field = "confKey";
                liveURLConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                liveURLConfRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "liveURL";
                liveURLConfRoot[field] = static_cast<string>(resultSet->getString("liveURL"));

                liveURLRoot.append(liveURLConfRoot);
            }
        }

        field = "liveURLConf";
        responseRoot[field] = liveURLRoot;

        field = "response";
        liveURLConfListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return liveURLConfListRoot;
}

string MMSEngineDBFacade::getLiveURLByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
    string      lastSQLCommand;
    string      liveURL;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getLiveURLByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select liveURL from MMS_Conf_LiveURL where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_LiveURL failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            liveURL = resultSet->getString("liveURL");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return liveURL;
}

int64_t MMSEngineDBFacade::addFTPConf(
    int64_t workspaceKey,
    string label,
    string server, int port, string userName, string password, string remoteDirectory)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_FTP(workspaceKey, label, server, port, userName, password, remoteDirectory) values ("
                "?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, server);
            preparedStatement->setInt(queryParameterIndex++, port);
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, password);
            preparedStatement->setString(queryParameterIndex++, remoteDirectory);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyFTPConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string server, int port, string userName, string password, string remoteDirectory)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "update MMS_Conf_FTP set label = ?, server = ?, port = ?, userName = ?, password = ?, remoteDirectory = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, server);
            preparedStatement->setInt(queryParameterIndex++, port);
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, password);
            preparedStatement->setString(queryParameterIndex++, remoteDirectory);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeFTPConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Conf_FTP where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getFTPConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value ftpConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getFTPConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;
            }
            
            field = "requestParameters";
            ftpConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_FTP ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value ftpRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, server, port, userName, password, remoteDirectory from MMS_Conf_FTP ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value ftpConfRoot;

                field = "confKey";
                ftpConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                ftpConfRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "server";
                ftpConfRoot[field] = static_cast<string>(resultSet->getString("server"));

                field = "port";
                ftpConfRoot[field] = resultSet->getInt("port");

                field = "userName";
                ftpConfRoot[field] = static_cast<string>(resultSet->getString("userName"));

                field = "password";
                ftpConfRoot[field] = static_cast<string>(resultSet->getString("password"));

                field = "remoteDirectory";
                ftpConfRoot[field] = static_cast<string>(resultSet->getString("remoteDirectory"));

                ftpRoot.append(ftpConfRoot);
            }
        }

        field = "ftpConf";
        responseRoot[field] = ftpRoot;

        field = "response";
        ftpConfListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return ftpConfListRoot;
}

tuple<string, int, string, string, string> MMSEngineDBFacade::getFTPByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
    string      lastSQLCommand;
	tuple<string, int, string, string, string> ftp;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getFTPByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select server, port, userName, password, remoteDirectory from MMS_Conf_FTP where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_FTP failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string server = resultSet->getString("server");
            int port = resultSet->getInt("port");
            string userName = resultSet->getString("userName");
            string password = resultSet->getString("password");
            string remoteDirectory = resultSet->getString("remoteDirectory");

			ftp = make_tuple(server, port, userName, password, remoteDirectory);
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return ftp;
}

int64_t MMSEngineDBFacade::addEMailConf(
    int64_t workspaceKey,
    string label,
    string addresses, string subject, string message)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_EMail(workspaceKey, label, addresses, subject, message) values ("
                "?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, addresses);
            preparedStatement->setString(queryParameterIndex++, subject);
            preparedStatement->setString(queryParameterIndex++, message);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyEMailConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string addresses, string subject, string message)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "update MMS_Conf_EMail set label = ?, addresses = ?, subject = ?, message = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, addresses);
            preparedStatement->setString(queryParameterIndex++, subject);
            preparedStatement->setString(queryParameterIndex++, message);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeEMailConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Conf_EMail where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getEMailConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value emailConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEMailConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;
            }
            
            field = "requestParameters";
            emailConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_EMail ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value emailRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, addresses, subject, message from MMS_Conf_EMail ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value emailConfRoot;

                field = "confKey";
                emailConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                emailConfRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "addresses";
                emailConfRoot[field] = static_cast<string>(resultSet->getString("addresses"));

                field = "subject";
                emailConfRoot[field] = static_cast<string>(resultSet->getString("subject"));

                field = "message";
                emailConfRoot[field] = static_cast<string>(resultSet->getString("message"));

                emailRoot.append(emailConfRoot);
            }
        }

        field = "emailConf";
        responseRoot[field] = emailRoot;

        field = "response";
        emailConfListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return emailConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getEMailByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
    string      lastSQLCommand;
	tuple<string, string, string> email;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getEMailByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select addresses, subject, message from MMS_Conf_EMail where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_EMail failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            string addresses = resultSet->getString("addresses");
            string subject = resultSet->getString("subject");
            string message = resultSet->getString("message");

			email = make_tuple(addresses, subject, message);
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return email;
}

bool MMSEngineDBFacade::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
}

int64_t MMSEngineDBFacade::getLastInsertId(shared_ptr<MySQLConnection> conn)
{
    int64_t         lastInsertId;
    
    string      lastSQLCommand;

    try
    {
        lastSQLCommand = 
            "select LAST_INSERT_ID()";
        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

        if (resultSet->next())
        {
            lastInsertId = resultSet->getInt64(1);
        }
        else
        {
            string error ("select LAST_INSERT_ID failed");
            
            _logger->error(error);
            
            throw runtime_error(error);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return lastInsertId;
}

