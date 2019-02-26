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

#include <random>
#include "catralibraries/Encrypt.h"
#include "catralibraries/FileIO.h"

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
    _mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName,
            selectTestingConnection);

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

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(int64_t workspaceKey)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	
    _logger->debug(__FILEREF__ + "DB connection borrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );

    string lastSQLCommand =
        "select workspaceKey, name, directoryName, maxStorageInMB, maxEncodingPriority from MMS_Workspace where workspaceKey = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    int queryParameterIndex = 1;
    preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
    if (resultSet->next())
    {
        workspace->_workspaceKey = resultSet->getInt("workspaceKey");
        workspace->_name = resultSet->getString("name");
        workspace->_directoryName = resultSet->getString("directoryName");
        workspace->_maxStorageInMB = resultSet->getInt("maxStorageInMB");
        workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("maxEncodingPriority")));

        // getTerritories(workspace);
    }
    else
    {
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceKey: " + to_string(workspaceKey)
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _logger->debug(__FILEREF__ + "DB connection unborrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );
    _connectionPool->unborrow(conn);
    
    return workspace;
}

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(string workspaceName)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	
    _logger->debug(__FILEREF__ + "DB connection borrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );

    string lastSQLCommand =
        "select workspaceKey, name, directoryName, maxStorageInMB, maxEncodingPriority from MMS_Workspace where name = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    int queryParameterIndex = 1;
    preparedStatement->setString(queryParameterIndex++, workspaceName);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
    if (resultSet->next())
    {
        workspace->_workspaceKey = resultSet->getInt("workspaceKey");
        workspace->_name = resultSet->getString("name");
        workspace->_directoryName = resultSet->getString("directoryName");
        workspace->_maxStorageInMB = resultSet->getInt("maxStorageInMB");
        workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("maxEncodingPriority")));

        // getTerritories(workspace);
    }
    else
    {
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceName: " + workspaceName
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _logger->debug(__FILEREF__ + "DB connection unborrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );
    _connectionPool->unborrow(conn);
    
    return workspace;
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
}
*/

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUserAndAddWorkspace(
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    string workspaceName,
    string workspaceDirectoryName,
    WorkspaceType workspaceType,
    string deliveryURL,
    EncodingPriority maxEncodingPriority,
    EncodingPeriod encodingPeriod,
    long maxIngestionsNumber,
    long maxStorageInMB,
    string languageCode,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    int64_t         userKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
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
            lastSQLCommand = 
                "insert into MMS_User (userKey, name, eMailAddress, password, country, creationDate, expirationDate) values ("
                "NULL, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);
            preparedStatement->setString(queryParameterIndex++, userPassword);
            preparedStatement->setString(queryParameterIndex++, userCountry);
            {
                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(userExpirationDate);
                
                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }
            
            preparedStatement->executeUpdate();
        }
        
        userKey = getLastInsertId(conn);

        {
            bool admin = false;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            
            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
                    userKey,
                    admin,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
                    workspaceName,
                    workspaceDirectoryName,
                    workspaceType,
                    deliveryURL,
                    maxEncodingPriority,
                    encodingPeriod,
                    maxIngestionsNumber,
                    maxStorageInMB,
                    languageCode,
                    userExpirationDate);

            workspaceKey = workspaceKeyAndConfirmationCode.first;
            confirmationCode = workspaceKeyAndConfirmationCode.second;
        }

        /*
        {
            bool enabled = false;
            
            lastSQLCommand = 
                    "insert into MMS_Workspace ("
                    "workspaceKey, creationDate, name, directoryName, workspaceType, deliveryURL, isEnabled, maxEncodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB, languageCode) values ("
                    "NULL,         NULL,         ?,    ?,             ?,             ?,           ?,         ?,                   ?,              ?,                   ?,              ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, workspaceName);
            preparedStatement->setString(queryParameterIndex++, workspaceDirectoryName);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(workspaceType));
            if (deliveryURL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryURL);
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(maxEncodingPriority));
            preparedStatement->setString(queryParameterIndex++, toString(encodingPeriod));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionsNumber);
            preparedStatement->setInt(queryParameterIndex++, maxStorageInMB);
            preparedStatement->setString(queryParameterIndex++, languageCode);

            preparedStatement->executeUpdate();
        }

        workspaceKey = getLastInsertId(conn);

        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
        confirmationCode = to_string(e());
        {
            string flags;
            {
                bool admin = false;
                bool ingestWorkflow = true;
                bool createProfiles = true;
                bool deliveryAuthorization = true;
                bool shareWorkspace = true;
                bool editMedia = true;

                if (admin)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("ADMIN");
                }

                if (ingestWorkflow)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("INGEST_WORKFLOW");
                }

                if (createProfiles)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATE_PROFILES");
                }

                if (deliveryAuthorization)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("DELIVERY_AUTHORIZATION");
                }

                if (shareWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("SHARE_WORKSPACE");
                }

                if (editMedia)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_MEDIA");
                }
            }
            
            lastSQLCommand = 
                    "insert into MMS_ConfirmationCode (userKey, flags, workspaceKey, isSharedWorkspace, creationDate, confirmationCode) values ("
                    "?, ?, ?, 0, NOW(), ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setString(queryParameterIndex++, flags);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, confirmationCode);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                    "insert into MMS_WorkspaceMoreInfo (workspaceKey, currentDirLevel1, currentDirLevel2, currentDirLevel3, startDateTime, endDateTime, currentIngestionsNumber) values ("
                    "?, 0, 0, 0, NOW(), NOW(), 0)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                "insert into MMS_ContentProvider (contentProviderKey, workspaceKey, name) values ("
                "NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, _defaultContentProviderName);

            preparedStatement->executeUpdate();
        }

        contentProviderKey = getLastInsertId(conn);

        // int64_t territoryKey = addTerritory(
        //        conn,
        //        workspaceKey,
        //        _defaultTerritoryName);
        
        */
        
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
            }
        }

        throw e;
    }
    
    tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
            make_tuple(workspaceKey, userKey, confirmationCode);
    
    return workspaceKeyUserKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::createWorkspace(
    int64_t userKey,
    string workspaceName,
    string workspaceDirectoryName,
    WorkspaceType workspaceType,
    string deliveryURL,
    EncodingPriority maxEncodingPriority,
    EncodingPeriod encodingPeriod,
    long maxIngestionsNumber,
    long maxStorageInMB,
    string languageCode,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
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
            bool admin = false;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            
            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
                    userKey,
                    admin,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
                    workspaceName,
                    workspaceDirectoryName,
                    workspaceType,
                    deliveryURL,
                    maxEncodingPriority,
                    encodingPeriod,
                    maxIngestionsNumber,
                    maxStorageInMB,
                    languageCode,
                    userExpirationDate);
            
            workspaceKey = workspaceKeyAndConfirmationCode.first;
            confirmationCode = workspaceKeyAndConfirmationCode.second;
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
            }
        }

        throw e;
    }
    
    pair<int64_t,string> workspaceKeyAndConfirmationCode = 
            make_pair(workspaceKey, confirmationCode);
    
    return workspaceKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::registerUserAndShareWorkspace(
    bool userAlreadyPresent,
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization, bool shareWorkspace, bool editMedia,
    int64_t workspaceKeyToBeShared,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         userKey;
    string          confirmationCode;
    
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
        
        if (userAlreadyPresent)
        {
            lastSQLCommand = 
                "select userKey from MMS_User where eMailAddress = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User does not exist"
                    + ", userEmailAddress: " + userEmailAddress
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "insert into MMS_User (userKey, name, eMailAddress, password, country, creationDate, expirationDate) values ("
                "NULL, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);
            preparedStatement->setString(queryParameterIndex++, userPassword);
            preparedStatement->setString(queryParameterIndex++, userCountry);
            {
                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(userExpirationDate);
                
                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }
            
            preparedStatement->executeUpdate();

            userKey = getLastInsertId(conn);
        }    
        
        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
        confirmationCode = to_string(e());

        {
            string flags;
            {
                bool admin = false;

                if (admin)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("ADMIN");
                }

                if (ingestWorkflow)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("INGEST_WORKFLOW");
                }

                if (createProfiles)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATE_PROFILES");
                }

                if (deliveryAuthorization)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("DELIVERY_AUTHORIZATION");
                }

                if (shareWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("SHARE_WORKSPACE");
                }

                if (editMedia)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_MEDIA");
                }
            }

            lastSQLCommand = 
                    "insert into MMS_ConfirmationCode (userKey, flags, workspaceKey, isSharedWorkspace, creationDate, confirmationCode) values ("
                    "?, ?, ?, 1, NOW(), ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setString(queryParameterIndex++, flags);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKeyToBeShared);
            preparedStatement->setString(queryParameterIndex++, confirmationCode);

            preparedStatement->executeUpdate();
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
            }
        }

        throw e;
    }
    
    pair<int64_t,string> userKeyAndConfirmationCode = 
            make_pair(userKey, confirmationCode);
    
    return userKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::addWorkspace(
        shared_ptr<MySQLConnection> conn,
        int64_t userKey,
        bool admin, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
        bool shareWorkspace, bool editMedia,
        string workspaceName,
        string workspaceDirectoryName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    try
    {
        {
            bool enabled = false;
            
            lastSQLCommand = 
                    "insert into MMS_Workspace ("
                    "workspaceKey, creationDate, name, directoryName, workspaceType, deliveryURL, isEnabled, maxEncodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB, languageCode) values ("
                    "NULL,         NULL,         ?,    ?,             ?,             ?,           ?,         ?,                   ?,              ?,                   ?,              ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, workspaceName);
            preparedStatement->setString(queryParameterIndex++, workspaceDirectoryName);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(workspaceType));
            if (deliveryURL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryURL);
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(maxEncodingPriority));
            preparedStatement->setString(queryParameterIndex++, toString(encodingPeriod));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionsNumber);
            preparedStatement->setInt(queryParameterIndex++, maxStorageInMB);
            preparedStatement->setString(queryParameterIndex++, languageCode);

            preparedStatement->executeUpdate();
        }

        workspaceKey = getLastInsertId(conn);

        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
        confirmationCode = to_string(e());
        {
            string flags;
            {
                if (admin)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("ADMIN");
                }

                if (ingestWorkflow)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("INGEST_WORKFLOW");
                }

                if (createProfiles)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATE_PROFILES");
                }

                if (deliveryAuthorization)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("DELIVERY_AUTHORIZATION");
                }

                if (shareWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("SHARE_WORKSPACE");
                }

                if (editMedia)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_MEDIA");
                }
            }
            
            lastSQLCommand = 
                    "insert into MMS_ConfirmationCode (userKey, flags, workspaceKey, isSharedWorkspace, creationDate, confirmationCode) values ("
                    "?, ?, ?, 0, NOW(), ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setString(queryParameterIndex++, flags);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, confirmationCode);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                    "insert into MMS_WorkspaceMoreInfo (workspaceKey, currentDirLevel1, currentDirLevel2, currentDirLevel3, startDateTime, endDateTime, currentIngestionsNumber) values ("
                    "?, 0, 0, 0, NOW(), NOW(), 0)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                "insert into MMS_ContentProvider (contentProviderKey, workspaceKey, name) values ("
                "NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, _defaultContentProviderName);

            preparedStatement->executeUpdate();
        }

        contentProviderKey = getLastInsertId(conn);

        /*
        int64_t territoryKey = addTerritory(
                conn,
                workspaceKey,
                _defaultTerritoryName);
        */        
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    
    pair<int64_t,string> workspaceKeyAndConfirmationCode = 
            make_pair(workspaceKey, confirmationCode);
    
    return workspaceKeyAndConfirmationCode;
}

