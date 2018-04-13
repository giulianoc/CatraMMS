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
#include "MMSEngineDBFacade.h"

// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{
    _logger     = logger;
    
    _defaultContentProviderName     = "default";
    _defaultTerritoryName           = "default";
    
    size_t dbPoolSize = configuration["database"].get("poolSize", 5).asInt();
    string dbServer = configuration["database"].get("server", "XXX").asString();
    /*
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
     */
    string dbUsername = configuration["database"].get("userName", "XXX").asString();
    string dbPassword = configuration["database"].get("password", "XXX").asString();
    string dbName = configuration["database"].get("dbName", "XXX").asString();

    _maxEncodingFailures            = configuration["encoding"].get("maxEncodingFailures", 3).asInt();
    
    _confirmationCodeRetentionInDays    = configuration["mms"].get("confirmationCodeRetentionInDays", 3).asInt();
    
    shared_ptr<MySQLConnectionFactory>  mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName);
        
    _connectionPool = make_shared<DBConnectionPool<MySQLConnection>>(dbPoolSize, mySQLConnectionFactory);

    createTablesIfNeeded();
}

MMSEngineDBFacade::~MMSEngineDBFacade() 
{
}

/*
vector<shared_ptr<Customer>> MMSEngineDBFacade::getCustomers()
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select customerKey, name, directoryName, maxStorageInGB, maxEncodingPriority from MMS_Customer where isEnabled = 1 and customerType in (1, 2)";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    vector<shared_ptr<Customer>>    customers;
    
    while (resultSet->next())
    {
        shared_ptr<Customer>    customer = make_shared<Customer>();
        
        customers.push_back(customer);
        
        customer->_customerKey = resultSet->getInt("customerKey");
        customer->_name = resultSet->getString("name");
        customer->_directoryName = resultSet->getString("directoryName");
        customer->_maxStorageInGB = resultSet->getInt("maxStorageInGB");
        customer->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("maxEncodingPriority")));

        getTerritories(customer);
    }

    _connectionPool->unborrow(conn);
    
    return customers;
}
*/

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(int64_t workspaceKey)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select workspaceKey, name, directoryName, maxStorageInGB, maxEncodingPriority from MMS_Workspace where workspaceKey = ?";
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
        workspace->_maxStorageInGB = resultSet->getInt("maxStorageInGB");
        workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("maxEncodingPriority")));

        getTerritories(workspace);
    }
    else
    {
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceKey: " + to_string(workspaceKey)
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _connectionPool->unborrow(conn);
    
    return workspace;
}

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(string workspaceName)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select workspaceKey, name, directoryName, maxStorageInGB, maxEncodingPriority from MMS_Workspace where name = ?";
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
        workspace->_maxStorageInGB = resultSet->getInt("maxStorageInGB");
        workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("maxEncodingPriority")));

        getTerritories(workspace);
    }
    else
    {
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceName: " + workspaceName
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _connectionPool->unborrow(conn);
    
    return workspace;
}

void MMSEngineDBFacade::getTerritories(shared_ptr<Workspace> workspace)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

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

    _connectionPool->unborrow(conn);
}

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUser(
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
    long maxStorageInGB,
    string languageCode,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    int64_t         userKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

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
            bool enabled = false;
            
            lastSQLCommand = 
                    "insert into MMS_Workspace ("
                    "workspaceKey, creationDate, name, directoryName, workspaceType, deliveryURL, isEnabled, maxEncodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInGB, currentStorageUsageInGB, languageCode) values ("
                    "NULL,         NULL,         ?,    ?,             ?,             ?,           ?,         ?,                   ?,              ?,                   ?,              0,                       ?)";

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
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPeriod));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionsNumber);
            preparedStatement->setInt(queryParameterIndex++, maxStorageInGB);
            preparedStatement->setString(queryParameterIndex++, languageCode);

            preparedStatement->executeUpdate();
        }

        workspaceKey = getLastInsertId(conn);

        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
        confirmationCode = to_string(e());
        {
            lastSQLCommand = 
                    "insert into MMS_ConfirmationCode (userKey, workspaceKey, creationDate, confirmationCode) values ("
                    "?, ?, NOW(), ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
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

        int64_t territoryKey = addTerritory(
                conn,
                workspaceKey,
                _defaultTerritoryName);
                
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
            make_tuple(workspaceKey, userKey, confirmationCode);
    
    return workspaceKeyUserKeyAndConfirmationCode;
}

