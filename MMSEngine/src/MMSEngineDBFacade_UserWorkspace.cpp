
#include <random>
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "MMSEngineDBFacade.h"
#include "JSONUtils.h"


shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(int64_t workspaceKey)
{
    shared_ptr<MySQLConnection> conn = nullptr;
    string  lastSQLCommand;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
		conn = connectionPool->borrow();	
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
			connectionPool->unborrow(conn);
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
		connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
}

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(string workspaceName)
{
    shared_ptr<MySQLConnection> conn = nullptr;
    string  lastSQLCommand;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
		conn = connectionPool->borrow();	
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
			connectionPool->unborrow(conn);
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
		connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
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
    string          userRegistrationCode;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
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

        conn = connectionPool->borrow();	
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
                "NULL, ?, ?, ?, ?, NOW(), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
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
			string trimWorkspaceName = StringUtils::trim(workspaceName);
			if (trimWorkspaceName == "")
			{
				string errorMessage = string("WorkspaceName is not well formed.")
					+ ", workspaceName: " + workspaceName
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			// defaults when a new user is added/registered
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
            bool editEncodersPool = true;
            bool applicationRecorder = true;

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
					applicationRecorder,
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
			userRegistrationCode = workspaceKeyAndConfirmationCode.second;
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
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    
	tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
		make_tuple(workspaceKey, userKey, userRegistrationCode);

	return workspaceKeyUserKeyAndConfirmationCode;
}

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUserAndShareWorkspace(
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    string shareWorkspaceCode,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    int64_t         userKey;
    string          userRegistrationCode;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
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

        conn = connectionPool->borrow();	
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
                "NULL, ?, ?, ?, ?, NOW(), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
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
			// this is a registration of a user because of a share workspace
			{
				lastSQLCommand = 
					"select workspaceKey from MMS_Code "
					"where code = ? and userEmail = ? and type = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, shareWorkspaceCode);
				preparedStatement->setString(queryParameterIndex++, userEmailAddress);
				preparedStatement->setString(queryParameterIndex++, toString(CodeType::ShareWorkspace));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", code: " + shareWorkspaceCode
					+ ", userEmail: " + userEmailAddress
					+ ", type: " + toString(CodeType::ShareWorkspace)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					workspaceKey = resultSet->getInt64("workspaceKey");
				}
				else
				{
					string errorMessage = __FILEREF__ + "Share Workspace Code not found"
						+ ", shareWorkspaceCode: " + shareWorkspaceCode
						+ ", userEmailAddress: " + userEmailAddress
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			{
				lastSQLCommand = 
					"update MMS_Code set userKey = ?, type = ? "
					"where code = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, userKey);
				preparedStatement->setString(queryParameterIndex++,
					toString(CodeType::UserRegistrationComingFromShareWorkspace));
				preparedStatement->setString(queryParameterIndex++, shareWorkspaceCode);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", userKey: " + to_string(userKey)
					+ ", type: " + toString(CodeType::UserRegistrationComingFromShareWorkspace)
					+ ", code: " + shareWorkspaceCode
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "Code update failed"
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", shareWorkspaceCode: " + shareWorkspaceCode
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			userRegistrationCode = shareWorkspaceCode;
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
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    
	tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
		make_tuple(workspaceKey, userKey, userRegistrationCode);

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
	bool admin,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         workspaceKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

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

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
            bool createRemoveWorkspace = true;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            bool editConfiguration = true;
            bool killEncoding = true;
            bool cancelIngestionJob = true;
			bool editEncodersPool = true;
			bool applicationRecorder = true;

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
					applicationRecorder,
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
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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

string MMSEngineDBFacade::createCode(
    int64_t workspaceKey,
    int64_t userKey, string userEmail,
	CodeType codeType,
    bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder
)
{
	string		code;

	shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		code = createCode(conn, workspaceKey, userKey, userEmail, codeType,
			admin, createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

    return code;
}

string MMSEngineDBFacade::createCode(
	shared_ptr<MySQLConnection> conn,
    int64_t workspaceKey,
    int64_t userKey, string userEmail,
	CodeType codeType,
    bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder
)
{
	string		code;

	string		lastSQLCommand;

    try
    {
		unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
		default_random_engine e(seed);
		code = to_string(e());

        {
			Json::Value permissionsRoot;
            {
				permissionsRoot["admin"] = admin;

				permissionsRoot["createRemoveWorkspace"] = createRemoveWorkspace;
				permissionsRoot["ingestWorkflow"] = ingestWorkflow;
				permissionsRoot["createProfiles"] = createProfiles;
				permissionsRoot["deliveryAuthorization"] = deliveryAuthorization;
				permissionsRoot["shareWorkspace"] = shareWorkspace;
				permissionsRoot["editMedia"] = editMedia;
				permissionsRoot["editConfiguration"] = editConfiguration;
				permissionsRoot["killEncoding"] = killEncoding;
				permissionsRoot["cancelIngestionJob"] = cancelIngestionJob;
				permissionsRoot["editEncodersPool"] = editEncodersPool;
				permissionsRoot["applicationRecorder"] = applicationRecorder;
			}
			string permissions = JSONUtils::toString(permissionsRoot);

			try
			{
				lastSQLCommand = 
					"insert into MMS_Code (code, workspaceKey, userKey, userEmail, "
					"type, permissions, creationDate) values ("
					"?, ?, ?, ?, ?, ?, NOW())";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, code);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				if (userKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, userKey);
				if (userEmail == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, userEmail);
				preparedStatement->setString(queryParameterIndex++, toString(codeType));
				preparedStatement->setString(queryParameterIndex++, permissions);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", code: " + code
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", userKey: " + to_string(userKey)
					+ ", userEmail: " + userEmail
					+ ", type: " + toString(codeType)
					+ ", permissions: " + permissions
					+ ", elapsed (secs): @"
						+ to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}
			catch(sql::SQLException se)
			{
				string exceptionMessage(se.what());

				throw se;
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
    
    return code;
}

pair<int64_t,string> MMSEngineDBFacade::registerActiveDirectoryUser(
    string userName,
    string userEmailAddress,
    string userCountry,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder,
	string defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
    chrono::system_clock::time_point userExpirationDate
)
{
	int64_t userKey;
	string apiKey;
    string  lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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
                "NULL, ?, ?, ?, ?, NOW(), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), NULL)";
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
						applicationRecorder,
						llDefaultWorkspaceKey, expirationInDaysWorkspaceDefaultValue);
					if (apiKey.empty())
						apiKey = localApiKey;
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
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
	bool applicationRecorder,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
    shared_ptr<MySQLConnection> conn = nullptr;
	string apiKey;
    string  lastSQLCommand;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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
			applicationRecorder,
			workspaceKey, expirationInDaysWorkspaceDefaultValue);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
	bool applicationRecorder,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
	string apiKey;
    string  lastSQLCommand;
    
    try
    {
		// create the API of the user for each existing Workspace
        {
			Json::Value permissionsRoot;
            {
                bool admin = false;

				permissionsRoot["admin"] = admin;

				permissionsRoot["createRemoveWorkspace"] = createRemoveWorkspace;
				permissionsRoot["ingestWorkflow"] = ingestWorkflow;
				permissionsRoot["createProfiles"] = createProfiles;
				permissionsRoot["deliveryAuthorization"] = deliveryAuthorization;
				permissionsRoot["shareWorkspace"] = shareWorkspace;
				permissionsRoot["editMedia"] = editMedia;
				permissionsRoot["editConfiguration"] = editConfiguration;
				permissionsRoot["killEncoding"] = killEncoding;
				permissionsRoot["cancelIngestionJob"] = cancelIngestionJob;
				permissionsRoot["editEncodersPool"] = editEncodersPool;
				permissionsRoot["applicationRecorder"] = applicationRecorder;
            }
			string permissions = JSONUtils::toString(permissionsRoot);

            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
			default_random_engine e(seed);

			string sourceApiKey = userEmailAddress + "__SEP__" + to_string(e());
			apiKey = Encrypt::opensslEncrypt(sourceApiKey);

			bool isOwner = false;
			bool isDefault = false;
          
			lastSQLCommand = 
				"insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
				"?, ?, ?, ?, ?, ?, NOW(), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
			shared_ptr<sql::PreparedStatement> preparedStatementAPIKey (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatementAPIKey->setString(queryParameterIndex++, apiKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, userKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isOwner);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isDefault);
			preparedStatementAPIKey->setString(queryParameterIndex++, permissions);
			char        strExpirationDate [64];
			{
				chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now()
					+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

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
				+ ", permissions: " + permissions
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			addWorkspaceForAdminUsers(conn,
				workspaceKey, expirationInDaysWorkspaceDefaultValue);
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
		bool applicationRecorder,
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
	Json::Value		workspaceRoot;
    
    string      lastSQLCommand;
    
    try
    {
        {
            bool enabled = false;
			string workspaceDirectoryName = "tempName";
            
            lastSQLCommand = 
                    "insert into MMS_Workspace ("
                    "workspaceKey, creationDate, name, directoryName, workspaceType, "
					"deliveryURL, isEnabled, maxEncodingPriority, encodingPeriod, "
					"maxIngestionsNumber, maxStorageInMB, languageCode) values ("
                    "NULL,         NOW(),         ?,    ?,             ?, "
					"?,           ?,         ?,                   ?, "
					"?,                   ?,              ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, workspaceName);
            preparedStatement->setString(queryParameterIndex++, workspaceDirectoryName);
            preparedStatement->setInt(queryParameterIndex++,
				static_cast<int>(workspaceType));
            if (deliveryURL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryURL);
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(maxEncodingPriority));
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
				+ ", maxEncodingPriority: "
					+ MMSEngineDBFacade::toString(maxEncodingPriority)
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
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

		confirmationCode = MMSEngineDBFacade::createCode(
			conn,
			workspaceKey,
			userKey, "",	// userEmail,
			CodeType::UserRegistration,
			admin, createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
			applicationRecorder);

        {
            lastSQLCommand = 
                    "insert into MMS_WorkspaceMoreInfo (workspaceKey, currentDirLevel1, "
					"currentDirLevel2, currentDirLevel3, startDateTime, endDateTime, "
					"currentIngestionsNumber) values ("
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
                "insert into MMS_EncodersPool(workspaceKey, label, lastEncoderIndexUsed) "
				"values (?, NULL, 0)";

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
                "insert into MMS_ContentProvider (contentProviderKey, workspaceKey, name) "
				"values (NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++,
				_defaultContentProviderName);

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
    string confirmationCode, int expirationInDaysWorkspaceDefaultValue
)
{
    string      apiKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        int64_t     userKey;
        string		permissions;
        int64_t     workspaceKey;
		CodeType codeType;
        {
            lastSQLCommand = 
                "select userKey, permissions, workspaceKey, type "
				"from MMS_Code "
				"where code = ? and type in (?, ?) and "
				"DATE_ADD(creationDate, INTERVAL ? DAY) >= NOW()";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, confirmationCode);
            preparedStatement->setString(queryParameterIndex++, toString(CodeType::UserRegistration));
            preparedStatement->setString(queryParameterIndex++, toString(CodeType::UserRegistrationComingFromShareWorkspace));
            preparedStatement->setInt(queryParameterIndex++, _confirmationCodeRetentionInDays);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confirmationCode: " + confirmationCode
				+ ", type: " + toString(CodeType::UserRegistration)
				+ ", type: " + toString(CodeType::UserRegistrationComingFromShareWorkspace)
				+ ", _confirmationCodeRetentionInDays: "
					+ to_string(_confirmationCodeRetentionInDays)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
				permissions = resultSet->getString("permissions");
                workspaceKey = resultSet->getInt64("workspaceKey");
				codeType = MMSEngineDBFacade::toCodeType(resultSet->getString("type"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "Confirmation Code not found or expired"
                    + ", confirmationCode: " + confirmationCode
					+ ", type: " + toString(codeType)
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
			// questa condizione fa si che solo nel caso di UserRegistration il Workspace sia enabled,
			// mentre nel caso di UserRegistration proveniente da un ShareWorkspace (UserRegistrationComingFromShareWorkspace)
			// non bisogna fare nulla sul Workspace di cui non si è proprietario
            if (codeType == CodeType::UserRegistration)
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
            apiKey = Encrypt::opensslEncrypt(sourceApiKey);
			_logger->info(__FILEREF__ + "Encrypt::opensslEncrypt"
				+ ", sourceApiKey: " + sourceApiKey
				+ ", apiKey: '" + apiKey + "'"
			);

            bool isOwner = codeType == CodeType::UserRegistration ? true : false;
			bool isDefault = false;
            
            lastSQLCommand = 
                "insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
                "                        ?,      ?,       ?,            ?,       ?, "
				"?,           NOW(),        STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt(queryParameterIndex++, isOwner);
            preparedStatement->setInt(queryParameterIndex++, isDefault);
            preparedStatement->setString(queryParameterIndex++, permissions);
			char        strExpirationDate [64];
            {
                chrono::system_clock::time_point apiKeyExpirationDate =
                        chrono::system_clock::now()
						+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

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
				+ ", permissions: " + permissions
				+ ", strExpirationDate: " + strExpirationDate
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			addWorkspaceForAdminUsers(conn,
				workspaceKey, expirationInDaysWorkspaceDefaultValue);
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
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }
        
        throw e;
    }    
}

void MMSEngineDBFacade::addWorkspaceForAdminUsers(
	shared_ptr<MySQLConnection> conn,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
	string apiKey;
    string  lastSQLCommand;

    try
    {
		Json::Value permissionsRoot;
		{
			bool admin = true;

			permissionsRoot["admin"] = admin;
		}
		string permissions = JSONUtils::toString(permissionsRoot);

		bool isOwner = false;
		bool isDefault = false;

		for(string adminEmailAddress: _adminEmailAddresses)
        {
			int64_t userKey;
			{
				lastSQLCommand = 
					"select userKey from MMS_User "
					"where eMailAddress = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, adminEmailAddress);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", adminEmailAddress: " + adminEmailAddress
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
					string errorMessage = __FILEREF__ + "Admin email address was not found"
						+ ", adminEmailAddress: " + adminEmailAddress
						+ ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}

			bool apiKeyAlreadyPresentForAdminUser = false;
			{
				lastSQLCommand = 
					"select count(*) from MMS_APIKey "
					"where userKey = ? and workspaceKey = ?";

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
					if (resultSet->getInt64(1) != 0)
						apiKeyAlreadyPresentForAdminUser = true;
					else
						apiKeyAlreadyPresentForAdminUser = false;
				}
				else
				{
					string errorMessage = __FILEREF__ + "count(*) has to return a row"
						+ ", userKey: " + to_string(userKey)
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}

			if (apiKeyAlreadyPresentForAdminUser)
				continue;

			unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
			default_random_engine e(seed);

			string sourceApiKey = adminEmailAddress + "__SEP__" + to_string(e());
			apiKey = Encrypt::opensslEncrypt(sourceApiKey);

			lastSQLCommand = 
				"insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
				"?, ?, ?, ?, ?, ?, NOW(), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
			shared_ptr<sql::PreparedStatement> preparedStatementAPIKey (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatementAPIKey->setString(queryParameterIndex++, apiKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, userKey);
			preparedStatementAPIKey->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isOwner);
			preparedStatementAPIKey->setInt(queryParameterIndex++, isDefault);
			preparedStatementAPIKey->setString(queryParameterIndex++, permissions);
			char        strExpirationDate [64];
			{
				chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now()
					+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

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
				+ ", permissions: " + permissions
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
}

vector<tuple<int64_t, string, string>> MMSEngineDBFacade::deleteWorkspace(
		int64_t userKey,
		int64_t workspaceKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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
                "select isOwner, permissions "
				"from MMS_APIKey where workspaceKey = ? and userKey = ?";
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
				string permissions = resultSet->getString("permissions");
				Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);

				admin = JSONUtils::asBool(permissionsRoot, "admin", false);
                isOwner = resultSet->getInt("isOwner") == 1 ? "true" : "false";
            }
        }

		if (!isOwner && !admin)
		{
			string errorMessage = __FILEREF__ + "The user requesting the deletion does not have the ownership rights and the delete cannot be done"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2023-03-01: è necessario eliminare tutti gli user aventi solamente questo workspace
		//	calcoliamo questi user
		vector<tuple<int64_t, string, string>> usersToBeRemoved;
        {
            lastSQLCommand = 
				"select u.userKey, u.name, u.eMailAddress from MMS_APIKey ak, MMS_User u "
				"where ak.userKey = u.userKey and ak.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                int64_t userKey = resultSet->getInt64("userKey");
                string name = resultSet->getString("name");
                string eMailAddress = resultSet->getString("eMailAddress");

				lastSQLCommand = 
					"select count(*) from MMS_APIKey where userKey = ?";
				shared_ptr<sql::PreparedStatement> countPreparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				countPreparedStatement->setInt64(queryParameterIndex++, userKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> countResultSet (countPreparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", userKey: " + to_string(userKey)
					+ ", countResultSet->rowsCount: " + to_string(countResultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (countResultSet->next())
				{
					if (countResultSet->getInt64(1) == 1)
					{
						// it means the user has ONLY the workspace that will be removed
						// So the user will be removed too

						usersToBeRemoved.push_back(make_tuple(userKey, name, eMailAddress));
					}
				}
            }
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
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        // the users do not have any other workspace will be removed
		if (usersToBeRemoved.size() > 0)
        {
			string sUsers;
			for(tuple<int64_t, string, string> userDetails: usersToBeRemoved)
			{
				int64_t userKey;

				tie(userKey, ignore, ignore) = userDetails;

				if (sUsers == "")
					sUsers = to_string(userKey);
				else
					sUsers += (", " + to_string(userKey));
			}

			// in all the tables depending from User we have 'on delete cascade'
			// So all should be removed automatically from DB
			lastSQLCommand = 
				"delete from MMS_User where userKey in (" + sUsers + ")";
			shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
          
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", rowsUpdated: " + to_string(rowsUpdated)
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
        connectionPool->unborrow(conn);
		conn = nullptr;

		return usersToBeRemoved;
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    
    // return workspaceKeyUserKeyAndConfirmationCode;
}

tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
	MMSEngineDBFacade::checkAPIKey (string apiKey, bool fromMaster)
{
    shared_ptr<Workspace> workspace;
    int64_t         userKey;
    Json::Value		permissionsRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t         workspaceKey;
        
        {
            lastSQLCommand = 
                "select userKey, workspaceKey, permissions from MMS_APIKey "
				"where apiKey = ? and expirationDate >= NOW()";
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
                string permissions = resultSet->getString("permissions");
				permissionsRoot = JSONUtils::toJson(-1, -1, permissions);
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
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

    return make_tuple(userKey, workspace,
		JSONUtils::asBool(permissionsRoot, "admin", false),
		JSONUtils::asBool(permissionsRoot, "createRemoveWorkspace", false),
		JSONUtils::asBool(permissionsRoot, "ingestWorkflow", false),
		JSONUtils::asBool(permissionsRoot, "createProfiles", false),
		JSONUtils::asBool(permissionsRoot, "deliveryAuthorization", false),
		JSONUtils::asBool(permissionsRoot, "shareWorkspace", false),
		JSONUtils::asBool(permissionsRoot, "editMedia", false),
		JSONUtils::asBool(permissionsRoot, "editConfiguration", false),
		JSONUtils::asBool(permissionsRoot, "killEncoding", false),
		JSONUtils::asBool(permissionsRoot, "cancelIngestionJob", false),
		JSONUtils::asBool(permissionsRoot, "editEncodersPool", false),
		JSONUtils::asBool(permissionsRoot, "applicationRecorder", false)
    );
}

Json::Value MMSEngineDBFacade::login (
        string eMailAddress, string password)
{
    Json::Value     loginDetailsRoot;
    string          lastSQLCommand;

	// 2023-02-22: in questo metodo viene:
	//	1. controllato l'utente
	//	2. aggiornato il campo lastSuccessfulLogin
	// Poichè si cerca di far funzionare il piu possibile anche in caso di failure del master,
	// abbiamo separato le due attività, solo l'update viene fatta con connessione al master e,
	// se quest'ultima fallisce, comunque non viene bloccato il login
	int64_t userKey = -1;
	{
		shared_ptr<MySQLConnection> conn = nullptr;

		shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

		try
		{
			conn = connectionPool->borrow();	
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
					userKey = resultSet->getInt64("userKey");

					string field = "userKey";
					loginDetailsRoot[field] = userKey;

					field = "name";
					loginDetailsRoot[field] = static_cast<string>(resultSet->getString("name"));

					field = "email";
					loginDetailsRoot[field] = eMailAddress;

					field = "country";
					loginDetailsRoot[field] = static_cast<string>(resultSet->getString("country"));

					field = "creationDate";
					loginDetailsRoot[field] = static_cast<string>(resultSet->getString("creationDate"));

					field = "expirationDate";
					loginDetailsRoot[field] = static_cast<string>(resultSet->getString("expirationDate"));

					/*
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
					*/
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
			connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
				conn = nullptr;
			}

			throw e;
		}
	}

	{
		shared_ptr<MySQLConnection> conn = nullptr;

		shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

		try
		{
			conn = connectionPool->borrow();
			_logger->debug(__FILEREF__ + "DB connection borrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);

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

			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
				conn = nullptr;
			}

			// throw se;
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
				connectionPool->unborrow(conn);
				conn = nullptr;
			}

			// throw e;
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
				connectionPool->unborrow(conn);
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
				connectionPool->unborrow(conn);
				conn = nullptr;
			}

			// throw e;
		}
	}
    
    return loginDetailsRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceList (
    int64_t userKey, bool admin)
{
	Json::Value workspaceListRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		string field;
		{
			Json::Value requestParametersRoot;

			field = "userKey";
			requestParametersRoot[field] = userKey;

			/*
			field = "start";
			requestParametersRoot[field] = start;

			field = "rows";
			requestParametersRoot[field] = rows;
			*/

			field = "requestParameters";
			workspaceListRoot[field] = requestParametersRoot;
		}

		Json::Value responseRoot;
		{
			if (admin)
				lastSQLCommand =
					"select count(*) from MMS_Workspace w, MMS_APIKey a "
					"where w.workspaceKey = a.workspaceKey "
					"and a.userKey = ?";
			else
				lastSQLCommand =
					"select count(*) from MMS_Workspace w, MMS_APIKey a "
					"where w.workspaceKey = a.workspaceKey "
					"and a.userKey = ? "
					"and w.isEnabled = 1 and NOW() < a.expirationDate";
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
			if (admin)
				lastSQLCommand = 
					"select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
					"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
					"DATE_FORMAT(convert_tz(a.expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
					"a.permissions, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and userKey = ?";
			else
				lastSQLCommand = 
					"select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
					"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
					"DATE_FORMAT(convert_tz(a.expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
					"a.permissions, "
                    "DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and userKey = ? "
					"and w.isEnabled = 1 and NOW() < a.expirationDate";
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
			// bool encoders = true;
            while (resultSet->next())
            {
                Json::Value workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, resultSet, userAPIKeyInfo);	//, encoders);

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
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    
	return workspaceListRoot;
}

Json::Value MMSEngineDBFacade::getLoginWorkspace(int64_t userKey, bool fromMaster)
{
	Json::Value loginWorkspaceRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			// if admin returns all the workspaces of the user (even the one not enabled)
			// if NOT admin returns only the one having isEnabled = 1
			lastSQLCommand = 
				"select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, "
				"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
				"w.languageCode, "
				"a.apiKey, a.isOwner, a.isDefault, "
				"DATE_FORMAT(convert_tz(a.expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
				"a.permissions, "
				"DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
				"from MMS_APIKey a, MMS_Workspace w "
				"where a.workspaceKey = w.workspaceKey "
				"and a.userKey = ? and a.isDefault = 1 "
				"and NOW() < a.expirationDate "
				"and (JSON_EXTRACT(a.permissions, '$.admin') = true or w.isEnabled = 1) "
				"limit 1";
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
			// bool encoders = true;
            if (resultSet->next())
            {
                loginWorkspaceRoot = getWorkspaceDetailsRoot (
					conn, resultSet, userAPIKeyInfo);	//, encoders);
            }
			else
			{
				lastSQLCommand = 
					"select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, w.languageCode, "
					"a.apiKey, a.isOwner, a.isDefault, "
					"DATE_FORMAT(convert_tz(a.expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
					"a.permissions, "
					"DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
					"from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and a.userKey = ? "
					"and NOW() < a.expirationDate "
					"and (JSON_EXTRACT(a.permissions, '$.admin') = true or w.isEnabled = 1) "
					"limit 1";
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
				// bool encoders = true;
				if (resultSet->next())
				{
					loginWorkspaceRoot = getWorkspaceDetailsRoot (
						conn, resultSet, userAPIKeyInfo);	//, encoders);
				}
				else
				{
					string errorMessage = __FILEREF__ + "No workspace found"
						+ ", userKey: " + to_string(userKey)
						// + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					// no exception, just return an empty loginWorkspaceRoot
					// throw runtime_error(errorMessage);
				}
			}
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    
	return loginWorkspaceRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceDetailsRoot (
	shared_ptr<MySQLConnection> conn,
	shared_ptr<sql::ResultSet> resultSet,
	bool userAPIKeyInfo
	// bool encoders
	)
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

			field = "expirationDate";
			userAPIKeyRoot[field] = static_cast<string>(resultSet->getString("expirationDate"));

			string permissions = resultSet->getString("permissions");
			Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);

			field = "admin";
			bool admin = JSONUtils::asBool(permissionsRoot, "admin", false);
			userAPIKeyRoot[field] = admin;

			field = "createRemoveWorkspace";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "createRemoveWorkspace", false);

			field = "ingestWorkflow";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "ingestWorkflow", false);

			field = "createProfiles";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "createProfiles", false);

			field = "deliveryAuthorization";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "deliveryAuthorization", false);

			field = "shareWorkspace";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "shareWorkspace", false);

			field = "editMedia";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editMedia", false);
                
			field = "editConfiguration";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editConfiguration", false);

			field = "killEncoding";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "killEncoding", false);

			field = "cancelIngestionJob";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "cancelIngestionJob", false);

			field = "editEncodersPool";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editEncodersPool", false);

			field = "applicationRecorder";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "applicationRecorder", false);

			field = "userAPIKey";
			workspaceDetailRoot[field] = userAPIKeyRoot;

			if (admin)
			{
				{
					string lastSQLCommand =
						"select u.userKey as workspaceOwnerUserKey, u.name as workspaceOwnerUserName "
						"from MMS_APIKey ak, MMS_User u "
						"where ak.userKey = u.userKey "
						"and ak.workspaceKey = ? and ak.isOwner = b'1'";
					shared_ptr<sql::PreparedStatement> preparedStatementOwnerWorkspace (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementOwnerWorkspace->setInt64(queryParameterIndex++, workspaceKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSetOwnerWorkspace (
						preparedStatementOwnerWorkspace->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", resultSetOwnerWorkspace->rowsCount: " + to_string(resultSetOwnerWorkspace->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if(resultSetOwnerWorkspace->next())
					{
						field = "workspaceOwnerUserKey";
						workspaceDetailRoot[field] =
							resultSetOwnerWorkspace->getInt64("workspaceOwnerUserKey");

						field = "workspaceOwnerUserName";
						workspaceDetailRoot[field] = static_cast<string>(
							resultSetOwnerWorkspace->getString("workspaceOwnerUserName"));
					}
				}
			}
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
	bool enabledChanged, bool newEnabled,
	bool nameChanged, string newName,
	bool maxEncodingPriorityChanged, string newMaxEncodingPriority,
	bool encodingPeriodChanged, string newEncodingPeriod,
	bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber,
	bool maxStorageInMBChanged, int64_t newMaxStorageInMB,
	bool languageCodeChanged, string newLanguageCode,
	bool expirationDateChanged, string newExpirationDate,
	bool newCreateRemoveWorkspace,
	bool newIngestWorkflow,
	bool newCreateProfiles,
	bool newDeliveryAuthorization,
	bool newShareWorkspace,
	bool newEditMedia,
	bool newEditConfiguration,
	bool newKillEncoding,
	bool newCancelIngestionJob,
	bool newEditEncodersPool,
	bool newApplicationRecorder)
{
    Json::Value		workspaceDetailRoot;
    string			lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // check if ADMIN flag is already present
        bool admin = false;
		bool isOwner = false;
        {
            lastSQLCommand = 
                "select isOwner, permissions from MMS_APIKey "
				"where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                string permissions = resultSet->getString("permissions");
				Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);
                
                admin = JSONUtils::asBool(permissionsRoot, "admin", false);
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

		if (admin)
		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (enabledChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("isEnabled = ?");
				oneParameterPresent = true;
			}

			if (maxEncodingPriorityChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxEncodingPriority = ?");
				oneParameterPresent = true;
			}

			if (encodingPeriodChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("encodingPeriod = ?");
				oneParameterPresent = true;
			}

			if (maxIngestionsNumberChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxIngestionsNumber = ?");
				oneParameterPresent = true;
			}

			if (maxStorageInMBChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxStorageInMB = ?");
				oneParameterPresent = true;
			}

			if (oneParameterPresent)
			{
				lastSQLCommand = 
					string("update MMS_Workspace ") + setSQL + " "
					"where workspaceKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				if (enabledChanged)
					preparedStatement->setInt(queryParameterIndex++, newEnabled);
				if (maxEncodingPriorityChanged)
					preparedStatement->setString(queryParameterIndex++,
						newMaxEncodingPriority);
				if (encodingPeriodChanged)
					preparedStatement->setString(queryParameterIndex++, newEncodingPeriod);
				if (maxIngestionsNumberChanged)
					preparedStatement->setInt64(queryParameterIndex++,
						newMaxIngestionsNumber);
				if (maxStorageInMBChanged)
					preparedStatement->setInt64(queryParameterIndex++, newMaxStorageInMB);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", newEnabled: " + to_string(newEnabled)
					+ ", newMaxEncodingPriority: " + newMaxEncodingPriority
					+ ", newEncodingPeriod: " + newEncodingPeriod
					+ ", newMaxIngestionsNumber: " + to_string(newMaxIngestionsNumber)
					+ ", newMaxStorageInMB: " + to_string(newMaxStorageInMB)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @"
						+ to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newEnabled: " + to_string(newEnabled)
                        + ", newMaxEncodingPriority: " + newMaxEncodingPriority
                        + ", newEncodingPeriod: " + newEncodingPeriod
                        + ", newMaxIngestionsNumber: " + to_string(newMaxIngestionsNumber)
                        + ", newMaxStorageInMB: " + to_string(newMaxStorageInMB)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}

			if (expirationDateChanged)
			{
				// 2023-02-13: nel caso in cui un admin vuole cambiare la data di scadenza di un workspace,
				//		questo cambiamento deve avvenire per tutte le chiavi presenti
				lastSQLCommand =
					"update MMS_APIKey "
					"set expirationDate = convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) "
					"where workspaceKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, newExpirationDate);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", newExpirationDate: " + newExpirationDate
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (rowsUpdated == 0)
				{
					string errorMessage = __FILEREF__ + "no update was done"
						+ ", newExpirationDate: " + newExpirationDate
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}
        }

        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (nameChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = ?");
				oneParameterPresent = true;
			}

			if (languageCodeChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("languageCode = ?");
				oneParameterPresent = true;
			}

			if (oneParameterPresent)
			{
				lastSQLCommand = 
					string("update MMS_Workspace ") + setSQL + " "
					"where workspaceKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
					+ ", elapsed (secs): @"
						+ to_string(chrono::duration_cast<chrono::seconds>(
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
        }

        {
			Json::Value permissionsRoot;
			permissionsRoot["admin"] = admin;
			permissionsRoot["createRemoveWorkspace"] = newCreateRemoveWorkspace;
			permissionsRoot["ingestWorkflow"] = newIngestWorkflow;
			permissionsRoot["createProfiles"] = newCreateProfiles;
			permissionsRoot["deliveryAuthorization"] = newDeliveryAuthorization;
			permissionsRoot["shareWorkspace"] = newShareWorkspace;
			permissionsRoot["editMedia"] = newEditMedia;
			permissionsRoot["editConfiguration"] = newEditConfiguration;
			permissionsRoot["killEncoding"] = newKillEncoding;
			permissionsRoot["cancelIngestionJob"] = newCancelIngestionJob;
			permissionsRoot["editEncodersPool"] = newEditEncodersPool;
			permissionsRoot["applicationRecorder"] = newApplicationRecorder;

			string permissions = JSONUtils::toString(permissionsRoot);

			lastSQLCommand =
				"update MMS_APIKey set permissions = ? "
				"where workspaceKey = ? and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, permissions);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", permissions: " + permissions
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
                        + ", permissions: " + permissions
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }

        {
			lastSQLCommand = 
				"select w.workspaceKey, w.isEnabled, w.name, w.maxEncodingPriority, "
				"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
				"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
				"DATE_FORMAT(convert_tz(a.expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
				"a.permissions, "
				"DATE_FORMAT(convert_tz(w.creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
				"from MMS_APIKey a, MMS_Workspace w "
				"where a.workspaceKey = w.workspaceKey "
				"and a.workspaceKey = ? "
				"and userKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
			bool userAPIKeyInfo = true;
            if (resultSet->next())
            {
				workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, resultSet, userAPIKeyInfo);
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		workspaceUsage = getWorkspaceUsage(conn, workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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
                connectionPool->unborrow(conn);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    
    pair<string, string> emailAddressAndName = make_pair(emailAddress, name);
            
    return emailAddressAndName;
}

pair<int64_t, string> MMSEngineDBFacade::getUserDetailsByEmail (string email)
{
	int64_t			userKey;
	string			name;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select userKey, name from MMS_User where eMailAddress = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, email);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", emailserKey: " + email
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("userKey");
                name = resultSet->getString("name");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User is not present"
                    + ", email: " + email
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

	pair<int64_t, string> userDetails = make_pair(userKey, name);

    return userDetails;
}

Json::Value MMSEngineDBFacade::updateUser (
		bool admin,
		bool ldapEnabled,
        int64_t userKey,
        bool nameChanged, string name,
        bool emailChanged, string email, 
        bool countryChanged, string country,
        bool expirationDateChanged, string expirationDate,
		bool passwordChanged, string newPassword, string oldPassword)
{
    Json::Value     loginDetailsRoot;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			int rowsUpdated;

			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (passwordChanged)
			{
				string savedPassword;
				{
					lastSQLCommand = 
						"select password from MMS_User where userKey = ?";
					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setInt64(queryParameterIndex++, userKey);

					chrono::system_clock::time_point startSql =
						chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSet (
						preparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", userKey: " + to_string(userKey)
						+ ", resultSet->rowsCount: "
							+ to_string(resultSet->rowsCount())
						+ ", elapsed (secs): @" + to_string(
							chrono::duration_cast<chrono::seconds>(
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

				if (savedPassword != oldPassword
						|| newPassword == "")
				{
					string errorMessage = __FILEREF__
						+ "old password is wrong or newPassword is not valid"
						+ ", userKey: " + to_string(userKey)
					;
					_logger->warn(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("password = ?");
				oneParameterPresent = true;
			}

			if (nameChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = ?");
				oneParameterPresent = true;
			}

			if (emailChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("eMailAddress = ?");
				oneParameterPresent = true;
			}

			if (countryChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = ?");
				oneParameterPresent = true;
			}
			if (admin && expirationDateChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("expirationDate = ?");
				oneParameterPresent = true;
			}

			lastSQLCommand = 
				string("update MMS_User ") + setSQL + " "
				"where userKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (passwordChanged)
				preparedStatement->setString(queryParameterIndex++, newPassword);
			if (nameChanged)
				preparedStatement->setString(queryParameterIndex++, name);
			if (emailChanged)
				preparedStatement->setString(queryParameterIndex++, email);
			if (countryChanged)
				preparedStatement->setString(queryParameterIndex++, country);
			if (admin && expirationDateChanged)
				preparedStatement->setString(queryParameterIndex++, expirationDate);
			preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newPassword: " + newPassword
				+ ", name: " + name
				+ ", email: " + email
				+ ", country: " + country
				+ ", expirationDate: " + expirationDate
				+ ", userKey: " + to_string(userKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                "DATE_FORMAT(convert_tz(expirationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as expirationDate, "
				"userKey, name, eMailAddress, country "
                "from MMS_User where userKey = ?";
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
				+ ", elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                string field = "creationDate";
                loginDetailsRoot[field] = static_cast<string>(
					resultSet->getString("creationDate"));

                field = "expirationDate";
                loginDetailsRoot[field] = static_cast<string>(
					resultSet->getString("expirationDate"));

				field = "userKey";
				loginDetailsRoot[field] = resultSet->getInt64("userKey");

				field = "name";
				loginDetailsRoot[field] = static_cast<string>(
					resultSet->getString("name"));

				field = "email";
				loginDetailsRoot[field] = static_cast<string>(
					resultSet->getString("eMailAddress"));

				field = "country";
				loginDetailsRoot[field] = static_cast<string>(
					resultSet->getString("country"));
                
				field = "ldapEnabled";
				loginDetailsRoot[field] = ldapEnabled;
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

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    
    return loginDetailsRoot;
}

string MMSEngineDBFacade::createResetPasswordToken(
	int64_t userKey
)
{
	string		resetPasswordToken;
	string		lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
		resetPasswordToken = to_string(e());

        {
			lastSQLCommand = 
				"insert into MMS_ResetPasswordToken (token, userKey, creationDate) "
				"values (?, ?, NOW())";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, resetPasswordToken);
			preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", resetPasswordToken: " + resetPasswordToken
				+ ", userKey: " + to_string(userKey)
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

    return resetPasswordToken;
}

pair<string, string> MMSEngineDBFacade::resetPassword(
	string resetPasswordToken,
	string newPassword
)
{
	string		name;
	string		email;
	string		lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int resetPasswordRetentionInHours = 24;

		int userKey;
        {
            lastSQLCommand = 
				"select u.name, u.eMailAddress, u.userKey "
				"from MMS_ResetPasswordToken rp, MMS_User u "
				"where rp.userKey = u.userKey and rp.token = ? "
				"and DATE_ADD(rp.creationDate, INTERVAL ? HOUR) >= NOW()";
			;
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, resetPasswordToken);
            preparedStatement->setInt(queryParameterIndex++, resetPasswordRetentionInHours);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", resetPasswordToken: " + resetPasswordToken
				+ ", resetPasswordRetentionInHours: " + to_string(resetPasswordRetentionInHours)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                name = resultSet->getString("name");
                email = resultSet->getString("eMailAddress");
                userKey = resultSet->getInt64("userKey");
            }
            else
            {
                string errorMessage = __FILEREF__
					+ "reset password token is not present or is expired"
                    + ", resetPasswordToken: " + resetPasswordToken
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
			lastSQLCommand = 
				"update MMS_User set password = ? where userKey = ?"
				;
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, newPassword);
			preparedStatement->setInt64(queryParameterIndex++, userKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newPassword: " + newPassword
				+ ", userKey: " + to_string(userKey)
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			/*
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done"
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
			*/
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }

	pair<string, string> details = make_pair(name, email);

    return details;
}

int64_t MMSEngineDBFacade::saveLoginStatistics(
	int userKey, string ip,
	string continent, string continentCode, string country, string countryCode,
	string region, string city, string org, string isp, int timezoneGMTOffset
)
{
	int64_t loginStatisticsKey;
    string  lastSQLCommand;

	shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
		conn = connectionPool->borrow();
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

		{
			lastSQLCommand = 
				"insert into MMS_LoginStatistics (userKey, ip, continent, continentCode, "
				"country, countryCode, region, city, org, isp, timezoneGMTOffset, successfulLogin) values ("
				                             "?,       ?,  ?,         ?, "
			    "?,       ?,           ?,      ?,    ?,   ?,   ?,                 NOW())";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, userKey);
			if (ip == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, ip);
			if (continent == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, continent);
			if (continentCode == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, continentCode);
			if (country == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, country);
			if (countryCode == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, countryCode);
			if (region == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, region);
			if (city == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, city);
			if (org == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, org);
			if (isp == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, isp);
			if (timezoneGMTOffset == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, timezoneGMTOffset);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", ip: " + ip
				+ ", continent: " + continent
				+ ", continentCode: " + continentCode
				+ ", country: " + country
				+ ", countryCode: " + countryCode
				+ ", region: " + region
				+ ", city: " + city
				+ ", org: " + org
				+ ", isp: " + isp
				+ ", timezoneGMTOffset: " + to_string(timezoneGMTOffset)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			loginStatisticsKey = getLastInsertId(conn);
		}

		_logger->debug(__FILEREF__ + "DB connection unborrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);
		connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
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
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

        throw e;
    }

	return loginStatisticsKey;
}