tuple<string,string,string> MMSEngineDBFacade::confirmRegistration(
    string confirmationCode
)
{
    string      apiKey;
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
        
        int64_t     userKey;
        string      flags;
        int64_t     workspaceKey;
        bool        isSharedWorkspace;
        {
            lastSQLCommand = 
                "select userKey, flags, workspaceKey, isSharedWorkspace from MMS_ConfirmationCode where confirmationCode = ? and DATE_ADD(creationDate, INTERVAL ? DAY) >= NOW()";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, confirmationCode);
            preparedStatement->setInt(queryParameterIndex++, _confirmationCodeRetentionInDays);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
                flags = resultSet->getString("flags");
                workspaceKey = resultSet->getInt64("workspaceKey");
                isSharedWorkspace = resultSet->getInt("isSharedWorkspace") == 1 ? true : false;
            }
            else
            {
                string errorMessage = __FILEREF__ + "Confirmation Code not found or expired"
                    + ", confirmationCode: " + confirmationCode
                    + ", _confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        // check if the apiKey is already present (maybe this is the second time the confirmRegistration API is called
        bool apiKeyAlreadyPresent = false;
        {
            lastSQLCommand = 
                "select apiKey from MMS_APIKey where userKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                apiKey = resultSet->getString("apiKey");
                apiKeyAlreadyPresent = true;
            }
        }

        if (!apiKeyAlreadyPresent)
        {
            if (!isSharedWorkspace)
            {
                bool enabled = true;

                lastSQLCommand = 
                    "update MMS_Workspace set isEnabled = ? where workspaceKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, enabled);
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", enabled: " + to_string(enabled)
                            + ", workspaceKey: " + to_string(workspaceKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
        }

        string emailAddress;
        string name;
        {
            lastSQLCommand = 
                "select name, eMailAddress from MMS_User where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                name = resultSet->getString("name");
                emailAddress = resultSet->getString("eMailAddress");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User are not present"
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        if (!apiKeyAlreadyPresent)
        {
            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
            default_random_engine e(seed);

            string sourceApiKey = emailAddress + "__SEP__" + to_string(e());
            apiKey = Encrypt::encrypt(sourceApiKey);

            bool isOwner = isSharedWorkspace ? false : true;
            
            lastSQLCommand = 
                "insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, flags, creationDate, expirationDate) values ("
                "?, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt(queryParameterIndex++, isOwner);
            preparedStatement->setString(queryParameterIndex++, flags);
            {
                chrono::system_clock::time_point apiKeyExpirationDate =
                        chrono::system_clock::now() + chrono::hours(24 * 365 * 10);     // chrono::system_clock::time_point userExpirationDate

                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(apiKeyExpirationDate);

                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }

            preparedStatement->executeUpdate();
        }

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

        return make_tuple(apiKey, name, emailAddress);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
            }
        }
        
        throw e;
    }    
}

tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool> MMSEngineDBFacade::checkAPIKey (string apiKey)
{
    shared_ptr<Workspace> workspace;
    int64_t         userKey;
    string          flags;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t         workspaceKey;
        
        {
            lastSQLCommand = 
                "select userKey, workspaceKey, flags from MMS_APIKey where apiKey = ? and expirationDate >= NOW()";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
                workspaceKey = resultSet->getInt64("workspaceKey");
                flags = resultSet->getString("flags");
            }
            else
            {
                string errorMessage = __FILEREF__ + "apiKey is not present or it is expired"
                    + ", apiKey: " + apiKey
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw APIKeyNotFoundOrExpired();
            }
        }

        
        workspace = getWorkspace(workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw se;
    }
    catch(APIKeyNotFoundOrExpired e)
    {        
        string exceptionMessage(e.what());

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
        }

        throw e;
    }
    
    tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool,bool,bool,bool> userKeyWorkspaceAndFlags;
    
    userKeyWorkspaceAndFlags = make_tuple(userKey, workspace,
        flags.find("ADMIN") == string::npos ? false : true,
        flags.find("INGEST_WORKFLOW") == string::npos ? false : true,
        flags.find("CREATE_PROFILES") == string::npos ? false : true,
        flags.find("DELIVERY_AUTHORIZATION") == string::npos ? false : true,
        flags.find("SHARE_WORKSPACE") == string::npos ? false : true,
        flags.find("EDIT_MEDIA") == string::npos ? false : true
    );
            
    return userKeyWorkspaceAndFlags;
}

Json::Value MMSEngineDBFacade::login (
        string eMailAddress, string password)
{
    Json::Value     loginDetailsRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select userKey, name, country, "
                "DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate, "
                "DATE_FORMAT(convert_tz(expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate "
                "from MMS_User where eMailAddress = ? and password = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, eMailAddress);
            preparedStatement->setString(queryParameterIndex++, password);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                string field = "userKey";
                loginDetailsRoot[field] = resultSet->getInt64("userKey");

                field = "name";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "eMailAddress";
                loginDetailsRoot[field] = eMailAddress;

                field = "country";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("country"));

                field = "creationDate";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

                field = "expirationDate";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("expirationDate"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "email and/or password are wrong"
                    + ", eMailAddress: " + eMailAddress
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw LoginFailed();
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw se;
    }
    catch(LoginFailed e)
    {        
        string exceptionMessage(e.what());

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
        }

        throw e;
    }
    
    return loginDetailsRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceDetails (
    int64_t userKey)
{
    Json::Value     workspaceDetailsRoot(Json::arrayValue);
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, w.languageCode, " 
                    "a.apiKey, a.isOwner, a.flags, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w where a.workspaceKey = w.workspaceKey and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value workspaceDetailRoot;

                string field = "workspaceKey";
                workspaceDetailRoot[field] = resultSet->getInt64("workspaceKey");
                
                field = "isEnabled";
                workspaceDetailRoot[field] = resultSet->getInt("isEnabled") == 1 ? "true" : "false";

                field = "workspaceName";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "maxEncodingPriority";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("maxEncodingPriority"));

                field = "encodingPeriod";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("encodingPeriod"));

                field = "maxIngestionsNumber";
                workspaceDetailRoot[field] = resultSet->getInt("maxIngestionsNumber");

                field = "maxStorageInMB";
                workspaceDetailRoot[field] = resultSet->getInt("maxStorageInMB");

                field = "languageCode";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("languageCode"));

                field = "creationDate";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

                field = "apiKey";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("apiKey"));

                field = "owner";
                workspaceDetailRoot[field] = resultSet->getInt("isOwner") == 1 ? "true" : "false";

                string flags = resultSet->getString("flags");
                
                field = "admin";
                workspaceDetailRoot[field] = flags.find("ADMIN") == string::npos ? false : true;
                
                field = "ingestWorkflow";
                workspaceDetailRoot[field] = flags.find("INGEST_WORKFLOW") == string::npos ? false : true;

                field = "createProfiles";
                workspaceDetailRoot[field] = flags.find("CREATE_PROFILES") == string::npos ? false : true;

                field = "deliveryAuthorization";
                workspaceDetailRoot[field] = flags.find("DELIVERY_AUTHORIZATION") == string::npos ? false : true;

                field = "shareWorkspace";
                workspaceDetailRoot[field] = flags.find("SHARE_WORKSPACE") == string::npos ? false : true;

                field = "editMedia";
                workspaceDetailRoot[field] = flags.find("EDIT_MEDIA") == string::npos ? false : true;
                
                workspaceDetailsRoot.append(workspaceDetailRoot);                        
            }
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw se;
    }
    catch(LoginFailed e)
    {        
        string exceptionMessage(e.what());

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
        }

        throw e;
    }
    
    return workspaceDetailsRoot;
}