string MMSEngineDBFacade::confirmUser(
    string confirmationCode
)
{
    string      apiKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        int64_t     userKey;
        int64_t     workspaceKey;
        {
            lastSQLCommand = 
                "select userKey, workspaceKey from MMS_ConfirmationCode where confirmationCode = ? and DATE_ADD(creationDate, INTERVAL ? DAY) >= NOW()";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, confirmationCode);
            preparedStatement->setInt(queryParameterIndex++, _confirmationCodeRetentionInDays);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
                workspaceKey = resultSet->getInt64("workspaceKey");
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

        string emailAddress;
        {
            lastSQLCommand = 
                "select eMailAddress from MMS_User where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
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

        {
            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
            default_random_engine e(seed);

            string sourceApiKey = emailAddress + "__SEP__" + to_string(e());
            apiKey = Encrypt::encrypt(sourceApiKey);

            string flags;
            {
                bool adminAPI = false;
                bool userAPI = true;

                if (adminAPI)
                    flags.append("ADMIN_API");

                if (userAPI)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("USER_API");
                }
            }
            
            lastSQLCommand = 
                "insert into MMS_APIKey (apiKey, userKey, workspaceKey, flags, creationDate, expirationDate) values ("
                "?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, flags); // ADMIN_API,USER_API
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
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return apiKey;
}

tuple<shared_ptr<Workspace>,bool,bool> MMSEngineDBFacade::checkAPIKey (string apiKey)
{
    shared_ptr<Workspace> workspace;
    string          flags;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        int64_t         userKey;
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(APIKeyNotFoundOrExpired e)
    {        
        _connectionPool->unborrow(conn);

        string exceptionMessage(e.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw e;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    tuple<shared_ptr<Workspace>,bool,bool> workspaceAndFlags;
    
    workspaceAndFlags = make_tuple(workspace,
        flags.find("ADMIN_API") == string::npos ? false : true,
        flags.find("USER_API") == string::npos ? false : true
    );
            
    return workspaceAndFlags;
}

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
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return territoryKey;
}

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
                        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return isLoginValid;
}

string MMSEngineDBFacade::getPassword(string emailAddress)
{
    string      password;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
                        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return password;
}

int64_t MMSEngineDBFacade::addEncodingProfilesSet (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
        string label)
{
    int64_t     encodingProfilesSetKey;
    
    string      lastSQLCommand;
    
    try
    {
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
                lastSQLCommand = 
                    "insert into MMS_EncodingProfilesSet (encodingProfilesSetKey, workspaceKey, label) values ("
                    "NULL, ?, ?)";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
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
                "select encodingProfileKey from MMS_EncodingProfile where workspaceKey =  and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfileKey     = resultSet->getInt64("encodingProfileKey");
                
                lastSQLCommand = 
                    "update MMS_EncodingProfile set contentType = ?, technology = ?, jsonProfile = ? where encodingProfileKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
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
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfileKey;
}

void MMSEngineDBFacade::getIngestionsToBeManaged(
        vector<tuple<int64_t, string, shared_ptr<Workspace>, string, IngestionType, IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs
        // int maxIngestionJobsWithDependencyToCheck
)
{
    string      lastSQLCommand;
    bool        autoCommit = true;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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

        // ingested jobs that do not have to wait a dependency
        {
            int mysqlOffset = 0;
            int mysqlRowCount = maxIngestionJobs;
            bool noMoreRowsReturned = false;
            while(ingestionsToBeManaged.size() < maxIngestionJobs && !noMoreRowsReturned)
            {
                lastSQLCommand = 
                    "select ij.ingestionJobKey, DATE_FORMAT(ij.startIngestion, '%Y-%m-%d %H:%i:%s') as startIngestion, "
                        "ir.workspaceKey, ij.metaDataContent, ij.status, ij.ingestionType "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey and ij.processorMMS is null "
                        "and (ij.status = ? or (ij.status in (?, ?, ?, ?) and ij.sourceBinaryTransferred = 1)) "
                        "limit ?, ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
                preparedStatement->setInt(queryParameterIndex++, mysqlOffset);
                preparedStatement->setInt(queryParameterIndex++, mysqlRowCount);

                noMoreRowsReturned = true;
                mysqlOffset += maxIngestionJobs;
                
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                while (resultSet->next())
                {
                    if (noMoreRowsReturned)
                        noMoreRowsReturned = false;
                
                    int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
                    string startIngestion       = resultSet->getString("startIngestion");
                    int64_t workspaceKey         = resultSet->getInt64("workspaceKey");
                    string metaDataContent      = resultSet->getString("metaDataContent");
                    IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("status"));
                    IngestionType ingestionType     = MMSEngineDBFacade::toIngestionType(resultSet->getString("ingestionType"));

                    /**/
                    _logger->info(__FILEREF__ + "Analyzing dependencies for the IngestionJob"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    );
                    /**/
                    
                    bool ingestionJobToBeManaged = true;

                    lastSQLCommand = 
                        "select dependOnIngestionJobKey, dependOnSuccess from MMS_IngestionJobDependency where ingestionJobKey = ? order by orderNumber asc";
                    shared_ptr<sql::PreparedStatement> preparedStatementDependency (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementDependency->setInt64(queryParameterIndex++, ingestionJobKey);

                    shared_ptr<sql::ResultSet> resultSetDependency (preparedStatementDependency->executeQuery());
                    while (resultSetDependency->next())
                    {
                        if (!resultSetDependency->isNull("dependOnIngestionJobKey"))
                        {
                            int64_t dependOnIngestionJobKey     = resultSetDependency->getInt64("dependOnIngestionJobKey");
                            int dependOnSuccess                 = resultSetDependency->getInt("dependOnSuccess");

                            lastSQLCommand = 
                                "select status from MMS_IngestionJob where ingestionJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementIngestionJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementIngestionJob->setInt64(queryParameterIndex++, dependOnIngestionJobKey);

                            shared_ptr<sql::ResultSet> resultSetIngestionJob (preparedStatementIngestionJob->executeQuery());
                            if (resultSetIngestionJob->next())
                            {
                                string sStatus = resultSetIngestionJob->getString("status");
                                
                                _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
                                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                                    + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
                                    + ", dependOnSuccess: " + to_string(dependOnSuccess)
                                    + ", status (dependOnIngestionJobKey): " + sStatus
                                );
                                
                                IngestionStatus ingestionStatusDependency     = MMSEngineDBFacade::toIngestionStatus(sStatus);

                                if (MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatusDependency))
                                {
                                    if (dependOnSuccess && MMSEngineDBFacade::isIngestionStatusFailed(ingestionStatusDependency))
                                    {
                                        ingestionJobToBeManaged = false;

                                        break;
                                    }
                                    else if (!dependOnSuccess && MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatusDependency))
                                    {
                                        ingestionJobToBeManaged = false;

                                        break;
                                    }
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
                    
                    if (ingestionJobToBeManaged)
                    {
                        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                        tuple<int64_t, string, shared_ptr<Workspace>, string, IngestionType, IngestionStatus> ingestionToBeManaged
                                = make_tuple(ingestionJobKey, startIngestion, workspace, metaDataContent, ingestionType, ingestionStatus);

                        ingestionsToBeManaged.push_back(ingestionToBeManaged);
                    }
                }
            }
        }

        /*
        // ingestion jobs that need to wait at least one dependency
        {
            {
                lastSQLCommand = 
                    "select ingestionJobKey, DATE_FORMAT(startIngestion, '%Y-%m-%d %H:%i:%s') as startIngestion, customerKey, metaDataContent, mediaItemKeysDependency, status, ingestionType from MMS_IngestionJob where "
                        "processorMMS is null and mediaItemKeysDependency is not null "
                        "and status = ? limit ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_Ingestion));
                preparedStatement->setInt(queryParameterIndex++, maxIngestionJobsWithDependencyToCheck);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                while (resultSet->next())
                {
                    int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
                    string startIngestion       = resultSet->getString("startIngestion");
                    int64_t customerKey         = resultSet->getInt64("customerKey");
                    string metaDataContent      = resultSet->getString("metaDataContent");
                    IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("status"));
                    string mediaItemKeysDependency;
                    shared_ptr<Workspace> customer = getWorkspace(customerKey);

                    mediaItemKeysDependency = resultSet->getString("mediaItemKeysDependency");

                    bool ingestionToBeManaged = false;
                    
                    if (mediaItemKeysDependency != "")
                    {
                        if (resultSet->getInt("ingestionType") == static_cast<int>(MMSEngineDBFacade::IngestionType::Screenshots))
                        {
                            // mediaItemKeysDependency has to contain just one MIK
                            int64_t mediaItemKey = stoll(mediaItemKeysDependency);

                            lastSQLCommand = 
                                "select mediaItemKey from MMS_MediaItem where mediaItemKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementMediaItems (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementMediaItems->setInt64(queryParameterIndex++, mediaItemKey);

                            shared_ptr<sql::ResultSet> resultSetMediaItems (preparedStatementMediaItems->executeQuery());
                            if (resultSetMediaItems->next())
                            {
                                ingestionToBeManaged = true;
                            }
                        }
                    }
                        
                    if (ingestionToBeManaged)
                    {
                        tuple<int64_t,string,shared_ptr<Workspace>,string,IngestionStatus,string> ingestionToBeManaged
                                = make_tuple(ingestionJobKey, startIngestion, customer, metaDataContent, ingestionStatus, mediaItemKeysDependency);

                        ingestionsToBeManaged.push_back(ingestionToBeManaged);
                    }
                }
            }
        }
         */

        for (tuple<int64_t, string, shared_ptr<Workspace>, string, IngestionType, IngestionStatus>& ingestionToBeManaged:
            ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            string startIngestion;
            shared_ptr<Workspace> workspace;
            string metaDataContent;
            string sourceReference;
            MMSEngineDBFacade::IngestionStatus ingestionStatus;
            MMSEngineDBFacade::IngestionType ingestionType;

            tie(ingestionJobKey, startIngestion, workspace, metaDataContent, ingestionType, ingestionStatus) = ingestionToBeManaged;

            lastSQLCommand = 
                "update MMS_IngestionJob set processorMMS = ? where ingestionJobKey = ?";
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }        
}

shared_ptr<MySQLConnection> MMSEngineDBFacade::beginIngestionJobs ()
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }

    return conn;    
}

int64_t MMSEngineDBFacade::addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, string rootType, string rootLabel,
        bool rootLabelDuplication
)
{
    int64_t     ingestionRootKey;
    
    string      lastSQLCommand;
    
    try
    {
        if (!rootLabelDuplication)
        {
            lastSQLCommand = 
                "select ingestionRootKey from MMS_IngestionRoot where workspaceKey = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, rootLabel);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int64_t ingestionRootKey = resultSet->getInt64("ingestionRootKey");
                
                {
                    string errorMessage = __FILEREF__ + "IngestionRoot is already present"
                        + ", ingestionRootKey: " + to_string(ingestionRootKey)
                        + ", rootLabel: " + rootLabel
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
                    "insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, type, label) values ("
                    "NULL, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                preparedStatement->setString(queryParameterIndex++, rootType);
                preparedStatement->setString(queryParameterIndex++, rootLabel);

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
                    "insert into MMS_IngestionJob (ingestionJobKey, ingestionRootKey, label, mediaItemKey, metaDataContent, ingestionType, startIngestion, endIngestion, downloadingProgress, uploadingProgress, sourceBinaryTransferred, processorMMS, status, errorMessage) values ("
                                                  "NULL,            ?,                ?,     NULL,         ?,               ?,             NULL,           NULL,         NULL,                NULL,              0,                       NULL,         ?,      NULL)";

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

                    preparedStatement->executeUpdate();
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

                        preparedStatement->executeUpdate();
                        
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
        string processorMMS
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

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

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ?, endIngestion = NOW(), processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

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

                lastSQLCommand = 
                    "update MMS_IngestionJob set status = ?, endIngestion = NOW() where ingestionJobKey in "
                    "(select ingestionJobKey from MMS_IngestionJobDependency where dependOnIngestionJobKey = ? and dependOnSuccess = ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted));
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);

                int rowsUpdated = preparedStatement->executeUpdate();
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
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

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

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set ingestionType = ?, status = ?, endIngestion = NOW(), processorMMS = ?, errorMessage = ? where ingestionJobKey = ?";

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

                lastSQLCommand = 
                    "update MMS_IngestionJob set status = ?, endIngestion = NOW() where ingestionJobKey in "
                    "(select ingestionJobKey from MMS_IngestionJobDependency where dependOnIngestionJobKey = ? and dependOnSuccess = ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted));
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);

                int rowsUpdated = preparedStatement->executeUpdate();
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
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage)
{
    
    bool        toBeCancelled = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
                
                if (ingestionStatus == IngestionStatus::End_DownloadCancelledByUser)
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

MMSEngineDBFacade::ContentType MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t referenceMediaItemKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    
    MMSEngineDBFacade::ContentType contentType;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceMediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        _connectionPool->unborrow(conn);
        
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );

        throw e;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return contentType;
}

