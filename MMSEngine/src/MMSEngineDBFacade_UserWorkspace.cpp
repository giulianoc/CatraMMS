
#include <random>
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "MMSEngineDBFacade.h"


shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(int64_t workspaceKey)
{
    shared_ptr<MySQLConnection> conn = nullptr;
    string  lastSQLCommand;

    try
    {
		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

		lastSQLCommand =
			"select workspaceKey, name, directoryName, maxStorageInMB, maxEncodingPriority "
			"from MMS_Workspace where workspaceKey = ?";
		shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
		int queryParameterIndex = 1;
		preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
		_logger->info(__FILEREF__ + "@SQL statistics@"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
			+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startSql).count()) + "@"
		);

		shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
		if (resultSet->next())
		{
			workspace->_workspaceKey = resultSet->getInt("workspaceKey");
			workspace->_name = resultSet->getString("name");
			workspace->_directoryName = resultSet->getString("directoryName");
			workspace->_maxStorageInMB = resultSet->getInt("maxStorageInMB");
			workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(
						resultSet->getString("maxEncodingPriority")));

			// getTerritories(workspace);
		}
		else
		{
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			_connectionPool->unborrow(conn);
			conn = nullptr;

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
		conn = nullptr;
    
		return workspace;
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

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(string workspaceName)
{
    shared_ptr<MySQLConnection> conn = nullptr;
    string  lastSQLCommand;

    try
    {
		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

		lastSQLCommand =
			"select workspaceKey, name, directoryName, maxStorageInMB, maxEncodingPriority "
			"from MMS_Workspace where name = ?";
		shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
		int queryParameterIndex = 1;
		preparedStatement->setString(queryParameterIndex++, workspaceName);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
		_logger->info(__FILEREF__ + "@SQL statistics@"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", workspaceName: " + workspaceName
			+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
			+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startSql).count()) + "@"
		);

		shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
		if (resultSet->next())
		{
			workspace->_workspaceKey = resultSet->getInt("workspaceKey");
			workspace->_name = resultSet->getString("name");
			workspace->_directoryName = resultSet->getString("directoryName");
			workspace->_maxStorageInMB = resultSet->getInt("maxStorageInMB");
			workspace->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(
						resultSet->getString("maxEncodingPriority")));

			// getTerritories(workspace);
		}
		else
		{
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			_connectionPool->unborrow(conn);
			conn = nullptr;

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
		conn = nullptr;
    
		return workspace;
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

Json::Value MMSEngineDBFacade::getWorkspaceList(int start, int rows)
{
    string  lastSQLCommand;
	Json::Value workspaceListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

		string field;
		{
			Json::Value requestParametersRoot;

			field = "start";
			requestParametersRoot[field] = start;

			field = "rows";
			requestParametersRoot[field] = rows;

			field = "requestParameters";
			workspaceListRoot[field] = requestParametersRoot;
		}

		Json::Value responseRoot;
		{
			lastSQLCommand =
				"select count(*) from MMS_Workspace ";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

		Json::Value workspacesRoot(Json::arrayValue);
		{
			lastSQLCommand =
				"select workspaceKey, isEnabled, name, maxEncodingPriority, "
				"encodingPeriod, maxIngestionsNumber, maxStorageInMB, languageCode, "
				"DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
				"from MMS_Workspace "
                "limit ? offset ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			bool userAPIKeyInfo = false;
			bool encoders = true;
			while(resultSet->next())
			{
                Json::Value workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, resultSet, userAPIKeyInfo, encoders);

				workspacesRoot.append(workspaceDetailRoot);
			}
		}

		field = "workspaces";
		responseRoot[field] = workspacesRoot;

		field = "response";
		workspaceListRoot[field] = responseRoot;

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

	return workspaceListRoot;
}

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUserAndAddWorkspace(
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    string workspaceName,
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
		string trimWorkspaceName = StringUtils::trim(workspaceName);
		if (trimWorkspaceName == "")
		{
			string errorMessage = string("WorkspaceName is not well formed.")                             
				+ ", workspaceName: " + workspaceName                                                     
			;                                                                                             
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		string trimUserName = StringUtils::trim(userName);
		if (trimUserName == "")
		{
			string errorMessage = string("userName is not well formed.")                             
				+ ", userName: " + userName                                                     
			;                                                                                             
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

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
			// This method is called only in case of MMS user (no ldapEnabled)
            lastSQLCommand = 
                "insert into MMS_User (userKey, name, eMailAddress, password, country, "
				"creationDate, expirationDate, lastSuccessfulLogin) values ("
                "NULL, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, trimUserName);
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);
            preparedStatement->setString(queryParameterIndex++, userPassword);
            preparedStatement->setString(queryParameterIndex++, userCountry);
			char        strExpirationDate [64];
            {
                tm          tmDateTime;
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
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", trimUserName: " + trimUserName
				+ ", userEmailAddress: " + userEmailAddress
				+ ", userPassword: " + userPassword
				+ ", userCountry: " + userCountry
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
        
        userKey = getLastInsertId(conn);

        {
            bool admin = false;
            bool createRemoveWorkspace = true;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            bool editConfiguration = true;
            bool killEncoding = true;
            bool cancelIngestionJob = true;
            bool editEncodersPool = false;
            
            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
                    userKey,
                    admin,
					createRemoveWorkspace,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
					editConfiguration,
					killEncoding,
					cancelIngestionJob,
					editEncodersPool,
                    trimWorkspaceName,
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
    
    tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
            make_tuple(workspaceKey, userKey, confirmationCode);
    
    return workspaceKeyUserKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::createWorkspace(
    int64_t userKey,
    string workspaceName,
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
		string trimWorkspaceName = StringUtils::trim(workspaceName);
		if (trimWorkspaceName == "")
		{
			string errorMessage = string("WorkspaceName is not well formed.")                             
				+ ", workspaceName: " + workspaceName                                                     
			;                                                                                             
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

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
            bool createRemoveWorkspace = true;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            bool editConfiguration = true;
            bool killEncoding = true;
            bool cancelIngestionJob = true;
            bool editEncodersPool = false;
            
            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
                    userKey,
                    admin,
                    createRemoveWorkspace,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
					editConfiguration,
					killEncoding,
					cancelIngestionJob,
					editEncodersPool,
                    trimWorkspaceName,
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
    
    pair<int64_t,string> workspaceKeyAndConfirmationCode = 
            make_pair(workspaceKey, confirmationCode);
    
    return workspaceKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::registerUserAndShareWorkspace(
		bool ldapEnabled,
    bool userAlreadyPresent,
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
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
        
		// In case of ActiveDirectory, this method is called always with userAlreadyPresent true
		// In case of MMS, userAlreadyPresent could be both true or false
        if (ldapEnabled || userAlreadyPresent)
        {
            lastSQLCommand = 
                "select userKey from MMS_User where eMailAddress = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userEmailAddress: " + userEmailAddress
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			string trimUserName = StringUtils::trim(userName);
			if (trimUserName == "")
			{
				string errorMessage = string("userName is not well formed.")                             
					+ ", userName: " + userName                                                     
				;                                                                                             
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

            lastSQLCommand = 
                "insert into MMS_User (userKey, name, eMailAddress, password, country, "
				"creationDate, expirationDate, lastSuccessfulLogin) values ("
                "NULL, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, trimUserName);
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);
            preparedStatement->setString(queryParameterIndex++, userPassword);
            preparedStatement->setString(queryParameterIndex++, userCountry);
			char        strExpirationDate [64];
            {
                tm          tmDateTime;
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
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", trimUserName: " + trimUserName
				+ ", userEmailAddress: " + userEmailAddress
				+ ", userPassword: " + userPassword
				+ ", userCountry: " + userCountry
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

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

                if (createRemoveWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATEREMOVE_WORKSPACE");
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

                if (editConfiguration)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_CONFIGURATION");
                }

                if (killEncoding)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("KILL_ENCODING");
                }

                if (cancelIngestionJob)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CANCEL_INGESTIONJOB");
                }

                if (editEncodersPool)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_ENCODERSPOOL");
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", flags: " + flags
				+ ", workspaceKeyToBeShared: " + to_string(workspaceKeyToBeShared)
				+ ", confirmationCode: " + confirmationCode
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
    
    pair<int64_t,string> userKeyAndConfirmationCode = 
            make_pair(userKey, confirmationCode);
    
    return userKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::registerActiveDirectoryUser(
    string userName,
    string userEmailAddress,
    string userCountry,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	string defaultWorkspaceKeys,
    chrono::system_clock::time_point userExpirationDate
)
{
	int64_t userKey;
	string apiKey;
    string  lastSQLCommand;
    
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
			string userPassword = "";

            lastSQLCommand = 
                "insert into MMS_User (userKey, name, eMailAddress, password, country, "
				"creationDate, expirationDate, lastSuccessfulLogin) values ("
                "NULL, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, userEmailAddress);
            preparedStatement->setString(queryParameterIndex++, userPassword);
            preparedStatement->setString(queryParameterIndex++, userCountry);
			char        strExpirationDate [64];
            {
                tm          tmDateTime;
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
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userName: " + userName
				+ ", userEmailAddress: " + userEmailAddress
				+ ", userPassword: " + userPassword
				+ ", userCountry: " + userCountry
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            userKey = getLastInsertId(conn);
        }
        
		// create the API of the user for each existing Workspace
        {
			_logger->info(__FILEREF__ + "Creating API for the default workspaces"
				+ ", defaultWorkspaceKeys: " + defaultWorkspaceKeys
			);
			stringstream ssDefaultWorkspaceKeys(defaultWorkspaceKeys);                                                                                  
			string defaultWorkspaceKey;
			char separator = ',';
			while (getline(ssDefaultWorkspaceKeys, defaultWorkspaceKey, separator))
			{
				if (!defaultWorkspaceKey.empty())
				{
					int64_t llDefaultWorkspaceKey = stoll(defaultWorkspaceKey);

					_logger->info(__FILEREF__ + "Creating API for the default workspace"
						+ ", llDefaultWorkspaceKey: " + to_string(llDefaultWorkspaceKey)
					);
					string localApiKey = createAPIKeyForActiveDirectoryUser(
						conn,
						userKey,
						userEmailAddress,
						createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
						shareWorkspace, editMedia,
						editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
						llDefaultWorkspaceKey);
					if (apiKey.empty())
						apiKey = localApiKey;
				}
			}
			/*
			apiKey = createAPIKeyForActiveDirectoryUser(
				conn,
				userKey,
				userEmailAddress,
				createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
				shareWorkspace, editMedia,
				editConfiguration, killEncoding,
				defaultWorkspaceKey_1);
			if (defaultWorkspaceKey_2 != -1)
			{
				createAPIKeyForActiveDirectoryUser(
					conn,
					userKey,
					userEmailAddress,
					createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
					shareWorkspace, editMedia,
					editConfiguration, killEncoding,
					defaultWorkspaceKey_2);
			}
			if (defaultWorkspaceKey_3 != -1)
			{
				createAPIKeyForActiveDirectoryUser(
					conn,
					userKey,
					userEmailAddress,
					createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
					shareWorkspace, editMedia,
					editConfiguration, killEncoding,
					defaultWorkspaceKey_3);
			}
			*/
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
    
    
    return make_pair(userKey, apiKey);
}

string MMSEngineDBFacade::createAPIKeyForActiveDirectoryUser(
    int64_t userKey,
    string userEmailAddress,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	int64_t workspaceKey
)
{
    shared_ptr<MySQLConnection> conn = nullptr;
	string apiKey;
    string  lastSQLCommand;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		apiKey = createAPIKeyForActiveDirectoryUser(
			conn,
			userKey,
			userEmailAddress,
			createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
			workspaceKey);

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

	return apiKey;
}

string MMSEngineDBFacade::createAPIKeyForActiveDirectoryUser(
	shared_ptr<MySQLConnection> conn,
    int64_t userKey,
    string userEmailAddress,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	int64_t workspaceKey
)
{
	string apiKey;
    string  lastSQLCommand;
    
    try
    {
		// create the API of the user for each existing Workspace
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

                if (createRemoveWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATEREMOVE_WORKSPACE");
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

                if (editConfiguration)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_CONFIGURATION");
                }

                if (killEncoding)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("KILL_ENCODING");
                }

                if (cancelIngestionJob)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CANCEL_INGESTIONJOB");
                }

                if (editEncodersPool)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_ENCODERSPOOL");
                }
            }

            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
			default_random_engine e(seed);

			string sourceApiKey = userEmailAddress + "__SEP__" + to_string(e());
			apiKey = Encrypt::encrypt(sourceApiKey);

			bool isOwner = false;
			bool isDefault = false;
          
			lastSQLCommand = 
				"insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"flags, creationDate, expirationDate) values ("
				"?, ?, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
			shared_ptr<sql::PreparedStatement> preparedStatementAPIKey (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatementAPIKey->setString(queryParameterIndex++, apiKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, userKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isOwner);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isDefault);
			preparedStatementAPIKey->setString(queryParameterIndex++, flags);
			char        strExpirationDate [64];
			{
				chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now() + chrono::hours(24 * 365 * 10);

				tm          tmDateTime;
				time_t utcTime = chrono::system_clock::to_time_t(apiKeyExpirationDate);

				localtime_r (&utcTime, &tmDateTime);

				sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);

				preparedStatementAPIKey->setString(queryParameterIndex++, strExpirationDate);
			}

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatementAPIKey->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", apiKey: " + apiKey
				+ ", userKey: " + to_string(userKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", isOwner: " + to_string(isOwner)
				+ ", isDefault: " + to_string(isDefault)
				+ ", flags: " + flags
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    
    return apiKey;
}

pair<int64_t,string> MMSEngineDBFacade::addWorkspace(
        shared_ptr<MySQLConnection> conn,
        int64_t userKey,
        bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
        bool shareWorkspace, bool editMedia,
        bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
        string workspaceName,
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
			string workspaceDirectoryName = "tempName";
            
            lastSQLCommand = 
                    "insert into MMS_Workspace ("
                    "workspaceKey, creationDate, name, directoryName, workspaceType, deliveryURL, isEnabled, maxEncodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB, languageCode) values ("
                    "NULL,         NOW(),         ?,    ?,             ?,             ?,           ?,         ?,                   ?,              ?,                   ?,              ?)";

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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceName: " + workspaceName
				+ ", workspaceDirectoryName: " + workspaceDirectoryName
				+ ", workspaceType: " + to_string(static_cast<int>(workspaceType))
				+ ", deliveryURL: " + deliveryURL
				+ ", enabled: " + to_string(enabled)
				+ ", maxEncodingPriority: " + MMSEngineDBFacade::toString(maxEncodingPriority)
				+ ", encodingPeriod: " + toString(encodingPeriod)
				+ ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
				+ ", maxStorageInMB: " + to_string(maxStorageInMB)
				+ ", languageCode: " + languageCode
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        workspaceKey = getLastInsertId(conn);

		{
			lastSQLCommand = 
				"update MMS_Workspace set directoryName = ? where workspaceKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, to_string(workspaceKey));
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

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

                if (createRemoveWorkspace)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CREATEREMOVE_WORKSPACE");
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

                if (editConfiguration)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_CONFIGURATION");
                }

                if (killEncoding)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("KILL_ENCODING");
                }

                if (cancelIngestionJob)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("CANCEL_INGESTIONJOB");
                }

                if (editEncodersPool)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("EDIT_ENCODERSPOOL");
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", flags: " + flags
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confirmationCode: " + confirmationCode
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        {
            lastSQLCommand = 
                    "insert into MMS_WorkspaceMoreInfo (workspaceKey, currentDirLevel1, currentDirLevel2, currentDirLevel3, startDateTime, endDateTime, currentIngestionsNumber) values ("
                    "?, 0, 0, 0, NOW(), NOW(), 0)";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

		// insert EncodersPool with label null.
		// This is used for the default encoders pool for the actual workspace
		// The default encoders will be all the internal encoders associated to the workspace
		// These encoders will not be saved in MMS_EncoderEncodersPoolMapping but they
		// will be retrieved directly by MMS_EncoderWorkspaceMapping
        {
            lastSQLCommand = 
                "insert into MMS_EncodersPool(workspaceKey, label, lastEncoderIndexUsed) values ( "
                "?, NULL, 0)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + "null"
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			// encodersPoolKey = getLastInsertId(conn);
        }

        {
            lastSQLCommand = 
                "insert into MMS_ContentProvider (contentProviderKey, workspaceKey, name) values ("
                "NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, _defaultContentProviderName);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", _defaultContentProviderName: " + _defaultContentProviderName
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                "select userKey, flags, workspaceKey, isSharedWorkspace from MMS_ConfirmationCode "
				"where confirmationCode = ? and DATE_ADD(creationDate, INTERVAL ? DAY) >= NOW()";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, confirmationCode);
            preparedStatement->setInt(queryParameterIndex++, _confirmationCodeRetentionInDays);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confirmationCode: " + confirmationCode
				+ ", _confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", enabled: " + to_string(enabled)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
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
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			bool isDefault = false;
            
            lastSQLCommand = 
                "insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"flags, creationDate, expirationDate) values ("
                "?, ?, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt(queryParameterIndex++, isOwner);
            preparedStatement->setInt(queryParameterIndex++, isDefault);
            preparedStatement->setString(queryParameterIndex++, flags);
			char        strExpirationDate [64];
            {
                chrono::system_clock::time_point apiKeyExpirationDate =
                        chrono::system_clock::now() + chrono::hours(24 * 365 * 10);     // chrono::system_clock::time_point userExpirationDate

                tm          tmDateTime;
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", apiKey: " + apiKey
				+ ", userKey: " + to_string(userKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", isOwner: " + to_string(isOwner)
				+ ", isDefault: " + to_string(isDefault)
				+ ", flags: " + flags
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
		conn = nullptr;

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

void MMSEngineDBFacade::deleteWorkspace(
		int64_t userKey,
		int64_t workspaceKey)
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
        
        // check if ADMIN flag is already present
        bool admin = false;
		bool isOwner = false;
        {
            lastSQLCommand = 
                "select isOwner, flags from MMS_APIKey where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                string flags = resultSet->getString("flags");
                
                admin = flags.find("ADMIN") == string::npos ? false : true;
                isOwner = resultSet->getInt("isOwner") == 1 ? "true" : "false";
            }
        }

		if (!isOwner)
		{
			string errorMessage = __FILEREF__ + "The user requesting the deletion does not have the ownership rights and the delete cannot be done"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

        {
			// in all the tables depending from Workspace we have 'on delete cascade'
			// So all should be removed automatically from DB
            lastSQLCommand = 
                "delete from MMS_Workspace where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
    
    // return workspaceKeyUserKeyAndConfirmationCode;
}

tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
	MMSEngineDBFacade::checkAPIKey (string apiKey)
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", apiKey: " + apiKey
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    return make_tuple(userKey, workspace,
        flags.find("ADMIN") == string::npos ? false : true,
        flags.find("CREATEREMOVE_WORKSPACE") == string::npos ? false : true,
        flags.find("INGEST_WORKFLOW") == string::npos ? false : true,
        flags.find("CREATE_PROFILES") == string::npos ? false : true,
        flags.find("DELIVERY_AUTHORIZATION") == string::npos ? false : true,
        flags.find("SHARE_WORKSPACE") == string::npos ? false : true,
        flags.find("EDIT_MEDIA") == string::npos ? false : true,
        flags.find("EDIT_CONFIGURATION") == string::npos ? false : true,
        flags.find("KILL_ENCODING") == string::npos ? false : true,
        flags.find("CANCEL_INGESTIONJOB") == string::npos ? false : true,
        flags.find("EDIT_ENCODERSPOOL") == string::npos ? false : true
    );
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", eMailAddress: " + eMailAddress
				+ ", password: " + password
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				int64_t userKey = resultSet->getInt64("userKey");

                string field = "userKey";
                loginDetailsRoot[field] = userKey;

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

				{
					lastSQLCommand = 
						"update MMS_User set lastSuccessfulLogin = NOW() "
						"where userKey = ?";

					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setInt64(queryParameterIndex++, userKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", userKey: " + to_string(userKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done"
								+ ", userKey: " + to_string(userKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", lastSQLCommand: " + lastSQLCommand
						;
						_logger->warn(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
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
                "select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, w.encodingPeriod, "
					"w.maxIngestionsNumber, w.maxStorageInMB, w.languageCode, "
					"a.apiKey, a.isOwner, a.isDefault, a.flags, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			bool userAPIKeyInfo = true;
			bool encoders = true;
            while (resultSet->next())
            {
                Json::Value workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, resultSet, userAPIKeyInfo, encoders);

                workspaceDetailsRoot.append(workspaceDetailRoot);                        
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
    
    return workspaceDetailsRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceDetailsRoot (
	shared_ptr<MySQLConnection> conn,
	shared_ptr<sql::ResultSet> resultSet,
	bool userAPIKeyInfo,
	bool encoders)
{
    Json::Value     workspaceDetailRoot;

    try
    {
		int64_t workspaceKey = resultSet->getInt64("workspaceKey");

		string field = "workspaceKey";
		workspaceDetailRoot[field] = workspaceKey;
                
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

		{
			int64_t workSpaceUsageInBytes;

			pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB =
				getWorkspaceUsage(conn, workspaceKey);
			tie(workSpaceUsageInBytes, ignore) = workSpaceUsageInBytesAndMaxStorageInMB;              

			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "workSpaceUsageInMB";
			workspaceDetailRoot[field] = workSpaceUsageInMB;
		}

		field = "languageCode";
		workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("languageCode"));

		field = "creationDate";
		workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

		if (userAPIKeyInfo)
		{
			Json::Value     userAPIKeyRoot;

			field = "apiKey";
			userAPIKeyRoot[field] = static_cast<string>(resultSet->getString("apiKey"));

			field = "owner";
			userAPIKeyRoot[field] = resultSet->getInt("isOwner") == 1 ? "true" : "false";

			field = "default";
			userAPIKeyRoot[field] = resultSet->getInt("isDefault") == 1
				? "true" : "false";

			string flags = resultSet->getString("flags");
                
			field = "admin";
			userAPIKeyRoot[field] = flags.find("ADMIN") == string::npos ? false : true;

			field = "createRemoveWorkspace";
			userAPIKeyRoot[field] = flags.find("CREATEREMOVE_WORKSPACE") == string::npos 
				? false : true;

			field = "ingestWorkflow";
			userAPIKeyRoot[field] = flags.find("INGEST_WORKFLOW") == string::npos
				? false : true;

			field = "createProfiles";
			userAPIKeyRoot[field] = flags.find("CREATE_PROFILES") == string::npos
				? false : true;

			field = "deliveryAuthorization";
			userAPIKeyRoot[field] = flags.find("DELIVERY_AUTHORIZATION") == string::npos 
				? false : true;

			field = "shareWorkspace";
			userAPIKeyRoot[field] = flags.find("SHARE_WORKSPACE") == string::npos
				? false : true;

			field = "editMedia";
			userAPIKeyRoot[field] = flags.find("EDIT_MEDIA") == string::npos
				? false : true;
                
			field = "editConfiguration";
			userAPIKeyRoot[field] = flags.find("EDIT_CONFIGURATION") == string::npos
				? false : true;

			field = "killEncoding";
			userAPIKeyRoot[field] = flags.find("KILL_ENCODING") == string::npos
				? false : true;

			field = "cancelIngestionJob";
			userAPIKeyRoot[field] = flags.find("CANCEL_INGESTIONJOB") == string::npos
				? false : true;

			field = "editEncodersPool";
			userAPIKeyRoot[field] = flags.find("EDIT_ENCODERSPOOL") == string::npos
				? false : true;

			field = "userAPIKey";
			workspaceDetailRoot[field] = userAPIKeyRoot;
		}

		if (encoders)
		{
			Json::Value encodersRoot(Json::arrayValue);
			{
				string lastSQLCommand =
					"select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
					"e.serverName, e.port, e.maxTranscodingCapability, e.maxLiveProxiesCapabilities, "
					"e.maxLiveRecordingCapabilities "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm "
					"where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatementEncoders (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatementEncoders->setInt64(queryParameterIndex++, workspaceKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSetEncoders (
					preparedStatementEncoders->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", resultSetEncoders->rowsCount: " + to_string(resultSetEncoders->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				while(resultSetEncoders->next())
				{
					Json::Value encoderRoot = getEncoderRoot(resultSetEncoders);

					encodersRoot.append(encoderRoot);
				}
			}

			field = "encoders";
			workspaceDetailRoot[field] = encodersRoot;
		}
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    
    return workspaceDetailRoot;
}

Json::Value MMSEngineDBFacade::updateWorkspaceDetails (
        int64_t userKey,
        int64_t workspaceKey,
        bool newEnabled, string newName, string newMaxEncodingPriority,
        string newEncodingPeriod, int64_t newMaxIngestionsNumber,
        int64_t newMaxStorageInMB, string newLanguageCode,
        bool newCreateRemoveWorkspace, bool newIngestWorkflow, bool newCreateProfiles,
        bool newDeliveryAuthorization, bool newShareWorkspace,
        bool newEditMedia, bool newEditConfiguration, bool newKillEncoding, bool newCancelIngestionJob,
		bool newEditEncodersPool)
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
		bool isOwner = false;
        {
            lastSQLCommand = 
                "select isOwner, flags from MMS_APIKey where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                string flags = resultSet->getString("flags");
                
                admin = flags.find("ADMIN") == string::npos ? false : true;
                isOwner = resultSet->getInt("isOwner") == 1 ? "true" : "false";
            }
            else
            {
                string errorMessage = __FILEREF__ + "user/workspace are not found"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

		if (!admin && !isOwner)
		{
			string errorMessage = __FILEREF__ + "The user requesting the update does not have neither the admin nor the ownership rights and the update cannot be done"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		// some fields (isEnabled, maxEncodingPriority, encodingPeriod, maxIngestionsNumber, maxStorageInMB) can be update only by an Administrator
		if (admin)
        {
            lastSQLCommand = 
                "update MMS_Workspace set isEnabled = ?, name = ?, maxEncodingPriority = ?, encodingPeriod = ?, maxIngestionsNumber = ?, "
                "maxStorageInMB = ?, languageCode = ? "
                "where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, newEnabled);
            preparedStatement->setString(queryParameterIndex++, newName);
            preparedStatement->setString(queryParameterIndex++, newMaxEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, newEncodingPeriod);
            preparedStatement->setInt64(queryParameterIndex++, newMaxIngestionsNumber);
            preparedStatement->setInt64(queryParameterIndex++, newMaxStorageInMB);
            preparedStatement->setString(queryParameterIndex++, newLanguageCode);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEnabled: " + to_string(newEnabled)
				+ ", newName: " + newName
				+ ", newMaxEncodingPriority: " + newMaxEncodingPriority
				+ ", newEncodingPeriod: " + newEncodingPeriod
				+ ", newMaxIngestionsNumber: " + to_string(newMaxIngestionsNumber)
				+ ", newMaxStorageInMB: " + to_string(newMaxStorageInMB)
				+ ", newLanguageCode: " + newLanguageCode
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newEnabled: " + to_string(newEnabled)
                        + ", newName: " + newName
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
		else if (isOwner)
        {
            lastSQLCommand = 
                "update MMS_Workspace set name = ?, languageCode = ? "
                "where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, newName);
            preparedStatement->setString(queryParameterIndex++, newLanguageCode);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newName: " + newName
				+ ", newLanguageCode: " + newLanguageCode
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newName: " + newName
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
            if (newCreateRemoveWorkspace)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("CREATEREMOVE_WORKSPACE");
            }
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
            if (newEditConfiguration)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("EDIT_CONFIGURATION");
            }
            if (newKillEncoding)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("KILL_ENCODING");
            }

            if (newCancelIngestionJob)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("CANCEL_INGESTIONJOB");
            }

            if (newEditEncodersPool)
            {
                if (flags != "")
                    flags.append(",");
                flags.append("EDIT_ENCODERSPOOL");
            }

            lastSQLCommand = 
                "update MMS_APIKey set flags = ? "
                "where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, flags);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", flags: " + flags
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userKey: " + to_string(userKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

        field = "workspaceName";
        workspaceDetailRoot[field] = newName;

        field = "maxEncodingPriority";
        workspaceDetailRoot[field] = newMaxEncodingPriority;

        field = "encodingPeriod";
        workspaceDetailRoot[field] = newEncodingPeriod;

        field = "maxIngestionsNumber";
        workspaceDetailRoot[field] = newMaxIngestionsNumber;

        field = "maxStorageInMB";
        workspaceDetailRoot[field] = newMaxStorageInMB;

		{
			int64_t workSpaceUsageInBytes;

			pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB = getWorkspaceUsage(conn, workspaceKey);
			tie(workSpaceUsageInBytes, ignore) = workSpaceUsageInBytesAndMaxStorageInMB;              
                                                                                                            
			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "workSpaceUsageInMB";
			workspaceDetailRoot[field] = workSpaceUsageInMB;
		}

        field = "languageCode";
        workspaceDetailRoot[field] = newLanguageCode;

        field = "admin";
        workspaceDetailRoot[field] = admin ? true : false;

        field = "createRemoveWorkspace";
        workspaceDetailRoot[field] = newCreateRemoveWorkspace ? true : false;

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
        
        field = "editConfiguration";
        workspaceDetailRoot[field] = newEditConfiguration ? true : false;
        
        field = "killEncoding";
        workspaceDetailRoot[field] = newKillEncoding ? true : false;
        
        field = "cancelIngestionJob";
        workspaceDetailRoot[field] = newCancelIngestionJob ? true : false;
        
        field = "editEncodersPool";
        workspaceDetailRoot[field] = newEditEncodersPool ? true : false;

        {
            lastSQLCommand = 
                "select w.name, a.apiKey, a.isOwner, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w where a.workspaceKey = w.workspaceKey and a.workspaceKey = ? and a.userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				/*
                field = "workspaceName";
                workspaceDetailRoot[field] = static_cast<string>(resultSet->getString("name"));
				*/

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
    
    return workspaceDetailRoot;
}

Json::Value MMSEngineDBFacade::setWorkspaceAsDefault (
        int64_t userKey,
        int64_t workspaceKey,
		int64_t workspaceKeyToBeSetAsDefault)
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

		string apiKey;
        {
            lastSQLCommand = 
                "select apiKey from MMS_APIKey where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKeyToBeSetAsDefault);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKeyToBeSetAsDefault: " + to_string(workspaceKeyToBeSetAsDefault)
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                apiKey = resultSet->getString("apiKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "user/workspace are not found"
                    + ", workspaceKey: " + to_string(workspaceKeyToBeSetAsDefault)
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
            lastSQLCommand = 
                "update MMS_APIKey set isDefault = 0 "
                "where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        {
            lastSQLCommand = 
                "update MMS_APIKey set isDefault = 1 "
                "where apiKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", apiKey: " + apiKey
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    return workspaceDetailRoot;
}

pair<int64_t,int64_t> MMSEngineDBFacade::getWorkspaceUsage(
        int64_t workspaceKey)
{
	pair<int64_t,int64_t>	workspaceUsage;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		workspaceUsage = getWorkspaceUsage(conn, workspaceKey);

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
    
    return workspaceUsage;
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
                "where mi.mediaItemKey = pp.mediaItemKey and mi.workspaceKey = ? "
				"and externalReadOnlyStorage = 0";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    pair<string, string> emailAddressAndName = make_pair(emailAddress, name);
            
    return emailAddressAndName;
}

Json::Value MMSEngineDBFacade::updateUser (
		bool ldapEnabled,
        int64_t userKey,
        string name, 
        string email, 
        string country,
		bool passwordChanged,
		string newPassword,
		string oldPassword)
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

		string savedPassword;
        {
            lastSQLCommand = 
                "select password from MMS_User where userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				savedPassword = resultSet->getString("password");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User is not present"
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
			int rowsUpdated;

			if (!ldapEnabled)
			{
				if (passwordChanged)
				{
					if (savedPassword != oldPassword
							|| newPassword == "")
					{
						string errorMessage = __FILEREF__ + "old password is wrong or newPassword is not valid"
							+ ", userKey: " + to_string(userKey)
						;
						_logger->warn(errorMessage);

						throw runtime_error(errorMessage);
					}

					lastSQLCommand = 
						"update MMS_User set country = ?, "
						// "expirationDate = convert_tz(STR_TO_DATE(?,'%Y-%m-%dT%H:%i:%SZ'), '+00:00', @@session.time_zone), "
						"name = ?, eMailAddress = ?, password = ? "
						"where userKey = ?";
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setString(queryParameterIndex++, country);
					// preparedStatement->setString(queryParameterIndex++, expirationDate);
					preparedStatement->setString(queryParameterIndex++, name);
					preparedStatement->setString(queryParameterIndex++, email);
					preparedStatement->setString(queryParameterIndex++, newPassword);
					preparedStatement->setInt64(queryParameterIndex++, userKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					rowsUpdated = preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", country: " + country
						+ ", name: " + name
						+ ", email: " + email
						+ ", newPassword: " + newPassword
						+ ", userKey: " + to_string(userKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}
				else
				{
					lastSQLCommand = 
						"update MMS_User set country = ?, "
						// "expirationDate = convert_tz(STR_TO_DATE(?,'%Y-%m-%dT%H:%i:%SZ'), '+00:00', @@session.time_zone), "
						"name = ?, eMailAddress = ? "
						"where userKey = ?";
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setString(queryParameterIndex++, country);
					// preparedStatement->setString(queryParameterIndex++, expirationDate);
					preparedStatement->setString(queryParameterIndex++, name);
					preparedStatement->setString(queryParameterIndex++, email);
					preparedStatement->setInt64(queryParameterIndex++, userKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					rowsUpdated = preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", country: " + country
						+ ", name: " + name
						+ ", email: " + email
						+ ", userKey: " + to_string(userKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}
			}
			else // if (ldapEnabled)
			{
				lastSQLCommand = 
					"update MMS_User set country = ? "
					// "expirationDate = convert_tz(STR_TO_DATE(?,'%Y-%m-%dT%H:%i:%SZ'), '+00:00', @@session.time_zone) "
					"where userKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, country);
				// preparedStatement->setString(queryParameterIndex++, expirationDate);
				preparedStatement->setInt64(queryParameterIndex++, userKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", country: " + country
					+ ", userKey: " + to_string(userKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}

            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", userKey: " + to_string(userKey)
                        + ", name: " + name
                        + ", country: " + country
                        + ", email: " + email
                        // + ", password: " + password
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                
        field = "ldapEnabled";
        loginDetailsRoot[field] = ldapEnabled;

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
    
    return loginDetailsRoot;
}