Json::Value MMSEngineDBFacade::updateWorkspaceDetails (
        int64_t userKey,
        int64_t workspaceKey,
        bool newEnabled, string newMaxEncodingPriority,
        string newEncodingPeriod, int64_t newMaxIngestionsNumber,
        int64_t newMaxStorageInMB, string newLanguageCode,
        bool newIngestWorkflow, bool newCreateProfiles,
        bool newDeliveryAuthorization, bool newShareWorkspace,
        bool newEditMedia)
{
    Json::Value workspaceDetailRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // check if ADMIN flag is already present
        bool admin = false;
        {
            lastSQLCommand = 
                "select flags from MMS_APIKey where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                string flags = resultSet->getString("flags");
                
                admin = flags.find("ADMIN") == string::npos ? false : true;
            }
        }

        {
            lastSQLCommand = 
                "update MMS_Workspace set isEnabled = ?, maxEncodingPriority = ?, encodingPeriod = ?, maxIngestionsNumber = ?, "
                "maxStorageInMB = ?, languageCode = ? "
                "where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, newEnabled);
            preparedStatement->setString(queryParameterIndex++, newMaxEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, newEncodingPeriod);
            preparedStatement->setInt64(queryParameterIndex++, newMaxIngestionsNumber);
            preparedStatement->setInt64(queryParameterIndex++, newMaxStorageInMB);
            preparedStatement->setString(queryParameterIndex++, newLanguageCode);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newEnabled: " + to_string(newEnabled)
                        + ", newMaxEncodingPriority: " + newMaxEncodingPriority
                        + ", newEncodingPeriod: " + newEncodingPeriod
                        + ", newMaxIngestionsNumber: " + to_string(newMaxIngestionsNumber)
                        + ", newMaxStorageInMB: " + to_string(newMaxStorageInMB)
                        + ", newLanguageCode: " + newLanguageCode
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        string flags;
        {
            if (admin)
                flags.append("ADMIN");
            if (newIngestWorkflow)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("INGEST_WORKFLOW");
            }
            if (newCreateProfiles)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("CREATE_PROFILES");
            }
            if (newDeliveryAuthorization)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("DELIVERY_AUTHORIZATION");
            }
            if (newShareWorkspace)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("SHARE_WORKSPACE");
            }
            if (newEditMedia)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("EDIT_MEDIA");
            }

            lastSQLCommand = 
                "update MMS_APIKey set flags = ? "
                "where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, flags);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", userKey: " + to_string(userKey)
                        + ", flags: " + flags
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
                
        string field = "workspaceKey";
        workspaceDetailRoot[field] = workspaceKey;

        field = "isEnabled";
        workspaceDetailRoot[field] = (newEnabled ? "true" : "false");

        field = "maxEncodingPriority";
        workspaceDetailRoot[field] = newMaxEncodingPriority;

        field = "encodingPeriod";
        workspaceDetailRoot[field] = newEncodingPeriod;

        field = "maxIngestionsNumber";
        workspaceDetailRoot[field] = newMaxIngestionsNumber;

        field = "maxStorageInMB";
        workspaceDetailRoot[field] = newMaxStorageInMB;

        field = "languageCode";
        workspaceDetailRoot[field] = newLanguageCode;

        field = "admin";
        workspaceDetailRoot[field] = admin ? true : false;

        field = "ingestWorkflow";
        workspaceDetailRoot[field] = newIngestWorkflow ? true : false;

        field = "createProfiles";
        workspaceDetailRoot[field] = newCreateProfiles ? true : false;

        field = "deliveryAuthorization";
        workspaceDetailRoot[field] = newDeliveryAuthorization ? true : false;

        field = "shareWorkspace";
        workspaceDetailRoot[field] = newShareWorkspace ? true : false;

        field = "editMedia";
        workspaceDetailRoot[field] = newEditMedia ? true : false;
        
        {
            lastSQLCommand = 
                "select w.name, a.apiKey, a.isOwner, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w where a.workspaceKey = w.workspaceKey and a.workspaceKey = ? and a.userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                field = "workspaceName";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "creationDate";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

                field = "apiKey";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("apiKey"));

                field = "owner";
                workspaceDetailRoot[field] = resultSet->getInt("isOwner") == 1 ? "true" : "false";
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw e;
    }
    
    return workspaceDetailRoot;
}

pair<string, string> MMSEngineDBFacade::getUserDetails (int64_t userKey)
{
    string emailAddress;
    string name;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select name, eMailAddress from MMS_User where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                name = resultSet->getString("name");
                emailAddress = resultSet->getString("eMailAddress");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User are not present"
                    + ", userKey: " + to_string(userKey)
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
        }

        throw se;
    }
    catch(APIKeyNotFoundOrExpired e)
    {        
        string exceptionMessage(e.what());

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
        }

        throw e;
    }
    
    pair<string, string> emailAddressAndName = make_pair(emailAddress, name);
            
    return emailAddressAndName;
}

Json::Value MMSEngineDBFacade::updateUser (
        int64_t userKey,
        string name, 
        string email, 
        string password,
        string country)
{
    Json::Value     loginDetailsRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_User set name = ?, country = ?, eMailAddress = ?, password = ? "
                // "expirationDate = convert_tz(STR_TO_DATE(?,'%Y-%m-%dT%H:%i:%SZ'), '+00:00', @@session.time_zone) "
                "where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, name);
            preparedStatement->setString(queryParameterIndex++, country);
            preparedStatement->setString(queryParameterIndex++, email);
            preparedStatement->setString(queryParameterIndex++, password);
            // preparedStatement->setString(queryParameterIndex++, expirationDate);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", userKey: " + to_string(userKey)
                        + ", name: " + name
                        + ", country: " + country
                        + ", email: " + email
                        + ", password: " + password
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        {
            lastSQLCommand = 
                "select DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate, "
                "DATE_FORMAT(convert_tz(expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate "
                "from MMS_User where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                string field = "creationDate";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

                field = "expirationDate";
                loginDetailsRoot[field] = static_cast<string>(resultSet->getString("expirationDate"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "userKey is wrong"
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        string field = "userKey";
        loginDetailsRoot[field] = userKey;

        field = "name";
        loginDetailsRoot[field] = name;

        field = "eMailAddress";
        loginDetailsRoot[field] = email;

        field = "country";
        loginDetailsRoot[field] = country;
                
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw e;
    }
    
    return loginDetailsRoot;
}

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

/*
bool MMSEngineDBFacade::isLoginValid(
        string emailAddress,
        string password
)
{
    bool        isLoginValid;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select userKey from MMS_User where eMailAddress = ? and password = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, emailAddress);
            preparedStatement->setString(queryParameterIndex++, password);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int64_t userKey = resultSet->getInt("userKey");
                
                isLoginValid = true;
            }
            else
            {
                isLoginValid = false;
            }            
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }
    
    return isLoginValid;
}
*/

/*
string MMSEngineDBFacade::getPassword(string emailAddress)
{
    string      password;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select password from MMS_User where eMailAddress = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, emailAddress);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                password = resultSet->getString("password");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User is not present"
                    + ", emailAddress: " + emailAddress
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
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }
    
    return password;
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
                    "insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, type, label, metaDataContent, ingestionDate, lastUpdate, status) values ("
                    "NULL, ?, ?, ?, ?, NOW(), NOW(), ?)";

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

/*
void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        string processorMMS
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_IngestionJob set processorMMS = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
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
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", processorMMS: " + processorMMS
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
}
*/
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

/*
void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        int64_t mediaItemKey,
        int64_t physicalPathKey,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        string errorMessageForSQL;
        if (errorMessage == "")
            errorMessageForSQL = errorMessage;
        else
        {
            if (errorMessageForSQL.length() >= 1024)
                errorMessageForSQL.substr(0, 1024);
            else
                errorMessageForSQL = errorMessage;
        }
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ?, mediaItemKey = ?, physicalPathKey = ?, endProcessing = NOW(), processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (mediaItemKey == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
            else
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            if (physicalPathKey == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
            else
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", physicalPathKey: " + to_string(physicalPathKey)
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ?, processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
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

                throw runtime_error(errorMessage);                    
            }
        }
        
        manageIngestionJobStatusUpdate (ingestionJobKey, newIngestionStatus, conn);

        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
}
*/

/*
void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionType ingestionType,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        string errorMessageForSQL;
        if (errorMessage == "")
            errorMessageForSQL = errorMessage;
        else
        {
            if (errorMessageForSQL.length() >= 1024)
                errorMessageForSQL.substr(0, 1024);
            else
                errorMessageForSQL = errorMessage;
        }
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set ingestionType = ?, status = ?, endProcessing = NOW(), processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(ingestionType));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
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

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set ingestionType = ?, status = ?, processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(ingestionType));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
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

                throw runtime_error(errorMessage);                    
            }
        }
        
        manageIngestionJobStatusUpdate (ingestionJobKey, newIngestionStatus, conn);

        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);

        throw e;
    }    
}
*/

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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
			// This select returns all the ingestion joob key of running HA LiveRecording

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, toleranceMinutes);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

				_logger->info(__FILEREF__ + "Manage HA LiveRecording"
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

					_logger->info(__FILEREF__ + "Manage HA LiveRecording Chunk"
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

					_logger->info(__FILEREF__ + "Manage HA LiveRecording Chunks"
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

						_logger->info(__FILEREF__ + "Manage HA LiveRecording, reset of chunk"
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
            }
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + e.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
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
        }

        throw e;
    } 
    
    return ftp;
}

int64_t MMSEngineDBFacade::addEMailConf(
    int64_t workspaceKey,
    string label,
    string address, string subject, string message)
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
                "insert into MMS_Conf_EMail(workspaceKey, label, address, subject, message) values ("
                "?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, address);
            preparedStatement->setString(queryParameterIndex++, subject);
            preparedStatement->setString(queryParameterIndex++, message);

            preparedStatement->executeUpdate();

            confKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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
        }

        throw e;
    }  
    
    return confKey;
}

void MMSEngineDBFacade::modifyEMailConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string address, string subject, string message)
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
                "update MMS_Conf_EMail set label = ?, address = ?, subject = ?, message = ? where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, address);
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
                string ("select confKey, label, address, subject, message from MMS_Conf_EMail ") 
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

                field = "address";
                emailConfRoot[field] = static_cast<string>(resultSet->getString("address"));

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
                string("select address, subject, message from MMS_Conf_EMail where workspaceKey = ? and label = ?");

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

            string address = resultSet->getString("address");
            string subject = resultSet->getString("subject");
            string message = resultSet->getString("message");

			email = make_tuple(address, subject, message);
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
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