pair<int64_t,MMSEngineDBFacade::ContentType> MMSEngineDBFacade::getMediaItemKeyDetailsByIngestionJobKey(
    int64_t referenceIngestionJobKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType from MMS_IngestionJob ij, MMS_MediaItem mi "
                "where ij.mediaItemKey = mi.mediaItemKey and ij.ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceIngestionJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                mediaItemKeyAndContentType.first = resultSet->getInt64("mediaItemKey");
                mediaItemKeyAndContentType.second = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        _connectionPool->unborrow(conn);
        
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );

        throw e;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return mediaItemKeyAndContentType;
}

pair<int64_t,MMSEngineDBFacade::ContentType> MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
    string referenceUniqueName, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select mediaItemKey, contentType from MMS_MediaItem where uniqueName = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(MediaItemKeyNotFound e)
    {
        _connectionPool->unborrow(conn);
        
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );
        else
            _logger->error(__FILEREF__ + "SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
            );

        throw e;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return mediaItemKeyAndContentType;
}

tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> MMSEngineDBFacade::getVideoDetails(
    int64_t mediaItemKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    
    try
    {
        conn = _connectionPool->borrow();	

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
                "from MMS_VideoItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

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

        _connectionPool->unborrow(conn);
        
        return make_tuple(durationInMilliSeconds, bitRate,
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::getEncodingJobs(
        bool resetToBeDone,
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        if (resetToBeDone)
        {
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null where processorMMS = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsReset = preparedStatement->executeUpdate();            
            if (rowsReset > 0)
                _logger->warn(__FILEREF__ + "Rows (MMS_EncodingJob) that were reset"
                    + ", rowsReset: " + to_string(rowsReset)
                );
        }
        else
        {
            int retentionDaysToReset = 7;
            
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null where processorMMS = ? and status = ? and DATE_ADD(encodingJobStart, INTERVAL ? DAY) <= NOW()";
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
                "select encodingJobKey, ingestionJobKey, sourcePhysicalPathKey, encodingPriority, encodingProfileKey from MMS_EncodingJob " 
                "where processorMMS is null and status = ? and encodingJobStart <= NOW() "
                "order by encodingPriority desc, encodingJobStart asc, failuresNumber asc for update";
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();
                
                encodingItem->_encodingJobKey = encodingResultSet->getInt64("encodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_physicalPathKey = encodingResultSet->getInt64("sourcePhysicalPathKey");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(encodingResultSet->getInt("encodingPriority"));
                encodingItem->_encodingProfileKey = encodingResultSet->getInt64("ncodingProfileKey");
                
                {
                    lastSQLCommand = 
                        "select m.workspaceKey, m.contentType, p.partitionNumber, p.mediaItemKey, p.fileName, p.relativePath "
                        "from MMS_MediaItem m, MMS_PhysicalPath p where m.mediaItemKey = p.mediaItemKey and p.physicalPathKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementPhysicalPath->setInt64(queryParameterIndex++, encodingItem->_physicalPathKey);

                    shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                    if (physicalPathResultSet->next())
                    {
                        encodingItem->_contentType = MMSEngineDBFacade::toContentType(physicalPathResultSet->getString("contentType"));
                        encodingItem->_workspace = getWorkspace(physicalPathResultSet->getInt64("workspaceKey"));
                        encodingItem->_mmsPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                        encodingItem->_mediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        encodingItem->_fileName = physicalPathResultSet->getString("fileName");
                        encodingItem->_relativePath = physicalPathResultSet->getString("relativePath");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }

                if (encodingItem->_contentType == ContentType::Video)
                {
                    lastSQLCommand = 
                        "select durationInMilliSeconds from MMS_VideoItem where mediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementVideo->setInt64(queryParameterIndex++, encodingItem->_mediaItemKey);

                    shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                    if (videoResultSet->next())
                    {
                        encodingItem->_durationInMilliSeconds = videoResultSet->getInt64("durationInMilliSeconds");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
                else if (encodingItem->_contentType == ContentType::Audio)
                {
                    lastSQLCommand = 
                        "select durationInMilliSeconds from MMS_AudioItem where mediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementAudio (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementAudio->setInt64(queryParameterIndex++, encodingItem->_mediaItemKey);

                    shared_ptr<sql::ResultSet> audioResultSet (preparedStatementAudio->executeQuery());
                    if (audioResultSet->next())
                    {
                        encodingItem->_durationInMilliSeconds = audioResultSet->getInt64("durationInMilliSeconds");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }

                {
                    lastSQLCommand = 
                        "select technology, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementEncodingProfile->setInt64(queryParameterIndex++, encodingItem->_encodingProfileKey);

                    shared_ptr<sql::ResultSet> encodingProfilesResultSet (preparedStatementEncodingProfile->executeQuery());
                    if (encodingProfilesResultSet->next())
                    {
                        encodingItem->_encodingProfileTechnology = static_cast<EncodingTechnology>(encodingProfilesResultSet->getInt("technology"));
                        encodingItem->_jsonProfile = encodingProfilesResultSet->getString("jsonProfile");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed"
                                + ", encodingItem->_encodingProfileKey: " + to_string(encodingItem->_encodingProfileKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", processorMMS: " + processorMMS
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetKey(
    int64_t workspaceKey,
    int64_t encodingProfilesSetKey)
{
    vector<int64_t> encodingProfilesSetKeys;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }       
    
    return encodingProfilesSetKeys;
}

int MMSEngineDBFacade::addEncodingJob (
    int64_t ingestionJobKey,
    int64_t encodingProfileKey,
    int64_t mediaItemKey,
    EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        int64_t sourcePhysicalPathKey;
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
                sourcePhysicalPathKey = resultSet->getInt64("physicalPathKey");
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
        
        {
            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, sourcePhysicalPathKey, encodingPriority, encodingProfileKey, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, failuresNumber) values ("
                "NULL, ?, ?, ?, ?, NULL, NULL, NULL, ?, NULL, 0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, sourcePhysicalPathKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPriority));
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            preparedStatement->executeUpdate();
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }        
}

int MMSEngineDBFacade::updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t ingestionJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    int encodingFailureNumber = -1;

    try
    {
        conn = _connectionPool->borrow();	

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
            
            if (encodingFailureNumber + 1 >= _maxEncodingFailures)
                newEncodingStatus          = EncodingStatus::End_Failed;
            else
            {
                newEncodingStatus          = EncodingStatus::ToBeProcessed;
                encodingFailureNumber++;
            }

            {
                lastSQLCommand = 
                    "update MMS_EncodingJob set status = ?, processorMMS = NULL, failuresNumber = ?, encodingProgress = NULL where encodingJobKey = ? and status = ?";
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
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingProgress = NULL where encodingJobKey = ? and status = ?";
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
            newEncodingStatus       = EncodingStatus::End_ProcessedSuccessful;

            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW(), encodingProgress = 100 where encodingJobKey = ? and status = ?";
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
        
        if (newEncodingStatus == EncodingStatus::End_ProcessedSuccessful)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_TaskSuccess;
            string errorMessage;
            string processorMMS;
            
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
        }
        else if (newEncodingStatus == EncodingStatus::End_Failed)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_IngestionFailure;
            string errorMessage;
            string processorMMS;

            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage, processorMMS);
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingFailureNumber;
}

void MMSEngineDBFacade::updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

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
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

string MMSEngineDBFacade::checkWorkspaceMaxIngestionNumber (
    int64_t workspaceKey
)
{
    string      relativePathToBeUsed;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        int maxIngestionsNumber;
        int currentIngestionsNumber;
        int encodingPeriod;
        string periodStartDateTime;
        string periodEndDateTime;
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select c.maxIngestionsNumber, cmi.currentIngestionsNumber, c.encodingPeriod, " 
                    "DATE_FORMAT(cmi.startDateTime, '%Y-%m-%d %H:%i:%s') as LocalStartDateTime, DATE_FORMAT(cmi.endDateTime, '%Y-%m-%d %H:%i:%s') as LocalEndDateTime, "
                    "cmi.currentDirLevel1, cmi.currentDirLevel2, cmi.currentDirLevel3 "
                "from MMS_Workspace c, MMS_WorkspaceMoreInfo cmi where c.workspaceKey = cmi.workspaceKey and c.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                maxIngestionsNumber = resultSet->getInt("maxIngestionsNumber");
                currentIngestionsNumber = resultSet->getInt("currentIngestionsNumber");
                encodingPeriod = resultSet->getInt("encodingPeriod");
                periodStartDateTime = resultSet->getString("LocalStartDateTime");
                periodEndDateTime = resultSet->getString("LocalEndDateTime");                
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
                
                if (encodingPeriod == static_cast<int>(EncodingPeriod::Daily))
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
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Weekly))
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
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Monthly))
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
                + ", encodingPeriod: " + to_string(static_cast<int>(encodingPeriod))
            ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
        
        {
            char pCurrentRelativePath [64];
            
            sprintf (pCurrentRelativePath, "/%03d/%03d/%03d/", 
                currentDirLevel1, currentDirLevel2, currentDirLevel3);
            
            relativePathToBeUsed = pCurrentRelativePath;
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "exception"
            + ", e.what: " + e.what()
        );

        throw e;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    
    return relativePathToBeUsed;
}

pair<int64_t,int64_t> MMSEngineDBFacade::saveIngestedContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,        
        MMSEngineDBFacade::ContentType contentType,
        Json::Value typeRoot,
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
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        _logger->info(__FILEREF__ + "Retrieving contentProviderKey...");
        // get ContentProviderKey
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
            if (isMetadataPresent(typeRoot, "ContentProviderName"))
                contentProviderName = typeRoot.get("ContentProviderName", "XXX").asString();
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

        _logger->info(__FILEREF__ + "Insert into MMS_MediaItem...");
        // int64_t encodingProfileSetKey;
        string uniqueName;
        {
            string title = "";
            string ingester = "";
            string keywords = "";
            string sContentType;
            // string encodingProfilesSet;

            if (isMetadataPresent(typeRoot, "UniqueName"))
                uniqueName = typeRoot.get("UniqueName", "XXX").asString();

            title = typeRoot.get("Title", "XXX").asString();
            
            if (isMetadataPresent(typeRoot, "Ingester"))
                ingester = typeRoot.get("Ingester", "XXX").asString();

            if (isMetadataPresent(typeRoot, "Keywords"))
                keywords = typeRoot.get("Keywords", "XXX").asString();

            /*
            {
                if (isMetadataPresent(typeRoot, "EncodingProfilesSet"))
                    encodingProfilesSet = typeRoot.get("EncodingProfilesSet", "XXX").asString();
                else
                    encodingProfilesSet = "workspaceDefault";
                
                if (encodingProfilesSet == "systemDefault")
                {
                    lastSQLCommand = 
                        "select encodingProfilesSetKey from MMS_EncodingProfilesSet where contentType = ? and workspaceKey is null and name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                }
                else if (encodingProfilesSet == "workspaceDefault")
                {
                    lastSQLCommand = 
                        "select encodingProfilesSetKey from MMS_EncodingProfilesSet where contentType = ? and workspaceKey = ? and name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
                }
                else
                {
                    lastSQLCommand = 
                        "select encodingProfilesSetKey from MMS_EncodingProfilesSet where contentType = ? and workspaceKey = ? and name = ?";
                }
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                if (encodingProfilesSet == "systemDefault")
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                }
                else if (encodingProfilesSet == "workspaceDefault")
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
                }
                else
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
                    preparedStatement->setString(queryParameterIndex++, encodingProfilesSet);
                }
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingProfileSetKey = resultSet->getInt64("encodingProfilesSetKey");
                }
                else
                {
                    string errorMessage = __FILEREF__ + "encodingProfilesSetKey is not present"
                        + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", encodingProfilesSet: " + encodingProfilesSet
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }
            */


            lastSQLCommand = 
                "insert into MMS_MediaItem (mediaItemKey, uniqueName, workspaceKey, contentProviderKey, title, ingester, keywords, " 
                "ingestionDate, contentType) values ("
                "NULL, ?, ?, ?, ?, ?, ?, NULL, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, uniqueName);
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, contentProviderKey);
            preparedStatement->setString(queryParameterIndex++, title);
            if (ingester == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, ingester);
            if (keywords == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, keywords);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));

            preparedStatement->executeUpdate();
        }
        
        int64_t mediaItemKey = getLastInsertId(conn);

        if (uniqueName == "")
        {
            lastSQLCommand = 
                "update MMS_MediaItem set uniqueName = ? where mediaItemKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, to_string(mediaItemKey));
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        {
            if (contentType == ContentType::Video)
            {
                lastSQLCommand = 
                    "insert into MMS_VideoItem (mediaItemKey, durationInMilliSeconds, bitRate, width, height, avgFrameRate, "
                    "videoCodecName, videoProfile, videoBitRate, "
                    "audioCodecName, audioSampleRate, audioChannels, audioBitRate) values ("
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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
                    "insert into MMS_AudioItem (mediaItemKey, durationInMilliSeconds, codecName, bitRate, sampleRate, channels) values ("
                    "?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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
                    "insert into MMS_ImageItem (mediaItemKey, width, height, format, quality) values ("
                    "?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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

        {
            int drm = 0;

            lastSQLCommand = 
                "insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, creationDate) values ("
                "NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, mediaSourceFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, mmsPartitionIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
            preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);

            preparedStatement->executeUpdate();
        }

        int64_t physicalPathKey = getLastInsertId(conn);

        // territories
        {
            string field = "Territories";
            if (isMetadataPresent(typeRoot, field))
            {
                const Json::Value territories = typeRoot[field];
                
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
        
        /*
        {
            EncodingPriority encodingPriority;
            if (contentType == ContentType::Video || contentType == ContentType::Audio)
            {
                string field = "EncodingPriority";
                if (isMetadataPresent(typeRoot, field))
                {
                    string strEncodingPriority = typeRoot.get(field, "XXX").asString();
                    encodingPriority = MMSEngineDBFacade::toEncodingPriority(strEncodingPriority);

                    if (static_cast<int>(encodingPriority) > workspace->_maxEncodingPriority)
                        encodingPriority = static_cast<EncodingPriority>(workspace->_maxEncodingPriority); 
                }
                else
                    encodingPriority = static_cast<EncodingPriority>(workspace->_maxEncodingPriority);
            }
            else
                encodingPriority = EncodingPriority::Medium;

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, sourcePhysicalPathKey, encodingPriority, encodingProfileKey, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, failuresNumber) "
                "select                       NULL,           ?,               ?,                     ?,                encodingProfileKey, NULL,             NULL,           NULL,     ?,      NULL,         0 "
                "from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPriority));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileSetKey);

            preparedStatement->executeUpdate();
        }
        */

        {
            IngestionStatus newIngestionStatus = IngestionStatus::End_TaskSuccess;
            
            lastSQLCommand = 
                "update MMS_IngestionJob set mediaItemKey = ?, status = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            _logger->info(__FILEREF__ + "Update IngestionJobs"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            );
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
        
        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return mediaItemKeyAndPhysicalPathKey;
}

tuple<int,string,string,string> MMSEngineDBFacade::getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey
)
{
        
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        int64_t workspaceKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, pp.partitionNumber, pp.relativePath, pp.fileName "
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
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
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

        _connectionPool->unborrow(conn);

        return make_tuple(mmsPartitionNumber, workspace->_directoryName, relativePath, fileName);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "exception"
            + ", e.what: " + e.what()
        );

        throw e;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }        
}

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey
)
{
    int64_t     encodedPhysicalPathKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
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
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            preparedStatement->executeUpdate();
        }

        encodedPhysicalPathKey = getLastInsertId(conn);

        // publishing territories
        {
            lastSQLCommand = 
                "select t.territoryKey, t.name, DATE_FORMAT(d.startPublishing, '%Y-%m-%d %H:%i:%s') as startPublishing, DATE_FORMAT(d.endPublishing, '%Y-%m-%d %H:%i:%s') as endPublishing from MMS_Territory t, MMS_DefaultTerritoryInfo d "
                "where t.territoryKey = d.territoryKey and t.workspaceKey = ? and d.mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatementTerritory (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementTerritory->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatementTerritory->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSetTerritory (preparedStatementTerritory->executeQuery());
            while (resultSetTerritory->next())
            {
                int64_t territoryKey = resultSetTerritory->getInt64("territoryKey");
                string territoryName = resultSetTerritory->getString("name");
                string startPublishing = resultSetTerritory->getString("startPublishing");
                string endPublishing = resultSetTerritory->getString("endPublishing");
                
                lastSQLCommand = 
                    "select publishingStatus from MMS_Publishing where mediaItemKey = ? and territoryKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatementPublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatementPublishing->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatementPublishing->setInt64(queryParameterIndex++, territoryKey);

                shared_ptr<sql::ResultSet> resultSetPublishing (preparedStatementPublishing->executeQuery());
                if (resultSetPublishing->next())
                {
                    int publishingStatus = resultSetPublishing->getInt("PublishingStatus");
                    
                    if (publishingStatus == 1)
                    {
                        lastSQLCommand = 
                            "update MMS_Publishing set publishingStatus = 0 where mediaItemKey = ? and territoryKey = ?";

                        shared_ptr<sql::PreparedStatement> preparedStatementUpdatePublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdatePublishing->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatementUpdatePublishing->setInt(queryParameterIndex++, territoryKey);

                        int rowsUpdated = preparedStatementUpdatePublishing->executeUpdate();
                        if (rowsUpdated != 1)
                        {
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", territoryKey: " + to_string(territoryKey)
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
                    lastSQLCommand = 
                        "insert into MMS_Publishing (publishingKey, mediaItemKey, territoryKey, startPublishing, endPublishing, publishingStatus) values ("
	        	"NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), 0)";

                    shared_ptr<sql::PreparedStatement> preparedStatementInsertPublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementInsertPublishing->setInt64(queryParameterIndex++, mediaItemKey);
                    preparedStatementInsertPublishing->setInt(queryParameterIndex++, territoryKey);
                    preparedStatementInsertPublishing->setString(queryParameterIndex++, startPublishing);
                    preparedStatementInsertPublishing->setString(queryParameterIndex++, endPublishing);

                    preparedStatementInsertPublishing->executeUpdate();
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodedPhysicalPathKey;
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
    shared_ptr<MySQLConnection> conn;

    string      lastSQLCommand;

    try
    {
        conn = _connectionPool->borrow();	

        shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());

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
                    "workspaceType                   TINYINT NOT NULL,"
                    "deliveryURL                    VARCHAR (256) NULL,"
                    "isEnabled                      TINYINT (1) NOT NULL,"
                    "maxEncodingPriority            VARCHAR (32) NOT NULL,"
                    "encodingPeriod                 TINYINT NOT NULL,"
                    "maxIngestionsNumber            INT NOT NULL,"
                    "maxStorageInGB                 INT NOT NULL,"
                    "currentStorageUsageInGB        INT DEFAULT 0,"
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
                    "apiKey                     VARCHAR (128) NOT NULL,"
                    "userKey                    BIGINT UNSIGNED NOT NULL,"
                    "workspaceKey               BIGINT UNSIGNED NOT NULL,"
                    "flags			SET('ADMIN_API', 'USER_API') NOT NULL,"
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
                    "workspaceKey                   BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
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
            //  The <image enc. params> format is:
            //	_I1<image_format>_I2<width>_I3<height>_I4<AspectRatio>_I5<Interlace>
            //	<image_format>: PNG, JPG, GIF
            //	<AspectRatio>: 1, 0
            //	<Interlace>: 0: NoInterlace, 1: LineInterlace, 2: PlaneInterlace, 3: PartitionInterlace
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfile ("
                    "encodingProfileKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  			BIGINT UNSIGNED NULL,"
                    "label				VARCHAR (64) NULL,"
                    "contentType			VARCHAR (32) NOT NULL,"
                    "technology         		TINYINT NOT NULL,"
                    "jsonProfile    			VARCHAR (512) NOT NULL,"
                    "constraint MMS_EncodingProfile_PK PRIMARY KEY (encodingProfileKey), "
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
            // workspaceKey and name
            //      both NULL: global/system EncodingProfiles for the ContentType
            //      only name NULL: Workspace default EncodingProfiles for the ContentType
            //      both different by NULL: named Workspace EncodingProfiles for the ContentType
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSet ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "workspaceKey  				BIGINT UNSIGNED NOT NULL,"
                    "label					VARCHAR (64) NOT NULL,"
                    "constraint MMS_EncodingProfilesSet_PK PRIMARY KEY (encodingProfilesSetKey)," 
                    "constraint MMS_EncodingProfilesSet_FK foreign key (workspaceKey) "
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
            //  insert global EncodingProfilesSet per ContentType
            vector<ContentType> contentTypes = { ContentType::Video, ContentType::Audio, ContentType::Image };
            
            for (ContentType contentType: contentTypes)
            {
                int     encodingProfilesSetCount = -1;
                {
                    lastSQLCommand = 
                        "select count(*) from MMS_EncodingProfilesSet where contentType = ? and workspaceKey is NULL and name is NULL";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));

                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                    if (resultSet->next())
                    {
                        encodingProfilesSetCount = resultSet->getInt(1);
                    }
                }

                if (encodingProfilesSetCount == 0)
                {
                    lastSQLCommand = 
                        "insert into MMS_EncodingProfilesSet (encodingProfilesSetKey, contentType, workspaceKey, name) values (NULL, ?, NULL, NULL)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->executeUpdate();
                }
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
        */    
        
        try
        {
            // EncodingProfiles associated to each family (EncodingProfilesSet)
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSetMapping ("
                    "encodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_EncodingProfilesSetMapping_PK PRIMARY KEY (encodingProfilesSetKey, encodingProfileKey), "
                    "constraint MMS_EncodingProfilesSetMapping_FK1 foreign key (encodingProfilesSetKey) "
                        "references MMS_EncodingProfilesSet (encodingProfilesSetKey), "
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
                    "workspaceKey                BIGINT UNSIGNED NOT NULL,"
                    "type                       VARCHAR (64) NOT NULL,"
                    "label                      VARCHAR (128) NULL,"
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
                "create table if not exists MMS_IngestionJob ("
                    "ingestionJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ingestionRootKey                BIGINT UNSIGNED NOT NULL,"
                    "label                              VARCHAR (128) NULL,"
                    "mediaItemKey               BIGINT UNSIGNED NULL,"
                    "metaDataContent            TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingestionType              VARCHAR (64) NOT NULL,"
                    "startIngestion             TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "endIngestion               DATETIME NULL,"
                    "downloadingProgress        DECIMAL(3,1) NULL,"
                    "uploadingProgress          DECIMAL(3,1) NULL,"
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
            // workspaceKey is the owner of the content
            // IngestedRelativePath MUST start always with '/' and ends always with '/'
            // IngestedFileName and IngestedRelativePath refer the ingested content independently
            //		if it is encoded or uncompressed
            // if EncodingProfilesSet is NULL, it means the ingested content is already encoded
            // The ContentProviderKey is the entity owner of the content. For example H3G is our workspace and EMI is the ContentProvider.
            lastSQLCommand = 
                "create table if not exists MMS_MediaItem ("
                    "mediaItemKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "uniqueName      			VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "workspaceKey			BIGINT UNSIGNED NOT NULL,"
                    "contentProviderKey			BIGINT UNSIGNED NOT NULL,"
                    "title      			VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "ingester				VARCHAR (128) NULL,"
                    "keywords				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "ingestionDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "contentType                        VARCHAR (32) NOT NULL,"
                    "constraint MMS_MediaItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_MediaItem_FK foreign key (workspaceKey) "
                        "references MMS_Workspace (workspaceKey) on delete cascade, "
                    "constraint MMS_MediaItem_FK2 foreign key (contentProviderKey) "
                        "references MMS_ContentProvider (contentProviderKey), "
                    "UNIQUE (uniqueName)) "
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
                "create index MMS_MediaItem_idx3 on MMS_MediaItem (contentType, ingestionDate)";
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
                "create table if not exists MMS_VideoItem ("
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
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
                    "constraint MMS_VideoItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_VideoItem_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade) "
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
                "create table if not exists MMS_AudioItem ("
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "durationInMilliSeconds		BIGINT NULL,"
                    "codecName          		VARCHAR (64) NULL,"
                    "bitRate             		INT NULL,"
                    "sampleRate                  	INT NULL,"
                    "channels             		INT NULL,"
                    "constraint MMS_AudioItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_AudioItem_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade) "
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
                "create table if not exists MMS_ImageItem ("
                    "mediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "width				INT NOT NULL,"
                    "height				INT NOT NULL,"
                    "format                       	VARCHAR (64) NULL,"
                    "quality				INT NOT NULL,"
                    "constraint MMS_ImageItem_PK PRIMARY KEY (mediaItemKey), "
                    "constraint MMS_ImageItem_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade) "
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
            // Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
            lastSQLCommand = 
                "create table if not exists MMS_DefaultTerritoryInfo ("
                    "defaultTerritoryInfoKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "mediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "territoryKey				BIGINT UNSIGNED NOT NULL,"
                    /*
                    "DownloadChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "DownloadChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "DownloadReserveCredit			TINYINT (1) NULL,"
                    "DownloadExternalBillingName		VARCHAR (64) NULL,"
                    "DownloadMaxRetries				INT NOT NULL,"
                    "DownloadTTLInSeconds			INT NOT NULL,"
                    "StreamingChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "StreamingChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "StreamingReserveCredit			TINYINT (1) NULL,"
                    "StreamingExternalBillingName		VARCHAR (64) NULL,"
                    "StreamingMaxRetries			INT NOT NULL,"
                    "StreamingTTLInSeconds			INT NOT NULL,"
                    */
                    "startPublishing				DATETIME NOT NULL,"
                    "endPublishing				DATETIME NOT NULL,"
                    "constraint MMS_DefaultTerritoryInfo_PK PRIMARY KEY (defaultTerritoryInfoKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK foreign key (mediaItemKey) "
                        "references MMS_MediaItem (mediaItemKey) on delete cascade, "
                    "constraint MMS_DefaultTerritoryInfo_FK2 foreign key (territoryKey) "
                        "references MMS_Territory (territoryKey) on delete cascade, "
                    /*
                    "constraint MMS_DefaultTerritoryInfo_FK3 foreign key (DownloadChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK4 foreign key (DownloadChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK5 foreign key (StreamingChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK6 foreign key (StreamingChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                     */
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

        try
        {
            // The MMS_EncodingJob table include all the contents that have to be encoded
            //  OriginatingProcedure.
            //      0: ContentIngestion1_0
            //          Used fields: FileName, RelativePath, customerKey, PhysicalPathKey, EncodingProfileKey
            //          The other fields will be NULL
            //      1: Encoding1_0
            //          Used fields: FileName, RelativePath, customerKey, FTPIPAddress (optional), FTPPort (optional),
            //              FTPUser (optional), FTPPassword (optional), EncodingProfileKey
            //          The other fields will be NULL
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
                    "sourcePhysicalPathKey		BIGINT UNSIGNED NULL,"
                    "encodingPriority			TINYINT NOT NULL,"
                    "encodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "encodingJobStart			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "encodingJobEnd			DATETIME NULL,"
                    "encodingProgress                   INT NULL,"
                    "status           			VARCHAR (64) NOT NULL,"
                    "processorMMS			VARCHAR (128) NULL,"
                    "failuresNumber           	INT NOT NULL,"
                    "constraint MMS_EncodingJob_PK PRIMARY KEY (encodingJobKey), "
                    "constraint MMS_EncodingJob_FK foreign key (ingestionJobKey) "
                        "references MMS_IngestionJob (ingestionJobKey) on delete cascade, "
                    "constraint MMS_EncodingJob_FK3 foreign key (sourcePhysicalPathKey) "
                    // on delete cascade is necessary because when the ingestion fails, it is important that the 'removeMediaItemMetaData'
                    //      remove the rows from this table too, otherwise we will be flooded by the errors: PartitionNumber is null
                    // The consequence is that when the PhysicalPath is removed in general, also the rows from this table will be removed
                        "references MMS_PhysicalPath (physicalPathKey) on delete cascade, "
                    "constraint MMS_EncodingJob_FK4 foreign key (encodingProfileKey) "
                        "references MMS_EncodingProfile (encodingProfileKey) on delete cascade, "
                    "UNIQUE (sourcePhysicalPathKey, encodingProfileKey)) "
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
    # Description is something like: +H.264, +enh-aac-plus, ...
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
    # Priority: 1 (the best), 2, ...
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


    # create table MMS_RequestsAuthorization
    # MediaItemKey or ExternalKey cannot be both null
    # DeliveryMethod:
    #		0: download
    #		1: 3gpp streaming
    #		2: RTMP Flash Streaming
    #		3: WindowsMedia Streaming
    # SwitchingType: 0: None, 1: FCS, 2: FTS
    # NetworkCoverage. 0: 2G, 1: 2.5G, 2: 3G
    # IngestedPathName: [<live prefix>]/<customer name>/<territory name>/<relative path>/<content name>
    # ToBeContinued. 0 or 1
    # ForceHTTPRedirection. 0: HTML page, 1: HTTP Redirection
    # TimeToLive is measured in seconds
    create table if not exists MMS_RequestsAuthorization (
            RequestAuthorizationKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            PlayerIP					VARCHAR (16) NULL,
            territoryKey				BIGINT UNSIGNED NOT NULL,
            Shuffling					TINYINT NULL,
            PartyID					VARCHAR (64) NOT NULL,
            MSISDN						VARCHAR (32) NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            ExternalKey				VARCHAR (64) NULL,
            EncodingProfileKey			BIGINT UNSIGNED NULL,
            EncodingLabel				VARCHAR (64) NULL,
            languageCode				VARCHAR (16) NULL,
            DeliveryMethod				TINYINT NULL,
            PreviewSeconds				INT NULL,
            SwitchingType				TINYINT NOT NULL default 0,
            ChargingKey				BIGINT UNSIGNED NULL,
            XSBTTLInSeconds			INT NULL,
            XSBMaxRetries				INT NULL,
            Sequence					VARCHAR (16) NULL,
            ToBeContinued				TINYINT NULL,
            ForceHTTPRedirection		TINYINT NULL,
            NetworkCoverage			TINYINT NULL,
            AuthorizationTimestamp		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            TimeToLive					INT NOT NULL,
            AuthorizationType			VARCHAR (64) NULL,
            AuthorizationData			VARCHAR (128) NULL,
            constraint MMS_RequestsAuthorization_PK PRIMARY KEY (RequestAuthorizationKey), 
            constraint MMS_RequestsAuthorization_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_RequestsAuthorization_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_RequestsAuthorization_FK3 foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
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

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", se.what(): " + se.what()
        );
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