void MMSEngineDBFacade::createTablesIfNeeded()
{
    shared_ptr<MySQLConnection> conn = nullptr;

    string      lastSQLCommand;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());

        try
        {
            // This table has to be present before the connection pool is created
            // otherwise the connection pool fails.
            // It has to be created before running the executable
            lastSQLCommand = 
                "create table if not exists MMS_TestConnection ("
                    "testConnectionKey          BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "constraint MMS_TestConnection_PK PRIMARY KEY (testConnectionKey))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // maxEncodingPriority (0: low, 1: default, 2: high)
            // workspaceType: (0: Live Sessions only, 1: Ingestion + Delivery, 2: Encoding Only)
            // encodingPeriod: 0: Daily, 1: Weekly, 2: Monthly

            lastSQLCommand = 
                "create table if not exists MMS_Workspace ("
                    "workspaceKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "creationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "name                           VARCHAR (64) NOT NULL,"
                    "directoryName                  VARCHAR (64) NOT NULL,"
                    "workspaceType                  TINYINT NOT NULL,"
                    "deliveryURL                    VARCHAR (256) NULL,"
                    "isEnabled                      TINYINT (1) NOT NULL,"
                    "maxEncodingPriority            VARCHAR (32) NOT NULL,"
                    "encodingPeriod                 VARCHAR (64) NOT NULL,"
                    "maxIngestionsNumber            INT NOT NULL,"
                    "maxStorageInMB                 INT UNSIGNED NOT NULL,"
                    "languageCode                   VARCHAR (16) NOT NULL,"
                    "constraint MMS_Workspace_PK PRIMARY KEY (workspaceKey),"
                    "UNIQUE (name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create unique index MMS_Workspace_idx on MMS_Workspace (directoryName)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_User ("
                    "userKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "name				VARCHAR (128) NULL,"
                    "eMailAddress			VARCHAR (128) NULL,"
                    "password				VARCHAR (128) NOT NULL,"
                    "country				VARCHAR (64) NULL,"
                    "creationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "expirationDate			DATETIME NOT NULL,"
                    "constraint MMS_User_PK PRIMARY KEY (userKey), "
                    "UNIQUE (eMailAddress))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_APIKey ("
                    "apiKey             VARCHAR (128) NOT NULL,"
                    "userKey            BIGINT UNSIGNED NOT NULL,"
                    "workspaceKey       BIGINT UNSIGNED NOT NULL,"
                    "isOwner            TINYINT (1) NOT NULL,"
                    // same in MMS_ConfirmationCode
                    "flags              SET('ADMIN', 'INGEST_WORKFLOW', 'CREATE_PROFILES', 'DELIVERY_AUTHORIZATION', 'SHARE_WORKSPACE', 'EDIT_MEDIA') NOT NULL,"
                    "creationDate		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "expirationDate		DATETIME NOT NULL,"
                    "constraint MMS_APIKey_PK PRIMARY KEY (apiKey), "
                    "constraint MMS_APIKey_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_APIKey_FK2 foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create unique index MMS_APIKey_idx on MMS_APIKey (userKey, workspaceKey)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ConfirmationCode ("
                    "userKey                        BIGINT UNSIGNED NOT NULL,"
                    // same in MMS_APIKey
                    "flags              SET('ADMIN', 'INGEST_WORKFLOW', 'CREATE_PROFILES', 'DELIVERY_AUTHORIZATION', 'SHARE_WORKSPACE', 'EDIT_MEDIA') NOT NULL,"
                    "workspaceKey                   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "isSharedWorkspace              TINYINT (1) NOT NULL,"
                    "creationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "confirmationCode               VARCHAR (64) NOT NULL,"
                    "constraint MMS_ConfirmationCode_PK PRIMARY KEY (userKey, workspaceKey),"
                    "constraint MMS_ConfirmationCode_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_ConfirmationCode_FK2 foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (confirmationCode))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        /*
        try
        {
            // The territories are present only if the Workspace is a 'Content Provider'.
            // In this case we could have two scenarios:
            // - workspace not having territories (we will have just one row in this table with Name set as 'default')
            // - workspace having at least one territory (we will as much rows in this table according the number of territories)
            lastSQLCommand = 
                "create table if not exists MMS_Territory ("
                    "territoryKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  				BIGINT UNSIGNED NOT NULL,"
                    "name					VARCHAR (64) NOT NULL,"
                    "currency					VARCHAR (16) DEFAULT NULL,"
                    "constraint MMS_Territory_PK PRIMARY KEY (territoryKey),"
                    "constraint MMS_Territory_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */    

        try
        {
            // create table MMS_WorkspaceMoreInfo. This table was created to move the fields
            //		that are updated during the ingestion from MMS_Workspace.
            //		That will avoid to put a lock in the MMS_Workspace during the update
            //		since the MMS_Workspace is a wide used table
            lastSQLCommand = 
                "create table if not exists MMS_WorkspaceMoreInfo ("
                    "workspaceKey  			BIGINT UNSIGNED NOT NULL,"
                    "currentDirLevel1			INT NOT NULL,"
                    "currentDirLevel2			INT NOT NULL,"
                    "currentDirLevel3			INT NOT NULL,"
                    "startDateTime			DATETIME NOT NULL,"
                    "endDateTime			DATETIME NOT NULL,"
                    "currentIngestionsNumber	INT NOT NULL,"
                    "constraint MMS_WorkspaceMoreInfo_PK PRIMARY KEY (workspaceKey), "
                    "constraint MMS_WorkspaceMoreInfo_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ContentProvider ("
                    "contentProviderKey                     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey                            BIGINT UNSIGNED NOT NULL,"
                    "name					VARCHAR (64) NOT NULL,"
                    "constraint MMS_ContentProvider_PK PRIMARY KEY (contentProviderKey), "
                    "constraint MMS_ContentProvider_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, name))" 
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // Technology.
            //      0: Images (Download),
            //      1: 3GPP (Streaming+Download),
            //      2: MPEG2-TS (IPhone Streaming),
            //      3: WEBM (VP8 and Vorbis)
            //      4: WindowsMedia,
            //      5: Adobe
            // workspaceKey NULL means predefined encoding profile
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfile ("
                    "encodingProfileKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  			BIGINT UNSIGNED NULL,"
                    "label				VARCHAR (64) NOT NULL,"
                    "contentType			VARCHAR (32) NOT NULL,"
                    "technology         		TINYINT NOT NULL,"
                    "jsonProfile    			VARCHAR (512) NOT NULL,"
                    "constraint MMS_EncodingProfile_PK PRIMARY KEY (encodingProfileKey), "
                    "constraint MMS_EncodingProfile_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, contentType, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            string predefinedProfilesDirectoryPath[3] = {
                _predefinedVideoProfilesDirectoryPath,
                _predefinedAudioProfilesDirectoryPath,
                _predefinedImageProfilesDirectoryPath
            };
            string videoSuffix("video");
            string audioSuffix("audio");
            string imageSuffix("image");
            
            for (string predefinedProfileDirectoryPath: predefinedProfilesDirectoryPath)
            {
                MMSEngineDBFacade::ContentType contentType;
                MMSEngineDBFacade::EncodingTechnology encodingTechnology;
                
                if (predefinedProfileDirectoryPath.size() >= videoSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-videoSuffix.size(), 
                            videoSuffix.size(), videoSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Video;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                }
                else if (predefinedProfileDirectoryPath.size() >= audioSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-audioSuffix.size(), 
                            audioSuffix.size(), audioSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Audio;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::MP4;
                }
                else if (predefinedProfileDirectoryPath.size() >= imageSuffix.size() 
                        && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-imageSuffix.size(), 
                            imageSuffix.size(), imageSuffix))
                {
                    contentType = MMSEngineDBFacade::ContentType::Image;
                    encodingTechnology = MMSEngineDBFacade::EncodingTechnology::Image;
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Wrong predefinedProfileDirectoryPath"
                           + ", predefinedProfileDirectoryPath: " + predefinedProfileDirectoryPath
                    ;
                    _logger->error(errorMessage);

                    continue;
                }

                FileIO::DirectoryEntryType_t detDirectoryEntryType;
                shared_ptr<FileIO::Directory> directory = FileIO::openDirectory (predefinedProfileDirectoryPath + "/");

                bool scanDirectoryFinished = false;
                while (!scanDirectoryFinished)
                {
                    string directoryEntry;
                    try
                    {
                        string directoryEntry = FileIO::readDirectory (directory,
                            &detDirectoryEntryType);

                        if (detDirectoryEntryType != FileIO::TOOLS_FILEIO_REGULARFILE)
                            continue;

                        size_t extensionIndex = directoryEntry.find_last_of(".");
                        if (extensionIndex == string::npos
                                || directoryEntry.substr(extensionIndex) != ".json")
                        {
                            string errorMessage = __FILEREF__ + "Wrong filename (encoding profile) extention"
                                   + ", directoryEntry: " + directoryEntry
                            ;
                            _logger->error(errorMessage);

                            continue;
                        }

                        string jsonProfile;
                        {        
                            ifstream profileFile(predefinedProfileDirectoryPath + "/" + directoryEntry);
                            stringstream buffer;
                            buffer << profileFile.rdbuf();

                            jsonProfile = buffer.str();

                            _logger->info(__FILEREF__ + "Reading profile"
                                + ", profile pathname: " + (predefinedProfileDirectoryPath + "/" + directoryEntry)
                                + ", profile: " + jsonProfile
                            );                            
                        }

                        string label = directoryEntry.substr(0, extensionIndex);
                        {                               
                            lastSQLCommand = 
                                "select encodingProfileKey from MMS_EncodingProfile where workspaceKey is null and contentType = ? and label = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                            preparedStatement->setString(queryParameterIndex++, label);
                            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                            if (resultSet->next())
                            {
                                int64_t encodingProfileKey     = resultSet->getInt64("encodingProfileKey");

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
                                    "NULL, NULL, ?, ?, ?, ?)";

                                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                    preparedStatement->setString(queryParameterIndex++, label);
                                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
                                preparedStatement->setString(queryParameterIndex++, jsonProfile);

                                preparedStatement->executeUpdate();

                                // encodingProfileKey = getLastInsertId(conn);
                            }
                        }
                    }
                    catch(DirectoryListFinished e)
                    {
                        scanDirectoryFinished = true;
                    }
                    catch(runtime_error e)
                    {
                        string errorMessage = __FILEREF__ + "listing directory failed"
                               + ", e.what(): " + e.what()
                        ;
                        _logger->error(errorMessage);

                        throw e;
                    }
                    catch(exception e)
                    {
                        string errorMessage = __FILEREF__ + "listing directory failed"
                               + ", e.what(): " + e.what()
                        ;
                        _logger->error(errorMessage);

                        throw e;
                    }
                }

                FileIO::closeDirectory (directory);
            }
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    
        
        try
        {
            // workspaceKey and name
            //      both NULL: global/system EncodingProfiles for the ContentType
            //      only name NULL: Workspace default EncodingProfiles for the ContentType
            //      both different by NULL: named Workspace EncodingProfiles for the ContentType
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSet ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  				BIGINT UNSIGNED NOT NULL,"
                    "contentType			VARCHAR (32) NOT NULL,"
                    "label					VARCHAR (64) NOT NULL,"
                    "constraint MMS_EncodingProfilesSet_PK PRIMARY KEY (encodingProfilesSetKey)," 
                    "constraint MMS_EncodingProfilesSet_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, contentType, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // EncodingProfiles associated to each family (EncodingProfilesSet)
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSetMapping ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_EncodingProfilesSetMapping_PK PRIMARY KEY (encodingProfilesSetKey, encodingProfileKey), "
                    "constraint MMS_EncodingProfilesSetMapping_FK1 foreign key (encodingProfilesSetKey) "
                        "references MMS_EncodingProfilesSet (encodingProfilesSetKey) on delete cascade, "
                    "constraint MMS_EncodingProfilesSetMapping_FK2 foreign key (encodingProfileKey) "
                        "references MMS_EncodingProfile (encodingProfileKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionRoot ("
                    "ingestionRootKey           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "type                       VARCHAR (64) NOT NULL,"
                    "label                      VARCHAR (256) NULL,"
                    "metaDataContent			MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingestionDate              DATETIME NOT NULL,"
                    "lastUpdate                 TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "constraint MMS_IngestionRoot_PK PRIMARY KEY (ingestionRootKey), "
                    "constraint MMS_IngestionRoot_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionRoot_idx on MMS_IngestionRoot (workspaceKey, label)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionRoot_idx2 on MMS_IngestionRoot (workspaceKey, ingestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJob ("
                    "ingestionJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionRootKey           BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (256) NULL,"
                    "metaDataContent            TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingestionType              VARCHAR (64) NOT NULL,"
                    "startProcessing            DATETIME NULL,"
                    "endProcessing              DATETIME NULL,"
                    "downloadingProgress        DECIMAL(4,1) NULL,"
                    "uploadingProgress          DECIMAL(4,1) NULL,"
                    "sourceBinaryTransferred    INT NOT NULL,"
                    "processorMMS               VARCHAR (128) NULL,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "errorMessage               VARCHAR (1024) NULL,"
                    "constraint MMS_IngestionJob_PK PRIMARY KEY (ingestionJobKey), "
                    "constraint MMS_IngestionJob_FK foreign key (ingestionRootKey) "
                        "references MMS_IngestionRoot (ingestionRootKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJobOutput ("
                    "ingestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "physicalPathKey  			BIGINT UNSIGNED NOT NULL,"
                    "UNIQUE (ingestionJobKey, mediaItemKey, physicalPathKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJobDependency ("
                    "ingestionJobDependencyKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionJobKey  			BIGINT UNSIGNED NOT NULL,"
                    "dependOnSuccess                    TINYINT (1) NOT NULL,"
                    "dependOnIngestionJobKey            BIGINT UNSIGNED NULL,"
                    "orderNumber                        INT UNSIGNED NOT NULL,"
                    "constraint MMS_IngestionJob_PK PRIMARY KEY (ingestionJobDependencyKey), "
                    "constraint MMS_IngestionJobDependency_FK foreign key (ingestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade, "	   	        				
                    "constraint MMS_IngestionJobDependency_FK2 foreign key (dependOnIngestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_IngestionJobDependency_idx on MMS_IngestionJobDependency (ingestionJobKey)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            bool jsonTypeSupported = isJsonTypeSupported(statement);
            
            string userDataDefinition;
            if (jsonTypeSupported)
                userDataDefinition = "JSON";
            else
                userDataDefinition = "VARCHAR (512) CHARACTER SET utf8 COLLATE utf8_bin NULL";
                
            
            // workspaceKey is the owner of the content
            // IngestedRelativePath MUST start always with '/' and ends always with '/'
            // IngestedFileName and IngestedRelativePath refer the ingested content independently
            //		if it is encoded or uncompressed
            // if EncodingProfilesSet is NULL, it means the ingested content is already encoded
            // The ContentProviderKey is the entity owner of the content. For example H3G is our workspace and EMI is the ContentProvider.
            lastSQLCommand = 
                "create table if not exists MMS_MediaItem ("
                    "mediaItemKey           BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey           BIGINT UNSIGNED NOT NULL,"
                    "contentProviderKey     BIGINT UNSIGNED NOT NULL,"
                    "title                  VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingester               VARCHAR (128) NULL,"
                    "userData               " + userDataDefinition + ","
                    "deliveryFileName       VARCHAR (128) NULL,"
                    "ingestionJobKey        BIGINT UNSIGNED NOT NULL,"
                    "ingestionDate          DATETIME NOT NULL,"
                    "contentType            VARCHAR (32) NOT NULL,"
                    "startPublishing        DATETIME NOT NULL,"
                    "endPublishing          DATETIME NOT NULL,"
                    "retentionInMinutes     BIGINT UNSIGNED NOT NULL,"
                    "processorMMSForRetention	VARCHAR (128) NULL,"
                    "constraint MMS_MediaItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_MediaItem_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "constraint MMS_MediaItem_FK2 foreign key (contentProviderKey) "
                        "references MMS_ContentProvider (contentProviderKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx2 on MMS_MediaItem (contentType, ingestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx3 on MMS_MediaItem (contentType, startPublishing, endPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItem_idx4 on MMS_MediaItem (contentType, title)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Tag ("
                    "tagKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey		BIGINT UNSIGNED NOT NULL,"
                    "name			VARCHAR (256) NOT NULL,"
                    "constraint MMS_Tag_PK PRIMARY KEY (tagKey), "
                    "constraint MMS_Tag_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // we cannot have two equal UniqueNames within the same workspace
            // we can have two equal UniqueNames on two different workspaces
            lastSQLCommand = 
                "create table if not exists MMS_ExternalUniqueName ("
                    "workspaceKey			BIGINT UNSIGNED NOT NULL,"
                    "uniqueName      		VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "mediaItemKey  			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_ExternalUniqueName_PK PRIMARY KEY (workspaceKey, uniqueName), "
                    "constraint MMS_ExternalUniqueName_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "constraint MMS_ExternalUniqueName_FK2 foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_ExternalUniqueName_idx on MMS_ExternalUniqueName (workspaceKey, mediaItemKey)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // DRM. 0: NO DRM, 1: YES DRM
            // EncodedFileName and EncodedRelativePath are NULL only if the content is un-compressed.
            //  EncodedRelativePath MUST start always with '/' and ends always with '/'
            // EncodingProfileKey will be NULL only in case of
            //      - an un-compressed video or audio
            //      - an Application
            // MMSPartitionNumber. -1: live partition, >= 0: partition for any other content
            // IsAlias (0: false): it is used for a PhysicalPath that is an alias and
            //  it really refers another existing PhysicalPath. It was introduced to manage the XLE live profile
            //  supporting really multi profiles: rtsp, hls, adobe. So for every different profiles, we will
            //  create just an alias
            lastSQLCommand = 
                "create table if not exists MMS_PhysicalPath ("
                    "physicalPathKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "drm	             		TINYINT NOT NULL,"
                    "fileName				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "relativePath			VARCHAR (256) NOT NULL,"
                    "partitionNumber			INT NULL,"
                    "sizeInBytes			BIGINT UNSIGNED NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NULL,"
                    "isAlias				INT NOT NULL DEFAULT 0,"
                    "creationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint MMS_PhysicalPath_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_PhysicalPath_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_PhysicalPath_FK2 foreign key (encodingProfileKey) "
                        "references MMS_EncodingProfile (encodingProfileKey), "
                    "UNIQUE (mediaItemKey, relativePath, fileName, isAlias), "
                    "UNIQUE (mediaItemKey, encodingProfileKey)) "	// it is not possible to have the same content using the same encoding profile key
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPath_idx2 on MMS_PhysicalPath (mediaItemKey, physicalPathKey, encodingProfileKey, partitionNumber)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPath_idx3 on MMS_PhysicalPath (relativePath, fileName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_VideoItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "durationInMilliSeconds		BIGINT NULL,"
                    "bitRate            		INT NULL,"
                    "width              		INT NULL,"
                    "height             		INT NULL,"
                    "avgFrameRate			VARCHAR (64) NULL,"
                    "videoCodecName			VARCHAR (64) NULL,"
                    "videoBitRate             		INT NULL,"
                    "videoProfile			VARCHAR (128) NULL,"
                    "audioCodecName			VARCHAR (64) NULL,"
                    "audioBitRate             		INT NULL,"
                    "audioSampleRate             	INT NULL,"
                    "audioChannels             		INT NULL,"
                    "constraint MMS_VideoItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_VideoItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_AudioItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "durationInMilliSeconds		BIGINT NULL,"
                    "codecName          		VARCHAR (64) NULL,"
                    "bitRate             		INT NULL,"
                    "sampleRate                  	INT NULL,"
                    "channels             		INT NULL,"
                    "constraint MMS_AudioItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_AudioItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ImageItemProfile ("
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "width				INT NOT NULL,"
                    "height				INT NOT NULL,"
                    "format                       	VARCHAR (64) NULL,"
                    "quality				INT NOT NULL,"
                    "constraint MMS_ImageItemProfile_PK PRIMARY KEY (physicalPathKey), "
                    "constraint MMS_ImageItemProfile_FK foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_YouTube ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "refreshToken               VARCHAR (128) NOT NULL,"
                    "constraint MMS_Conf_YouTube_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_YouTube_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_Facebook ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "pageToken                  VARCHAR (256) NOT NULL,"
                    "constraint MMS_Conf_Facebook_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_Facebook_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_LiveURL ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "liveURL					VARCHAR (128) NOT NULL,"
                    "constraint MMS_Conf_LiveURL_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_LiveURL_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_FTP ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "server						VARCHAR (64) NOT NULL,"
                    "port						INT UNSIGNED NOT NULL,"
                    "userName					VARCHAR (64) NOT NULL,"
                    "password					VARCHAR (64) NOT NULL,"
                    "remoteDirectory			VARCHAR (256) NOT NULL,"
                    "constraint MMS_Conf_FTP_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_FTP_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Conf_EMail ("
                    "confKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "label                      VARCHAR (128) NOT NULL,"
                    "address					VARCHAR (256) NOT NULL,"
                    "subject					VARCHAR (128) NOT NULL,"
                    "message					VARCHAR (1024) NOT NULL,"
                    "constraint MMS_Conf_EMail_PK PRIMARY KEY (confKey), "
                    "constraint MMS_Conf_EMail_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "UNIQUE (workspaceKey, label)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        /*
        try
        {
            // Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
            lastSQLCommand = 
                "create table if not exists MMS_DefaultTerritoryInfo ("
                    "defaultTerritoryInfoKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "territoryKey				BIGINT UNSIGNED NOT NULL,"
                    "startPublishing				DATETIME NOT NULL,"
                    "endPublishing				DATETIME NOT NULL,"
                    "constraint MMS_DefaultTerritoryInfo_PK PRIMARY KEY (defaultTerritoryInfoKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_DefaultTerritoryInfo_FK2 foreign key (territoryKey) "
                        "references MMS_Territory (territoryKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, territoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */
        
        /*
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create table if not exists MMS_Publishing ("
                    "publishingKey                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey                   BIGINT UNSIGNED NOT NULL,"
                    "territoryKey                   BIGINT UNSIGNED NOT NULL,"
                    "startPublishing                DATETIME NOT NULL,"
                    "endPublishing                  DATETIME NOT NULL,"
                    "publishingStatus               TINYINT (1) NOT NULL,"
                    "processorMMS                   VARCHAR (128) NULL,"
                    "constraint MMS_Publishing_PK PRIMARY KEY (publishingKey), "
                    "constraint MMS_Publishing_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_Publishing_FK2 foreign key (territoryKey) "
                        "references MMS_Territory (territoryKey) on delete cascade, "
                    "UNIQUE (mediaItemKey, territoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx2 on MMS_Publishing (mediaItemKey, startPublishing, endPublishing, publishingStatus)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx3 on MMS_Publishing (publishingStatus, startPublishing, endPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing_idx4 on MMS_Publishing (publishingStatus, endPublishing, startPublishing)";            
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        */

        try
        {
            // The MMS_EncodingJob table include all the contents that have to be encoded
            //  RelativePath: it is the relative path of the original uncompressed file name
            //  PhysicalPathKey: it is the physical path key of the original uncompressed file name
            //  The ContentType was added just to avoid a big join to retrieve this info
            //  ProcessorMMS is the MMSEngine processing the encoding
            //  Status.
            //      0: TOBEPROCESSED
            //      1: PROCESSING
            //      2: SUCCESS (PROCESSED)
            //      3: FAILED
            //  EncodingPriority:
            //      0: low
            //      1: default
            //      2: high
            lastSQLCommand = 
                "create table if not exists MMS_EncodingJob ("
                    "encodingJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "type                       VARCHAR (64) NOT NULL,"
                    "parameters                 LONGTEXT NOT NULL,"
                    "encodingPriority			TINYINT NOT NULL,"
                    "encodingJobStart			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "encodingJobEnd             DATETIME NULL,"
                    "encodingProgress           INT NULL,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "processorMMS               VARCHAR (128) NULL,"
                    "transcoder					VARCHAR (128) NULL,"
                    "failuresNumber           	INT NOT NULL,"
                    "constraint MMS_EncodingJob_PK PRIMARY KEY (encodingJobKey), "
                    "constraint MMS_EncodingJob_FK foreign key (ingestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important because it will be used by the query looking every 15 seconds if there are
            // contents to be encoded
            lastSQLCommand = 
                "create index MMS_EncodingJob_idx2 on MMS_EncodingJob (status, processorMMS, failuresNumber, encodingJobStart)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
    
        try
        {
            // create table MMS_RequestsAuthorization
            // MediaItemKey or ExternalKey cannot be both null
            // DeliveryMethod:
            //    0: download
            //    1: 3gpp streaming
            //    2: RTMP Flash Streaming
            //    3: WindowsMedia Streaming
            // SwitchingType: 0: None, 1: FCS, 2: FTS
            // NetworkCoverage. 0: 2G, 1: 2.5G, 2: 3G
            // IngestedPathName: [<live prefix>]/<customer name>/<territory name>/<relative path>/<content name>
            // ToBeContinued. 0 or 1
            // ForceHTTPRedirection. 0: HTML page, 1: HTTP Redirection
            // TimeToLive is measured in seconds
            lastSQLCommand = 
                "create table if not exists MMS_DeliveryAuthorization ("
                    "deliveryAuthorizationKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "userKey    				BIGINT UNSIGNED NOT NULL,"
                    "clientIPAddress			VARCHAR (16) NULL,"
                    "physicalPathKey			BIGINT UNSIGNED NOT NULL,"
                    "deliveryURI    			VARCHAR (1024) NOT NULL,"
                    "ttlInSeconds               INT NOT NULL,"
                    "currentRetriesNumber       INT NOT NULL,"
                    "maxRetries                 INT NOT NULL,"
                    "authorizationTimestamp		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint MMS_DeliveryAuthorization_PK PRIMARY KEY (deliveryAuthorizationKey), "
                    "constraint MMS_DeliveryAuthorization_FK foreign key (userKey) "
                        "references MMS_User (userKey) on delete cascade, "
                    "constraint MMS_DeliveryAuthorization_FK2 foreign key (physicalPathKey) "
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
    /*
    # create table MMS_HTTPSessions
    # One session is per userKey and UserAgent
    create table if not exists MMS_HTTPSessions (
            HTTPSessionKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            userKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ExpirationDate				DATETIME NOT NULL,
            constraint MMS_HTTPSessions_PK PRIMARY KEY (HTTPSessionKey), 
            constraint MMS_HTTPSessions_FK foreign key (userKey) 
                    references MMS_User (userKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_HTTPSessions_idx on MMS_HTTPSessions (userKey, UserAgent);

    # create table MMS_ReportsConfiguration
    # Type. 0: Billing Statistics, 1: Content Access, 2: Active Users,
    #		3: Streaming Statistics, 4: Usage (how to call the one in XHP today?)
    # Period. 0: Hourly, 1: Daily, 2: Weekly, 3: Monthly, 4: Yearly
    # Format. 0: CSV, 1: HTML
    # EmailAddresses. List of email addresses separated by ‘;’
    create table if not exists MMS_ReportsConfiguration (
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            customerKey             	BIGINT UNSIGNED NOT NULL,
            Type						INT NOT NULL,
            Period						INT NOT NULL,
            TimeOfDay					INT NOT NULL,
            Format						INT NOT NULL,
            EmailAddresses				VARCHAR (1024) NULL,
            constraint MMS_ReportsConfiguration_PK PRIMARY KEY (ReportConfigurationKey), 
            constraint MMS_ReportsConfiguration_FK foreign key (customerKey) 
                    references MMS_Workspaces (customerKey) on delete cascade, 
            UNIQUE (customerKey, Type, Period)) 
            ENGINE=InnoDB;

    # create table MMS_ReportURLCategory
    create table if not exists MMS_ReportURLCategory (
            ReportURLCategoryKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name             			VARCHAR (128) NOT NULL,
            URLsPattern				VARCHAR (512) NOT NULL,
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_ReportURLCategory_PK PRIMARY KEY (ReportURLCategoryKey), 
            constraint MMS_ReportURLCategory_FK foreign key (ReportConfigurationKey) 
                    references MMS_ReportsConfiguration (ReportConfigurationKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_CustomersSharable
    create table if not exists MMS_CustomersSharable (
            customerKeyOwner			BIGINT UNSIGNED NOT NULL,
            CustomerKeySharable		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CustomersSharable_PK PRIMARY KEY (CustomerKeyOwner, CustomerKeySharable), 
            constraint MMS_CustomersSharable_FK1 foreign key (CustomerKeyOwner) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomersSharable_FK2 foreign key (CustomerKeySharable) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table Handsets
    # It represent a family of handsets
    # Description is something like: +H.264, +enh-aac-plus, 
    # FamilyType: 0: Delivery, 1: Music/Presentation (used by MMS Application Images)
    # SupportedDelivery: see above the definition for iSupportedDelivery_*
    create table if not exists MMS_HandsetsFamilies (
            HandsetFamilyKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Description				VARCHAR (128) NOT NULL,
            FamilyType					INT NOT NULL,
            SupportedDelivery			INT NOT NULL DEFAULT 3,
            SmallSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            MediumSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            SmallIconProfileKey		BIGINT UNSIGNED NULL,
            MediumIconProfileKey		BIGINT UNSIGNED NULL,
            LargeIconProfileKey		BIGINT UNSIGNED NULL,
            constraint MMS_HandsetsFamilies_PK PRIMARY KEY (HandsetFamilyKey)) 
            ENGINE=InnoDB;

    # create table Handsets
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    # HTTPRedirectionForRTSP. 0 if supported, 0 if not supported
    # DRMMethod. 0: no DRM, 1: oma1forwardlock, 2: cfm, 3: cfm+
    # If HandsetFamilyKey is NULL, it means the handset is not connected to his family
    # SupportedNetworkCoverage. NULL: no specified, 0: 2G, 1: 2.5G, 2: 3G
    create table if not exists MMS_Handsets (
            HandsetKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetFamilyKey			BIGINT UNSIGNED NULL,
            MusicHandsetFamilyKey		BIGINT UNSIGNED NULL,
            Brand						VARCHAR (32) NOT NULL,
            Model						VARCHAR (32) NOT NULL,
            Alias						VARCHAR (32) NULL,
            OperativeSystem			VARCHAR (32) NOT NULL,
            HTTPRedirectionForRTSP		TINYINT (1) NOT NULL,
            DRMMethod					TINYINT NOT NULL,
            ScreenWidth				INT NOT NULL,
            ScreenHeight				INT NOT NULL,
            ScreenDensity				INT NULL,
            SupportedNetworkCoverage	INT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_Handsets_PK PRIMARY KEY (HandsetKey), 
            constraint MMS_Handsets_FK foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            constraint MMS_Handsets_FK2 foreign key (MusicHandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            UNIQUE (Brand, Model)) 
            ENGINE=InnoDB;

    # create table UserAgents
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    create table if not exists MMS_UserAgents (
            UserAgentKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_UserAgents_PK PRIMARY KEY (UserAgentKey), 
            constraint MMS_UserAgents_FK foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey) on delete cascade, 
            UNIQUE (UserAgent)) 
            ENGINE=InnoDB;

    # create table HandsetsProfilesMapping
    # This table perform a mapping between (HandsetKey, NetworkCoverage) with EncodingProfileKey and a specified Priority
    # NetworkCoverage: 0: 2G, 1: 2.5G, 2: 3G.
    # If CustomerKey is NULL, it means the mapping is the default mapping
    # Priority: 1 (the best), 2, runtime_error e
    create table if not exists MMS_HandsetsProfilesMapping (
            HandsetProfileMappingKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey  				BIGINT UNSIGNED NULL,
            ContentType                TINYINT NOT NULL,
            HandsetFamilyKey			BIGINT UNSIGNED NOT NULL,
            NetworkCoverage			TINYINT NOT NULL,
            EncodingProfileKey			BIGINT UNSIGNED NOT NULL,
            Priority					INT NOT NULL,
            constraint MMS_HandsetsProfilesMapping_PK primary key (HandsetProfileMappingKey), 
            constraint MMS_HandsetsProfilesMapping_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK2 foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK3 foreign key (EncodingProfileKey) 
                    references MMS_EncodingProfiles (encodingProfileKey), 
            UNIQUE (CustomerKey, ContentType, HandsetFamilyKey, NetworkCoverage, EncodingProfileKey, Priority)) 
            ENGINE=InnoDB;


    # create table MMS_GenresTranslation
    create table if not exists MMS_GenresTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_GenresTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_GenresTranslation_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_GenresTranslation_idx on MMS_GenresTranslation (GenreKey, Field, languageCode);


    # create table MMS_MediaItemsTranslation
    create table if not exists MMS_MediaItemsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_MediaItemsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_MediaItemsTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_MediaItemsTranslation_idx on MMS_MediaItemsTranslation (MediaItemKey, Field, languageCode);

    # create table MMS_GenresMediaItemsMapping
    create table if not exists MMS_GenresMediaItemsMapping (
            GenresMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            constraint MMS_GenresMediaItemsMapping_PK PRIMARY KEY (GenresMediaItemsMappingKey), 
            constraint MMS_GenresMediaItemsMapping_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade, 
            constraint MMS_GenresMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (GenreKey, MediaItemKey))
            ENGINE=InnoDB;

    # create table MMS_MediaItemsCustomerMapping
    # customerType could be 0 (Owner of the content) or 1 (User of the shared content)
    # MMS_MediaItemsCustomerMapping table will contain one row for the Customer Ownerof the content and one row for each shared content
    create table if not exists MMS_MediaItemsCustomerMapping (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            customerType				TINYINT NOT NULL,
            constraint MMS_MediaItemsCustomerMapping_PK PRIMARY KEY (MediaItemKey, CustomerKey), 
            constraint MMS_MediaItemsCustomerMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsCustomerMapping_FK2 foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table MediaItemsRemoved
    create table if not exists MMS_MediaItemsRemoved (
            MediaItemRemovedKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            DeletionDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            MediaItemKey  				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            ContentProviderKey			BIGINT UNSIGNED NOT NULL,
            DisplayName				VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Ingester					VARCHAR (128) NULL,
            Description				TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Country					VARCHAR (32) NULL,
            IngestionDate				TIMESTAMP,
            ContentType                TINYINT NOT NULL,
            LogicalType				VARCHAR (32) NULL,
            IngestedFileName			VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            IngestedRelativePath		VARCHAR (256) NULL, 
            constraint MMS_MediaItemsRemoved_PK PRIMARY KEY (MediaItemRemovedKey)) 
            ENGINE=InnoDB;

    # create table MMS_CrossReferences
    # This table will be used to set cross references between MidiaItems
    # Type could be:
    #	0: <not specified>
    #	1: IsScreenshotOfVideo
    #	2: IsImageOfAlbum
    create table if not exists MMS_CrossReferences (
            SourceMediaItemKey		BIGINT UNSIGNED NOT NULL,
            Type					TINYINT NOT NULL,
            TargetMediaItemKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CrossReferences_PK PRIMARY KEY (SourceMediaItemKey, TargetMediaItemKey), 
            constraint MMS_CrossReferences_FK1 foreign key (SourceMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_CrossReferences_FK2 foreign key (TargetMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_CrossReferences_idx on MMS_CrossReferences (SourceMediaItemKey, TargetMediaItemKey);

    # create table MMS_3SWESubscriptions
    # This table will be used to set cross references between MidiaItems
    create table if not exists MMS_3SWESubscriptions (
            ThreeSWESubscriptionKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name						VARCHAR (64) NOT NULL,
            constraint MMS_3SWESubscriptions_PK PRIMARY KEY (ThreeSWESubscriptionKey), 
            UNIQUE (name)) 
            ENGINE=InnoDB;

    # create table MMS_3SWESubscriptionsMapping
    # This table will be used to specified the contents to be added in an HTML presentation
    create table if not exists MMS_3SWESubscriptionsMapping (
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ThreeSWESubscriptionKey	BIGINT UNSIGNED NOT NULL,
            constraint MMS_3SWESubscriptionsMapping_PK PRIMARY KEY (MediaItemKey, ThreeSWESubscriptionKey), 
            constraint MMS_3SWESubscriptionsMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_3SWESubscriptionsMapping_FK2 foreign key (ThreeSWESubscriptionKey) 
                    references MMS_3SWESubscriptions (ThreeSWESubscriptionKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_3SWESubscriptionsMapping_idx on MMS_3SWESubscriptionsMapping (MediaItemKey, ThreeSWESubscriptionKey);

    # create table MMS_3SWEMoreChargingInfo
    # This table include the information included into the Billing definition and
    # missing in ChargingInfo table
    create table if not exists MMS_3SWEMoreChargingInfo (
            ChargingKey				BIGINT UNSIGNED NOT NULL,
            AssetType					VARCHAR (16) NOT NULL,
            AmountTax					INT NOT NULL,
            PartnerID					VARCHAR (32) NOT NULL,
            Category					VARCHAR (32) NOT NULL,
            RetailAmount				INT NOT NULL,
            RetailAmountTax			INT NOT NULL,
            RetailAmountWithSub		INT NOT NULL,
            RetailAmountTaxWithSub		INT NOT NULL,
            constraint MMS_3SWEMoreChargingInfo_PK PRIMARY KEY (ChargingKey), 
            constraint MMS_3SWEMoreChargingInfo_FK foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_Advertisements
    # territoryKey: if NULL the ads is valid for any territory
    # Type:
    #		0: pre-roll
    #		1: post-roll
    create table if not exists MMS_Advertisements (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            territoryKey				BIGINT UNSIGNED NULL,
            name						VARCHAR (32) NOT NULL,
            ContentType				TINYINT NOT NULL,
            isEnabled	                TINYINT (1) NOT NULL,
            Type						TINYINT NOT NULL,
            ValidityStart				TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            ValidityEnd				TIMESTAMP NOT NULL,
            constraint MMS_Advertisements_PK PRIMARY KEY (AdvertisementKey), 
            constraint MMS_Advertisements_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_Advertisements_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Advertisements_idx on MMS_Advertisements (CustomerKey, territoryKey, name);

    # create table MMS_AdvertisementAdvertisings
    create table if not exists MMS_Advertisement_Ads (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Ads_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Ads_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Ads_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_AdvertisementContents
    create table if not exists MMS_Advertisement_Contents (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Contents_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Contents_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Contents_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;



    # create table MMS_RequestsStatistics
    # Status:
    #	- 0: Received
    #	- 1: Failed (final status)
    #	- 2: redirected (final status)
    create table if not exists MMS_RequestsStatistics (
            RequestStatisticKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Status           			TINYINT (2) NOT NULL,
            ErrorMessage				VARCHAR (512) NULL,
            RedirectionURL				VARCHAR (256) NULL,
            RequestAuthorizationKey	BIGINT UNSIGNED NULL,
            CustomerKey				BIGINT UNSIGNED NULL,
            UserAgent					VARCHAR (512) NULL,
            HandsetKey					BIGINT UNSIGNED NULL,
            PartyID					VARCHAR (64) NULL,
            MSISDN						VARCHAR (32) NULL,
            PhysicalPathKey			BIGINT UNSIGNED NULL,
            AuthorizationKey			BIGINT UNSIGNED NULL,
            RequestReceivedTimestamp	TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ElapsedSecondsToManageTheRequest	INT NULL,
            constraint MMS_RequestsStatistics_PK PRIMARY KEY (RequestStatisticKey)) 
            ENGINE=InnoDB;
    create index MMS_RequestsStatistics_idx2 on MMS_RequestsStatistics (AuthorizationKey);



    # create table MMS_MediaItemsPublishing
    # In this table it is considered the publishing 'per content'.
    # In MMS_MediaItemsPublishing, a content is considered published if all his profiles are published.
    create table if not exists MMS_MediaItemsPublishing (
            MediaItemPublishingKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            territoryKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_MediaItemsPublishing_PK PRIMARY KEY (territoryKey, MediaItemKey), 
            constraint MMS_MediaItemsPublishing_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsPublishing_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade)
            ENGINE=InnoDB;

    // Done by a Zoli music SQL script:
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD COLUMN AccessCount INT NOT NULL DEFAULT 0,
    //	ADD COLUMN Popularity DECIMAL(12, 2) NOT NULL DEFAULT 0,
    //	ADD COLUMN LastAccess DATETIME NOT NULL DEFAULT 0;
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD KEY idx_AccessCount (territoryKey, AccessCount),
    //	ADD KEY idx_Popularity (territoryKey, Popularity),
    //	ADD KEY idx_LastAccess (territoryKey, LastAccess);


    # create table MediaItemsBillingInfo
    # Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
    create table if not exists MMS_MediaItemsBillingInfo (
            MediaItemsBillingInfoKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            territoryKey				BIGINT UNSIGNED NOT NULL,
            ChargingKey1				BIGINT UNSIGNED NOT NULL,
            ChargingKey2				BIGINT UNSIGNED NOT NULL,
            ReserveCredit				TINYINT (1) NULL,
            ExternalBillingName		VARCHAR (64) NULL,
            MaxRetries					INT NOT NULL,
            TTLInSeconds				INT NOT NULL,
            constraint MMS_MediaItemsBillingInfo_PK PRIMARY KEY (MediaItemsBillingInfoKey), 
            constraint MMS_MediaItemsBillingInfo_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPath (physicalPathKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK2 foreign key (territoryKey) 
                    references MMS_Territory (territoryKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK3 foreign key (ChargingKey1) 
                    references ChargingInfo (ChargingKey), 
            constraint MMS_MediaItemsBillingInfo_FK4 foreign key (ChargingKey2) 
                    references ChargingInfo (ChargingKey), 
            UNIQUE (PhysicalPathKey, DeliveryMethod, territoryKey)) 
            ENGINE=InnoDB;


    # create table MMS_AllowedDeliveryMethods
    create table if not exists MMS_AllowedDeliveryMethods (
            ContentType				TINYINT NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            constraint MMS_AllowedDeliveryMethods_PK PRIMARY KEY (ContentType, DeliveryMethod)) 
            ENGINE=InnoDB;


    # create table MMS_Artists
    create table if not exists MMS_Artists (
            ArtistKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            name						VARCHAR (255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Country					VARCHAR (128) NULL,
            HomePageURL				VARCHAR (256) NULL,
            constraint MMS_Artists_PK PRIMARY KEY (ArtistKey), 
            UNIQUE (name)) 
            ENGINE=InnoDB;


    # create table MMS_ArtistsTranslation
    create table if not exists MMS_ArtistsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_ArtistsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_ArtistsTranslation_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_ArtistsTranslation_idx on MMS_ArtistsTranslation (ArtistKey, Field, languageCode);

    # create table MMS_CustomerCatalogMoreInfo
    # GlobalHomePage: 0 or 1 (it specifies if his home page has to be the global one or his private home page)
    # IsPublic: 0 or 1
    create table if not exists MMS_CustomerCatalogMoreInfo (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            GlobalHomePage				INT NOT NULL,
            IsPublic					INT NOT NULL,
            CatalogImageMediaItemKey	BIGINT UNSIGNED NULL,
            HeaderIsEnabled_0					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_0		INT UNSIGNED NULL,
            HeaderFunctionParameters_0			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_0			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_1					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_1		INT UNSIGNED NULL,
            HeaderFunctionParameters_1			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_2					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_2		INT UNSIGNED NULL,
            HeaderFunctionParameters_2			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_3					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_3		INT UNSIGNED NULL,
            HeaderFunctionParameters_3			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_4					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_4		INT UNSIGNED NULL,
            HeaderFunctionParameters_4			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            constraint MMS_CustomerCatalogMoreInfo_PK PRIMARY KEY (CustomerKey), 
            constraint MMS_CustomerCatalogMoreInfo_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomerCatalogMoreInfo_FK2 foreign key (CatalogImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PresentationWorkspaces
    # name: if NULL, it is the Production Workspace
    create table if not exists MMS_PresentationWorkspaces (
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            name						VARCHAR (128) NULL,
            constraint MMS_PresentationWorkspaces_PK PRIMARY KEY (PresentationWorkspaceKey), 
            constraint MMS_PresentationWorkspaces_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationWorkspaces_idx on MMS_PresentationWorkspaces (CustomerKey, name);

    # create table MMS_PresentationItems
    # PresentationItemType: see PresentationItemType in MMSClient.h
    # NodeType:
    #	0: internal (no root type)
    #	1: MainRoot
    #	2: Root_1
    #	3: Root_2
    #	4: Root_3
    #	5: Root_4
    create table if not exists MMS_PresentationItems (
            PresentationItemKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL,
            ParentPresentationItemKey	BIGINT UNSIGNED NULL,
            NodeType					INT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            PresentationItemType		INT UNSIGNED NOT NULL,
            ImageMediaItemKey			BIGINT UNSIGNED NULL,
            Title						VARCHAR (128) NULL,
            SubTitle					VARCHAR (256) NULL,
            Parameter					VARCHAR (256) NULL,
            PositionIndex				INT UNSIGNED NOT NULL,
            ToolbarIsEnabled_1					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_1		INT UNSIGNED NULL,
            ToolbarFunctionParameters_1			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_2					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_2		INT UNSIGNED NULL,
            ToolbarFunctionParameters_2			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_3					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_3		INT UNSIGNED NULL,
            ToolbarFunctionParameters_3			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_4					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_4		INT UNSIGNED NULL,
            ToolbarFunctionParameters_4			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_5					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_5		INT UNSIGNED NULL,
            ToolbarFunctionParameters_5			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_5			BIGINT UNSIGNED NULL,
            constraint MMS_PresentationItems_PK PRIMARY KEY (PresentationItemKey), 
            constraint MMS_PresentationItems_FK foreign key (PresentationWorkspaceKey) 
                    references MMS_PresentationWorkspaces (PresentationWorkspaceKey) on delete cascade, 
            constraint MMS_PresentationItems_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PresentationItems_FK3 foreign key (ImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationItems_idx on MMS_PresentationItems (ParentPresentationItemKey, PositionIndex);

    # create table MMS_Albums
    # AlbumKey: it is the PlaylistMediaItemKey
    create table if not exists MMS_Albums (
            AlbumKey  					BIGINT UNSIGNED NOT NULL,
            Version					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            UPC						VARCHAR (32) NOT NULL,
            Type						VARCHAR (32) NULL,
            ReleaseDate				DATETIME NULL,
            Title						VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            LabelName					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Supplier					VARCHAR (64) NULL,
            SubSupplier				VARCHAR (64) NULL,
            PCopyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Copyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Explicit					VARCHAR (16) NULL,
            constraint MMS_Albums_PK PRIMARY KEY (AlbumKey), 
            constraint MMS_Albums_FK foreign key (AlbumKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Albums_idx2 on MMS_Albums (Supplier, UPC);

    # create table MMS_ArtistsMediaItemsMapping
    # Role:
    #	- 'NOROLE' if not present in the XML
    #	- 'MAINARTIST' if compareToIgnoreCase(main artist)
    #	- 'FEATURINGARTIST' if compareToIgnoreCase(featuring artist)
    #	- any other string in upper case without any space
    create table if not exists MMS_ArtistsMediaItemsMapping (
            ArtistsMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            Role							VARCHAR (128) NOT NULL,
            constraint MMS_ArtistsMediaItemsMapping_PK PRIMARY KEY (ArtistsMediaItemsMappingKey), 
            constraint MMS_ArtistsMediaItemsMapping_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade, 
            constraint MMS_ArtistsMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (ArtistKey, MediaItemKey, Role))
            ENGINE=InnoDB;
    create index MMS_ArtistsMediaItemsMapping_idx on MMS_ArtistsMediaItemsMapping (ArtistKey, Role, MediaItemKey);
    create index MMS_ArtistsMediaItemsMapping_idx2 on MMS_ArtistsMediaItemsMapping (MediaItemKey, Role);

    # create table MMS_ISRCMapping
    # SourceISRC: VideoItem or Ringtone -----> TargetISRC: AudioItem (Track)
    create table if not exists MMS_ISRCMapping (
            SourceISRC					VARCHAR (32) NOT NULL,
            TargetISRC					VARCHAR (32) NOT NULL,
            constraint MMS_ISRCMapping_PK PRIMARY KEY (SourceISRC, TargetISRC)) 
            ENGINE=InnoDB;
    create unique index MMS_ISRCMapping_idx on MMS_ISRCMapping (TargetISRC, SourceISRC);


    # create table MMS_SubTitlesTranslation
    create table if not exists MMS_SubTitlesTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            languageCode				VARCHAR (16) NOT NULL,
            Translation				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_SubTitlesTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_SubTitlesTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_SubTitlesTranslation_idx on MMS_SubTitlesTranslation (MediaItemKey, Field, languageCode);


    # create table ApplicationItems
    create table if not exists MMS_ApplicationItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            constraint MMS_ApplicationItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_ApplicationItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_ApplicationHandsetsMapping
    create table if not exists MMS_ApplicationHandsetsMapping (
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            JadFile					TEXT NULL,
            constraint MMS_ApplicationHandsetsMapping_PK PRIMARY KEY (PhysicalPathKey, HandsetKey), 
            constraint MMS_ApplicationHandsetsMapping_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPath (physicalPathKey) on delete cascade, 
            constraint MMS_ApplicationHandsetsMapping_FK2 foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey)) 
            ENGINE=InnoDB;
    create index MMS_ApplicationHandsetsMapping_idx on MMS_ApplicationHandsetsMapping (PhysicalPathKey, HandsetKey);

    # create table PlaylistItems
    # ClipType. 0 (iContentType_Video): video, 1 (iContentType_Audio): audio
    # ScheduledStartTime: used only in case of Linear Playlist (playlist of clips plus the scheduled_start_time field)
    create table if not exists MMS_PlaylistItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ClipType					INT NOT NULL,
            ScheduledStartTime			DATETIME NULL,
            constraint MMS_PlaylistItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_PlaylistItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PlaylistClips
    # if ClipMediaItemKey is null it means the playlist item is a live and the LiveChannelURL will be initialized
    # LiveType. NULL: ClipMediaItemKey is initialized, 0: live from XAC/XLE, 1: live from the SDP file
    # ClipDuration: duration of the clip in milliseconds (NULL in case of live)
    # Seekable: 0 or 1 (NULL in case of live)
    create table if not exists MMS_PlaylistClips (
            PlaylistClipKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PlaylistMediaItemKey		BIGINT UNSIGNED NOT NULL,
            ClipMediaItemKey			BIGINT UNSIGNED NULL,
            ClipSequence           	INT NOT NULL,
            ClipDuration           	BIGINT NULL,
            Seekable           		TINYINT NULL,
            WaitingProfileSince		DATETIME NULL,
            constraint MMS_PlaylistClips_PK PRIMARY KEY (PlaylistClipKey), 
            constraint MMS_PlaylistClips_FK foreign key (PlaylistMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PlaylistClips_FK2 foreign key (ClipMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey)) 
            ENGINE=InnoDB;
    create unique index MMS_PlaylistClips_idx2 on MMS_PlaylistClips (PlaylistMediaItemKey, ClipSequence);

    # create table LiveItems
    # FeedType. 0: video, 1: audio
    create table if not exists MMS_LiveItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            FeedType					INT NOT NULL,
            constraint MMS_LiveItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_LiveItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table MimeTypes
    # HandsetBrandPattern, HandsetModelPattern and HandsetOperativeSystem must be all different from null or all equal to null
    #		we cannot have i.e. HandsetBrandPattern == null and HandsetModelPattern != null
    create table if not exists MMS_MimeTypes (
            MimeTypeKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MimeType					VARCHAR (64) NOT NULL,
            ContentType                TINYINT NOT NULL,
            HandsetBrandPattern		VARCHAR (32) NULL,
            HandsetModelPattern		VARCHAR (32) NULL,
            HandsetOperativeSystem		VARCHAR (32) NULL,
            EncodingProfileNamePattern	VARCHAR (64) NOT NULL,
            Description             	VARCHAR (64) NULL,
            constraint MMS_MimeTypes_PK PRIMARY KEY (MimeTypeKey)) 
            ENGINE=InnoDB;
         */

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", se.what(): " + se.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
        }
    }    
}

bool MMSEngineDBFacade::isRealDBError(string exceptionMessage)
{        
    transform(
            exceptionMessage.begin(), 
            exceptionMessage.end(), 
            exceptionMessage.begin(), 
            [](unsigned char c){return tolower(c); } );

    if (exceptionMessage.find("already exists") == string::npos &&            // error (table) on mysql
            exceptionMessage.find("duplicate key name") == string::npos    // error (index) on mysql
            )
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool MMSEngineDBFacade::isJsonTypeSupported(shared_ptr<sql::Statement> statement)
{        
    bool jsonTypeSupported = true;

    try
    {
        string lastSQLCommand = 
            "create table if not exists MMS_JsonTest ("
                "userData               JSON) "
                "ENGINE=InnoDB";
        statement->execute(lastSQLCommand);
        
        lastSQLCommand = 
            "drop table MMS_JsonTest";
        statement->execute(lastSQLCommand);
    }
    catch(sql::SQLException se)
    {
        jsonTypeSupported = false;
    }
    
    return jsonTypeSupported;
}
