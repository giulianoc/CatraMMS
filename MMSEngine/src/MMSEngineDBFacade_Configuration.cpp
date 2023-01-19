
#include "JSONUtils.h"
#include "FFMpeg.h"
#include "MMSEngineDBFacade.h"

Json::Value MMSEngineDBFacade::addYouTubeConf(
	int64_t workspaceKey,
	string label,
	string tokenType,
	string refreshToken,
	string accessToken
	)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t confKey;
		{
			lastSQLCommand = 
				"insert into MMS_Conf_YouTube(workspaceKey, label, tokenType, "
				"refreshToken, accessToken) values ("
				"?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, tokenType);
            preparedStatement->setString(queryParameterIndex++, refreshToken);
            preparedStatement->setString(queryParameterIndex++, accessToken);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", tokenType: " + tokenType
				+ ", refreshToken: " + refreshToken
				+ ", accessToken: " + accessToken
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
        }

		Json::Value youTubeConfRoot;
		{
			string field = "confKey";
			youTubeConfRoot[field] = confKey;

			field = "label";
			youTubeConfRoot[field] = label;

			field = "tokenType";
			youTubeConfRoot[field] = tokenType;

			field = "refreshToken";
			if (tokenType == "RefreshToken")
			{
				youTubeConfRoot[field] = refreshToken;

				field = "accessToken";
				youTubeConfRoot[field] = Json::nullValue;
			}
			else
			{
				youTubeConfRoot[field] = Json::nullValue;

				field = "accessToken";
				youTubeConfRoot[field] = accessToken;
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return youTubeConfRoot;
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

Json::Value MMSEngineDBFacade::modifyYouTubeConf(
    int64_t confKey, int64_t workspaceKey,
    string label, bool labelModified,
    string tokenType, bool tokenTypeModified,
    string refreshToken, bool refreshTokenModified,
    string accessToken, bool accessTokenModified
	)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = ?");
				oneParameterPresent = true;
			}

			if (tokenTypeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tokenType = ?");
				oneParameterPresent = true;
			}

			if (refreshTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("refreshToken = ?");
				oneParameterPresent = true;
			}

			if (accessTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("accessToken = ?");
				oneParameterPresent = true;
			}

            lastSQLCommand = 
                string("update MMS_Conf_YouTube ") + setSQL + " "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (labelModified)
				preparedStatement->setString(queryParameterIndex++, label);
			if (tokenTypeModified)
				preparedStatement->setString(queryParameterIndex++, tokenType);
			if (refreshTokenModified)
			{
				if (refreshToken == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, refreshToken);
			}
			if (accessTokenModified)
			{
				if (accessToken == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, accessToken);
			}
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label (" + to_string(labelModified) + "): " + label
				+ ", tokenType (" + to_string(tokenTypeModified) + "): " + tokenType
				+ ", refreshToken (" + to_string(refreshTokenModified) + "): " + refreshToken
				+ ", accessToken (" + to_string(accessTokenModified) + "): " + accessToken
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

		Json::Value youTubeConfRoot;
        {
            lastSQLCommand = 
                "select confKey, label, tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where confKey = ? and workspaceKey = ?";
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage = __FILEREF__ + "No YouTube conf found"
					+ ", confKey: " + to_string(confKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
            }

            {
                string field = "confKey";
                youTubeConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                youTubeConfRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "tokenType";
				youTubeConfRoot[field] = static_cast<string>(
					resultSet->getString("tokenType"));

				field = "refreshToken";
				if (resultSet->isNull("refreshToken"))
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = static_cast<string>(
						resultSet->getString("refreshToken"));

				field = "accessToken";
				if (resultSet->isNull("accessToken"))
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = static_cast<string>(
						resultSet->getString("accessToken"));
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return youTubeConfRoot;
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

void MMSEngineDBFacade::removeYouTubeConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_YouTube where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getYouTubeConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value youTubeConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getYouTubeConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = connectionPool->borrow();	
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
                string ("select confKey, label, tokenType, refreshToken, accessToken ")
				+ "from MMS_Conf_YouTube "
                + sqlWhere;

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
            while (resultSet->next())
            {
                Json::Value youTubeConfRoot;

                field = "confKey";
                youTubeConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                youTubeConfRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "tokenType";
				youTubeConfRoot[field] = static_cast<string>(
					resultSet->getString("tokenType"));

				field = "refreshToken";
				if (resultSet->isNull("refreshToken"))
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = static_cast<string>(
						resultSet->getString("refreshToken"));

				field = "accessToken";
				if (resultSet->isNull("accessToken"))
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = static_cast<string>(
						resultSet->getString("accessToken"));

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
    
    return youTubeConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getYouTubeDetailsByConfigurationLabel(
	int64_t workspaceKey, string youTubeConfigurationLabel
)
{
    string		lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
		string		youTubeTokenType;
		string		youTubeRefreshToken;
		string		youTubeAccessToken;

        _logger->info(__FILEREF__ + "getYouTubeDetailsByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "select tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where workspaceKey = ? and label = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, youTubeConfigurationLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_YouTube failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            youTubeTokenType = resultSet->getString("tokenType");
			if (!resultSet->isNull("refreshToken"))
				youTubeRefreshToken = resultSet->getString("refreshToken");
			if (!resultSet->isNull("accessToken"))
				youTubeAccessToken = resultSet->getString("accessToken");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(youTubeTokenType, youTubeRefreshToken, youTubeAccessToken);
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

int64_t MMSEngineDBFacade::addFacebookConf(
    int64_t workspaceKey,
    string label,
    string userAccessToken)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
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
                "insert into MMS_Conf_Facebook(workspaceKey, label, modificationDate, userAccessToken) "
				"values (?, ?, NOW(), ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, userAccessToken);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", userAccessToken: " + userAccessToken
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyFacebookConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string userAccessToken)
{
    string      lastSQLCommand;
    
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
                "update MMS_Conf_Facebook set label = ?, userAccessToken = ?, modificationDate = NOW() "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, userAccessToken);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", userAccessToken: " + userAccessToken
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeFacebookConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_Facebook where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getFacebookConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	string      lastSQLCommand;
	Json::Value facebookConfListRoot;

	shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getFacebookConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            facebookConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (confKey != -1)
			sqlWhere += "and confKey = ? ";
		else if (label != "")
			sqlWhere += "and label = ? ";
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_Facebook ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                string ("select confKey, label, userAccessToken, ")
				+ "DATE_FORMAT(convert_tz(modificationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as modificationDate "
                + "from MMS_Conf_Facebook "
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value facebookConfRoot;

                field = "confKey";
                facebookConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                facebookConfRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "modificationDate";
				facebookConfRoot[field] = static_cast<string>(resultSet->getString("modificationDate"));

                field = "userAccessToken";
                facebookConfRoot[field] = static_cast<string>(resultSet->getString("userAccessToken"));

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
    
    return facebookConfListRoot;
}

string MMSEngineDBFacade::getFacebookUserAccessTokenByConfigurationLabel(
    int64_t workspaceKey, string facebookConfigurationLabel
)
{
    string      lastSQLCommand;
    string      facebookUserAccessToken;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getFacebookUserAccessTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", facebookConfigurationLabel: " + facebookConfigurationLabel
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select userAccessToken from MMS_Conf_Facebook where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, facebookConfigurationLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", facebookConfigurationLabel: " + facebookConfigurationLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Facebook failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", facebookConfigurationLabel: " + facebookConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            facebookUserAccessToken = resultSet->getString("userAccessToken");
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
    
    return facebookUserAccessToken;
}

int64_t MMSEngineDBFacade::addTwitchConf(
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
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
                "insert into MMS_Conf_Twitch(workspaceKey, label, modificationDate, refreshToken) "
				"values (?, ?, NOW(), ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, refreshToken);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", refreshToken: " + refreshToken
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyTwitchConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
    string      lastSQLCommand;
    
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
                "update MMS_Conf_Twitch set label = ?, refreshToken = ?, modificationDate = NOW() "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, refreshToken);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", refreshToken: " + refreshToken
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeTwitchConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_Twitch where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getTwitchConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	string      lastSQLCommand;
	Json::Value twitchConfListRoot;

	shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getTwitchConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            twitchConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (confKey != -1)
			sqlWhere += "and confKey = ? ";
		else if (label != "")
			sqlWhere += "and label = ? ";
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_Twitch ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value twitchRoot(Json::arrayValue);
        {                    
            lastSQLCommand =
                string ("select confKey, label, refreshToken, ")
				+ "DATE_FORMAT(convert_tz(modificationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as modificationDate "
                + "from MMS_Conf_Twitch "
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value twitchConfRoot;

                field = "confKey";
                twitchConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                twitchConfRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "modificationDate";
				twitchConfRoot[field] = static_cast<string>(resultSet->getString("modificationDate"));

                field = "refreshToken";
                twitchConfRoot[field] = static_cast<string>(resultSet->getString("refreshToken"));

                twitchRoot.append(twitchConfRoot);
            }
        }

        field = "twitchConf";
        responseRoot[field] = twitchRoot;

        field = "response";
        twitchConfListRoot[field] = responseRoot;

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
    
    return twitchConfListRoot;
}

string MMSEngineDBFacade::getTwitchUserAccessTokenByConfigurationLabel(
    int64_t workspaceKey, string twitchConfigurationLabel
)
{
    string      lastSQLCommand;
    string      twitchRefreshToken;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getTwitchUserAccessTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", twitchConfigurationLabel: " + twitchConfigurationLabel
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select refreshToken from MMS_Conf_Twitch where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, twitchConfigurationLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", twitchConfigurationLabel: " + twitchConfigurationLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Twitch failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", twitchConfigurationLabel: " + twitchConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            twitchRefreshToken = resultSet->getString("refreshToken");
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
    
    return twitchRefreshToken;
}

int64_t MMSEngineDBFacade::addTiktokConf(
    int64_t workspaceKey,
    string label,
    string accessToken)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
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
                "insert into MMS_Conf_Tiktok(workspaceKey, label, modificationDate, accessToken) "
				"values (?, ?, NOW(), ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, accessToken);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", accessToken: " + accessToken
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyTiktokConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string accessToken)
{
    string      lastSQLCommand;
    
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
                "update MMS_Conf_Tiktok set label = ?, accessToken = ?, modificationDate = NOW() "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, accessToken);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", accessToken: " + accessToken
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeTiktokConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_Tiktok where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getTiktokConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	string      lastSQLCommand;
	Json::Value tiktokConfListRoot;

	shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getTiktokConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            tiktokConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (confKey != -1)
			sqlWhere += "and confKey = ? ";
		else if (label != "")
			sqlWhere += "and label = ? ";
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_Tiktok ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value tiktokRoot(Json::arrayValue);
        {                    
            lastSQLCommand =
                string ("select confKey, label, accessToken, ")
				+ "DATE_FORMAT(convert_tz(modificationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as modificationDate "
                + "from MMS_Conf_Tiktok "
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
			else if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value tiktokConfRoot;

                field = "confKey";
                tiktokConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                tiktokConfRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "modificationDate";
				tiktokConfRoot[field] = static_cast<string>(resultSet->getString("modificationDate"));

                field = "accessToken";
                tiktokConfRoot[field] = static_cast<string>(resultSet->getString("accessToken"));

                tiktokRoot.append(tiktokConfRoot);
            }
        }

        field = "tiktokConf";
        responseRoot[field] = tiktokRoot;

        field = "response";
        tiktokConfListRoot[field] = responseRoot;

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
    
    return tiktokConfListRoot;
}

string MMSEngineDBFacade::getTiktokAccessTokenByConfigurationLabel(
    int64_t workspaceKey, string tiktokConfigurationLabel
)
{
    string      lastSQLCommand;
    string      tiktokAccessToken;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getTiktokAccessTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", tiktokConfigurationLabel: " + tiktokConfigurationLabel
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                string("select accessToken from MMS_Conf_Tiktok where workspaceKey = ? and label = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, tiktokConfigurationLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", tiktokConfigurationLabel: " + tiktokConfigurationLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Tiktok failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", tiktokConfigurationLabel: " + tiktokConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            tiktokAccessToken = resultSet->getString("accessToken");
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
    
    return tiktokAccessToken;
}

Json::Value MMSEngineDBFacade::addStream(
    int64_t workspaceKey,
    string label,
	string sourceType,
	int64_t encodersPoolKey,
	string url,
	string pushProtocol,
	int64_t pushEncoderKey,
	string pushServerName,		// indica il nome del server (public or internal)
	int pushServerPort,
	string pushUri,
	int pushListenTimeout,
	int captureLiveVideoDeviceNumber,
	string captureLiveVideoInputFormat,
	int captureLiveFrameRate,
	int captureLiveWidth,
	int captureLiveHeight,
	int captureLiveAudioDeviceNumber,
	int captureLiveChannelsNumber,
	int64_t tvSourceTVConfKey,
	string type,
	string description,
	string name,
	string region,
	string country,
	int64_t imageMediaItemKey,
	string imageUniqueName,
	int position,
	Json::Value userData)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
			string sUserData;
			if (userData != Json::nullValue)
			{
				sUserData = JSONUtils::toString(userData);
			}

            lastSQLCommand = 
                "insert into MMS_Conf_Stream(workspaceKey, label, sourceType, "
				"encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"tvSourceTVConfKey, "
				"type, description, name, "
				"region, country, imageMediaItemKey, imageUniqueName, "
				"position, userData) values ("
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
				"?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, sourceType);
			if (encodersPoolKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			if (url == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, url);

			if (pushProtocol == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, pushProtocol);
			if (pushEncoderKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, pushEncoderKey);
			if (pushServerName == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, pushServerName);
			if (pushServerPort == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, pushServerPort);
			if (pushUri == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, pushUri);
			if (pushListenTimeout == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, pushListenTimeout);
			if (captureLiveVideoDeviceNumber == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveVideoDeviceNumber);
			if (captureLiveVideoInputFormat == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, captureLiveVideoInputFormat);
			if (captureLiveFrameRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveFrameRate);
			if (captureLiveWidth == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveWidth);
			if (captureLiveHeight == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveHeight);
			if (captureLiveAudioDeviceNumber == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveAudioDeviceNumber);
			if (captureLiveChannelsNumber == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, captureLiveChannelsNumber);
			if (tvSourceTVConfKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, tvSourceTVConfKey);

			if (type == "")
				 preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, type);
			if (description == "")
				 preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, description);
			if (name == "")
				 preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, name);
			if (region == "")
				 preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, region);
			if (country == "")
				 preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, country);
			if (imageMediaItemKey == -1)
			{
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);

				if (imageUniqueName == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, imageUniqueName);
			}
			else
			{
				preparedStatement->setInt64(queryParameterIndex++, imageMediaItemKey);
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			}
			if (position == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, position);
			if (sUserData == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, sUserData);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", url: " + url
				+ ", type: " + type
				+ ", description: " + description
				+ ", name: " + name
				+ ", region: " + region
				+ ", country: " + country
				+ ", imageMediaItemKey: " + to_string(imageMediaItemKey)
				+ ", imageUniqueName: " + imageUniqueName
				+ ", position: " + to_string(position)
				+ ", sUserData: " + sUserData
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
        }

		Json::Value streamsRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			bool labelLike = true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value streamListRoot = getStreamList (
				conn, workspaceKey, confKey,
				start, rows, label, labelLike, url, sourceType, type, name,
				region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return streamsRoot[0];
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

Json::Value MMSEngineDBFacade::modifyStream(
    int64_t confKey,
    int64_t workspaceKey,
    bool labelToBeModified, string label,

	bool sourceTypeToBeModified, string sourceType,
	bool encodersPoolKeyToBeModified, int64_t encodersPoolKey,
	bool urlToBeModified, string url,
	bool pushProtocolToBeModified, string pushProtocol,
	bool pushEncoderKeyToBeModified, int64_t pushEncoderKey,
	bool pushServerNameToBeModified, string pushServerName,
	bool pushServerPortToBeModified, int pushServerPort,
	bool pushUriToBeModified, string pushUri,
	bool pushListenTimeoutToBeModified, int pushListenTimeout,
	bool captureLiveVideoDeviceNumberToBeModified, int captureLiveVideoDeviceNumber,
	bool captureLiveVideoInputFormatToBeModified, string captureLiveVideoInputFormat,
	bool captureLiveFrameRateToBeModified, int captureLiveFrameRate,
	bool captureLiveWidthToBeModified, int captureLiveWidth,
	bool captureLiveHeightToBeModified, int captureLiveHeight,
	bool captureLiveAudioDeviceNumberToBeModified, int captureLiveAudioDeviceNumber,
	bool captureLiveChannelsNumberToBeModified, int captureLiveChannelsNumber,
	bool tvSourceTVConfKeyToBeModified, int64_t tvSourceTVConfKey,

	bool typeToBeModified, string type,
	bool descriptionToBeModified, string description,
	bool nameToBeModified, string name,
	bool regionToBeModified, string region,
	bool countryToBeModified, string country,
	bool imageToBeModified, int64_t imageMediaItemKey, string imageUniqueName,
	bool positionToBeModified, int position,
	bool userDataToBeModified, Json::Value userData)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = ?");
				oneParameterPresent = true;
			}

			if (sourceTypeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("sourceType = ?");
				oneParameterPresent = true;
			}

			if (encodersPoolKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("encodersPoolKey = ?");
				oneParameterPresent = true;
			}

			if (urlToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("url = ?");
				oneParameterPresent = true;
			}

			if (pushProtocolToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushProtocol = ?");
				oneParameterPresent = true;
			}

			if (pushEncoderKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushEncoderKey = ?");
				oneParameterPresent = true;
			}

			if (pushServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushServerName = ?");
				oneParameterPresent = true;
			}

			if (pushServerPortToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushServerPort = ?");
				oneParameterPresent = true;
			}

			if (pushUriToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushUri = ?");
				oneParameterPresent = true;
			}

			if (pushListenTimeoutToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushListenTimeout = ?");
				oneParameterPresent = true;
			}

			if (captureLiveVideoDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveVideoDeviceNumber = ?");
				oneParameterPresent = true;
			}

			if (captureLiveVideoInputFormatToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveVideoInputFormat = ?");
				oneParameterPresent = true;
			}

			if (captureLiveFrameRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveFrameRate = ?");
				oneParameterPresent = true;
			}

			if (captureLiveWidthToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveWidth = ?");
				oneParameterPresent = true;
			}

			if (captureLiveHeightToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveHeight = ?");
				oneParameterPresent = true;
			}

			if (captureLiveAudioDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveAudioDeviceNumber = ?");
				oneParameterPresent = true;
			}

			if (captureLiveChannelsNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveChannelsNumber = ?");
				oneParameterPresent = true;
			}

			if (tvSourceTVConfKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tvSourceTVConfKey = ?");
				oneParameterPresent = true;
			}

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = ?");
				oneParameterPresent = true;
			}

			if (descriptionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("description = ?");
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = ?");
				oneParameterPresent = true;
			}

			if (regionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("region = ?");
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = ?");
				oneParameterPresent = true;
			}

			if (imageToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("imageMediaItemKey = ?, imageUniqueName = ?");
				oneParameterPresent = true;
			}

			if (positionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("position = ?");
				oneParameterPresent = true;
			}

			string sUserData;
			if (userDataToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("userData = ?");
				oneParameterPresent = true;

				if (userData != Json::nullValue)
				{
					sUserData = JSONUtils::toString(userData);
				}
			}

			if (!oneParameterPresent)
            {
                string errorMessage = __FILEREF__ + "Wrong input, no parameters to be updated"
                        + ", confKey: " + to_string(confKey)
                        + ", oneParameterPresent: " + to_string(oneParameterPresent)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            lastSQLCommand = 
                string("update MMS_Conf_Stream ") + setSQL + " "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (labelToBeModified)
				preparedStatement->setString(queryParameterIndex++, label);
			if (sourceTypeToBeModified)
				preparedStatement->setString(queryParameterIndex++, sourceType);
			if (encodersPoolKeyToBeModified)
			{
				if (encodersPoolKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			}
			if (urlToBeModified)
			{
				if (url == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, url);
			}
			if (pushProtocolToBeModified)
			{
				if (pushProtocol == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, pushProtocol);
			}
			if (pushEncoderKeyToBeModified)
			{
				if (pushEncoderKey == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, pushEncoderKey);
			}
			if (pushServerNameToBeModified)
			{
				if (pushServerName == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, pushServerName);
			}
			if (pushServerPortToBeModified)
			{
				if (pushServerPort == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++, pushServerPort);
			}
			if (pushUriToBeModified)
			{
				if (pushUri == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, pushUri);
			}
			if (pushListenTimeoutToBeModified)
			{
				if (pushListenTimeout == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++, pushListenTimeout);
			}
			if (captureLiveVideoDeviceNumberToBeModified)
			{
				if (captureLiveVideoDeviceNumber == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++,
						captureLiveVideoDeviceNumber);
			}
			if (captureLiveVideoInputFormatToBeModified)
			{
				if (captureLiveVideoInputFormat == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++,
						captureLiveVideoInputFormat);
			}
			if (captureLiveFrameRateToBeModified)
			{
				if (captureLiveFrameRate == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++,
						captureLiveFrameRate);
			}
			if (captureLiveWidthToBeModified)
			{
				if (captureLiveWidth == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++, captureLiveWidth);
			}
			if (captureLiveHeightToBeModified)
			{
				if (captureLiveHeight == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++, captureLiveHeight);
			}
			if (captureLiveAudioDeviceNumberToBeModified)
			{
				if (captureLiveAudioDeviceNumber == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++,
						captureLiveAudioDeviceNumber);
			}
			if (captureLiveChannelsNumberToBeModified)
			{
				if (captureLiveChannelsNumber == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++,
						captureLiveChannelsNumber);
			}
			if (tvSourceTVConfKeyToBeModified)
			{
				if (tvSourceTVConfKey == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::BIGINT);
				else
					preparedStatement->setInt(queryParameterIndex++,
						tvSourceTVConfKey);
			}
			if (typeToBeModified)
			{
				if (type == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, type);
			}
			if (descriptionToBeModified)
			{
				if (description == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, description);
			}
			if (nameToBeModified)
			{
				if (name == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, name);
			}
			if (regionToBeModified)
			{
				if (region == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, region);
			}
			if (countryToBeModified)
			{
				if (country == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, country);
			}
			if (imageToBeModified)
			{
				if (imageMediaItemKey == -1)
				{
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::BIGINT);

					if (imageUniqueName == "")
						preparedStatement->setNull(queryParameterIndex++,
							sql::DataType::VARCHAR);
					else
						preparedStatement->setString(queryParameterIndex++,
							imageUniqueName);
				}
				else
				{
					preparedStatement->setInt64(queryParameterIndex++,
						imageMediaItemKey);
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				}
			}
			if (positionToBeModified)
			{
				if (position == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::INTEGER);
				else
					preparedStatement->setInt(queryParameterIndex++, position);
			}
			if (userDataToBeModified)
			{
				if (sUserData == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, sUserData);
			}
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label (" + to_string(labelToBeModified) + "): " + label
				+ ", sourceType (" + to_string(sourceTypeToBeModified) + "): "
					+ sourceType
				+ ", encodersPoolKey (" + to_string(encodersPoolKeyToBeModified) + "): "
					+ to_string(encodersPoolKey)
				+ ", url (" + to_string(urlToBeModified) + "): " + url
				+ ", pushProtocol (" + to_string(pushProtocolToBeModified) + "): "
					+ pushProtocol
				+ ", pushEncoderKey (" + to_string(pushEncoderKeyToBeModified) + "): "
					+ to_string(pushEncoderKey)
				+ ", pushServerName (" + to_string(pushServerNameToBeModified) + "): "
					+ pushServerName
				+ ", pushServerPort (" + to_string(pushServerPortToBeModified) + "): "
					+ to_string(pushServerPort)
				+ ", pushUri (" + to_string(pushUriToBeModified) + "): " + pushUri
				+ ", pushListenTimeout (" + to_string(pushListenTimeoutToBeModified) + "): "
					+ to_string(pushListenTimeout)
				+ ", type (" + to_string(typeToBeModified) + "): " + type
				+ ", description (" + to_string(descriptionToBeModified) + "): "
					+ description
				+ ", name (" + to_string(nameToBeModified) + "): " + name
				+ ", region (" + to_string(regionToBeModified) + "): " + region
				+ ", country (" + to_string(countryToBeModified) + "): " + country
				+ ", imageMediaItemKey (" + to_string(imageToBeModified) + "): "
					+ to_string(imageMediaItemKey)
				+ ", imageUniqueName (" + to_string(imageToBeModified) + "): "
					+ imageUniqueName
				+ ", position (" + to_string(positionToBeModified) + "): "
					+ to_string(position)
				+ ", sUserData (" + to_string(userDataToBeModified) + "): "
					+ sUserData
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

		Json::Value streamsRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			bool labelLike = true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value streamListRoot = getStreamList (
				conn, workspaceKey, confKey,
				start, rows, label, labelLike, url, sourceType, type, name, region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return streamsRoot[0];
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

void MMSEngineDBFacade::removeStream(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_Stream where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getStreamList (
	int64_t workspaceKey, int64_t liveURLKey,
	int start, int rows,
	string label, bool labelLike, string url, string sourceType, string type,
	string name, string region, string country,
	string labelOrder	// "" or "asc" or "desc"
)
{
    Json::Value streamListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		streamListRoot = MMSEngineDBFacade::getStreamList (
			conn, workspaceKey, liveURLKey,
			start, rows,
			label, labelLike, url, sourceType, type,
			name, region, country,
			labelOrder);

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
    
    return streamListRoot;
}

Json::Value MMSEngineDBFacade::getStreamList (
	shared_ptr<MySQLConnection> conn,
	int64_t workspaceKey, int64_t liveURLKey,
	int start, int rows,
	string label, bool labelLike, string url, string sourceType, string type,
	string name, string region, string country,
	string labelOrder	// "" or "asc" or "desc"
)
{
    string      lastSQLCommand;
    Json::Value streamListRoot;
    
    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getStreamList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", liveURLKey: " + to_string(liveURLKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", label: " + label
            + ", labelLike: " + to_string(labelLike)
            + ", url: " + url
            + ", sourceType: " + sourceType
            + ", type: " + type
            + ", name: " + name
            + ", region: " + region
            + ", country: " + country
            + ", labelOrder: " + labelOrder
        );
        
        {
            Json::Value requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}
            
            if (liveURLKey != -1)
			{
				field = "liveURLKey";
				requestParametersRoot[field] = liveURLKey;
			}
            
			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}
            
            if (label != "")
			{
				field = "label";
				requestParametersRoot[field] = label;
			}
            
			{
				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}
            
            if (url != "")
			{
				field = "url";
				requestParametersRoot[field] = url;
			}

            if (sourceType != "")
			{
				field = "sourceType";
				requestParametersRoot[field] = sourceType;
			}

            if (type != "")
			{
				field = "type";
				requestParametersRoot[field] = type;
			}

            if (name != "")
			{
				field = "name";
				requestParametersRoot[field] = name;
			}

            if (region != "")
			{
				field = "region";
				requestParametersRoot[field] = region;
			}

            if (country != "")
			{
				field = "country";
				requestParametersRoot[field] = country;
			}

            if (labelOrder != "")
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

            field = "requestParameters";
            streamListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        if (liveURLKey != -1)
			sqlWhere += ("and confKey = ? ");
        if (label != "")
		{
			if (labelLike)
				sqlWhere += ("and LOWER(label) like LOWER(?) ");
			else
				sqlWhere += ("and LOWER(label) = LOWER(?) ");
		}
        if (url != "")
            sqlWhere += ("and url like ? ");
        if (sourceType != "")
            sqlWhere += ("and sourceType = ? ");
        if (type != "")
            sqlWhere += ("and type = ? ");
        if (name != "")
            sqlWhere += ("and LOWER(name) like LOWER(?) ");
        if (region != "")
            sqlWhere += ("and region like ? ");
        if (country != "")
            sqlWhere += ("and country like ? ");

        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_Stream ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (liveURLKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, liveURLKey);
            if (label != "")
			{
				if (labelLike)
					preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
				else
					preparedStatement->setString(queryParameterIndex++, label);
			}
            if (url != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + url + "%");
            if (sourceType != "")
                preparedStatement->setString(queryParameterIndex++, sourceType);
            if (type != "")
                preparedStatement->setString(queryParameterIndex++, type);
            if (name != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + name + "%");
            if (region != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + region + "%");
            if (country != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + country + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", liveURLKey: " + to_string(liveURLKey)
				+ ", url: " + "%" + url + "%"
				+ ", sourceType: " + sourceType
				+ ", type: " + type
				+ ", name: " + "%" + name + "%"
				+ ", region: " + "%" + region + "%"
				+ ", country: " + "%" + country + "%"
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value streamsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

            lastSQLCommand = 
                string("select confKey, label, sourceType, encodersPoolKey, url, "
						"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
						"pushListenTimeout, captureLiveVideoDeviceNumber, "
						"captureLiveVideoInputFormat, captureLiveFrameRate, captureLiveWidth, "
						"captureLiveHeight, captureLiveAudioDeviceNumber, "
						"captureLiveChannelsNumber, tvSourceTVConfKey, "
						"type, description, name, "
						"region, country, "
						"imageMediaItemKey, imageUniqueName, position, userData "
						"from MMS_Conf_Stream ") 
                + sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (liveURLKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, liveURLKey);
            if (label != "")
			{
				if (labelLike)
					preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
				else
					preparedStatement->setString(queryParameterIndex++, label);
			}
            if (url != "")
                preparedStatement->setString(queryParameterIndex++,
					string("%") + url + "%");
            if (sourceType != "")
                preparedStatement->setString(queryParameterIndex++, sourceType);
            if (type != "")
                preparedStatement->setString(queryParameterIndex++, type);
            if (name != "")
                preparedStatement->setString(queryParameterIndex++,
					string("%") + name + "%");
            if (region != "")
                preparedStatement->setString(queryParameterIndex++,
					string("%") + region + "%");
            if (country != "")
                preparedStatement->setString(queryParameterIndex++,
					string("%") + country + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", liveURLKey: " + to_string(liveURLKey)
				+ ", label: " + "%" + label + "%"
				+ ", labelLike: " + to_string(labelLike)
				+ ", url: " + "%" + url + "%"
				+ ", sourceType: " + sourceType
				+ ", type: " + type
				+ ", name: " + "%" + name + "%"
				+ ", region: " + "%" + region + "%"
				+ ", country: " + "%" + country + "%"
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value streamRoot;

				int64_t confKey = resultSet->getInt64("confKey");
                field = "confKey";
                streamRoot[field] = confKey;

                field = "label";
                streamRoot[field] = static_cast<string>(
					resultSet->getString("label"));

				string sourceType = static_cast<string>(
					resultSet->getString("sourceType"));
				field = "sourceType";
				streamRoot[field] = sourceType;

                field = "encodersPoolKey";
				if (resultSet->isNull("encodersPoolKey"))
					streamRoot[field] = Json::nullValue;
				else
				{
					int64_t encodersPoolKey = resultSet->getInt64("encodersPoolKey");
					streamRoot[field] = encodersPoolKey;

					if (encodersPoolKey >= 0)
					{
						try
						{
							string encodersPoolLabel =
								getEncodersPoolDetails (encodersPoolKey);

							field = "encodersPoolLabel";
							streamRoot[field] = encodersPoolLabel;
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "getEncodersPoolDetails failed"
								+ ", confKey: " + to_string(confKey)
								+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							);
						}
					}
				}

				// if (sourceType == "IP_PULL")
				{
					field = "url";
					if (resultSet->isNull("url"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = static_cast<string>(
							resultSet->getString("url"));
				}
				// else if (sourceType == "IP_PUSH")
				{
					field = "pushProtocol";
					if (resultSet->isNull("pushProtocol"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = static_cast<string>(
							resultSet->getString("pushProtocol"));

					field = "pushEncoderKey";
					if (resultSet->isNull("pushEncoderKey"))
						streamRoot[field] = Json::nullValue;
					else
					{
						int64_t pushEncoderKey = resultSet->getInt64("pushEncoderKey");
						streamRoot[field] = pushEncoderKey;

						if (pushEncoderKey >= 0)
						{
							try
							{
								tuple<string, string, string> encoderDetails
									= getEncoderDetails (pushEncoderKey);

								string pushEncoderLabel;
								tie(pushEncoderLabel, ignore, ignore) = encoderDetails;
	
								field = "pushEncoderLabel";
								streamRoot[field] = pushEncoderLabel;
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "getEncoderDetails failed"
									+ ", pushEncoderKey: " + to_string(pushEncoderKey)
								);
							}
						}
					}

					field = "pushServerName";
					if (resultSet->isNull("pushServerName"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = static_cast<string>(
							resultSet->getString("pushServerName"));

					field = "pushServerPort";
					if (resultSet->isNull("pushServerPort"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("pushServerPort");

					field = "pushUri";
					if (resultSet->isNull("pushUri"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = static_cast<string>(
							resultSet->getString("pushUri"));

					field = "pushListenTimeout";
					if (resultSet->isNull("pushListenTimeout"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("pushListenTimeout");
				}
				// else if (sourceType == "CaptureLive")
				{
					field = "captureLiveVideoDeviceNumber";
					if (resultSet->isNull("captureLiveVideoDeviceNumber"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] =
							resultSet->getInt("captureLiveVideoDeviceNumber");

					field = "captureLiveVideoInputFormat";
					if (resultSet->isNull("captureLiveVideoInputFormat"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = static_cast<string>(
							resultSet->getString("captureLiveVideoInputFormat"));

					field = "captureLiveFrameRate";
					if (resultSet->isNull("captureLiveFrameRate"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("captureLiveFrameRate");

					field = "captureLiveWidth";
					if (resultSet->isNull("captureLiveWidth"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("captureLiveWidth");

					field = "captureLiveHeight";
					if (resultSet->isNull("captureLiveHeight"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("captureLiveHeight");

					field = "captureLiveAudioDeviceNumber";
					if (resultSet->isNull("captureLiveAudioDeviceNumber"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] =
							resultSet->getInt("captureLiveAudioDeviceNumber");

					field = "captureLiveChannelsNumber";
					if (resultSet->isNull("captureLiveChannelsNumber"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt("captureLiveChannelsNumber");
				}
				// else if (sourceType == "TV")
				{
					field = "tvSourceTVConfKey";
					if (resultSet->isNull("tvSourceTVConfKey"))
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = resultSet->getInt64("tvSourceTVConfKey");
				}

				field = "type";
				if (resultSet->isNull("type"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("type"));

                field = "description";
				if (resultSet->isNull("description"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(
						resultSet->getString("description"));

                field = "name";
				if (resultSet->isNull("name"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "region";
				if (resultSet->isNull("region"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("region"));

                field = "country";
				if (resultSet->isNull("country"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(
						resultSet->getString("country"));

                field = "imageMediaItemKey";
				if (resultSet->isNull("imageMediaItemKey"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("imageMediaItemKey");

                field = "imageUniqueName";
				if (resultSet->isNull("imageUniqueName"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(
						resultSet->getString("imageUniqueName"));

                field = "position";
				if (resultSet->isNull("position"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt("position");

                field = "userData";
				if (resultSet->isNull("userData"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(
						resultSet->getString("userData"));

                streamsRoot.append(streamRoot);
            }
        }

        field = "streams";
        responseRoot[field] = streamsRoot;

        field = "response";
        streamListRoot[field] = responseRoot;
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
    
    return streamListRoot;
}

tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
	int, string, int, int, int, int, int, int64_t>
	MMSEngineDBFacade::getStreamDetails(
    int64_t workspaceKey, string label,
	bool warningIfMissing
)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        _logger->info(__FILEREF__ + "getStreamDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t confKey;
		string sourceType;
		string encodersPoolLabel;
		string url;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		string pushServerName;		// indica il nome del server (public or internal)
		int pushServerPort = -1;
		string pushUri;
		int pushListenTimeout = -1;
		int captureLiveVideoDeviceNumber = -1;
		string captureLiveVideoInputFormat;
		int captureLiveFrameRate = -1;
		int captureLiveWidth = -1;
		int captureLiveHeight = -1;
		int captureLiveAudioDeviceNumber = -1;
		int captureLiveChannelsNumber = -1;
		int64_t tvSourceTVConfKey = -1;
		{
			lastSQLCommand = "select confKey, sourceType, "
				"encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, "
				"captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"tvSourceTVConfKey "
				"from MMS_Conf_Stream "
				"where workspaceKey = ? and label = ?";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (!resultSet->next())
			{
				string errorMessage = __FILEREF__ + "Configuration label is not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);                    
            }

			confKey = resultSet->getInt64("confKey");
			sourceType = resultSet->getString("sourceType");
			if (!resultSet->isNull("encodersPoolKey"))
			{
				int64_t encodersPoolKey = resultSet->getInt64("encodersPoolKey");

				if (encodersPoolKey >= 0)
				{
					try
					{
						encodersPoolLabel = getEncodersPoolDetails (encodersPoolKey);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "getEncodersPoolDetails failed"
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
						);
					}
				}
			}
			if (!resultSet->isNull("url"))
				url = resultSet->getString("url");
			if (!resultSet->isNull("pushProtocol"))
				pushProtocol = resultSet->getString("pushProtocol");
			if (!resultSet->isNull("pushEncoderKey"))
				pushEncoderKey = resultSet->getInt64("pushEncoderKey");
			if (!resultSet->isNull("pushServerName"))
				pushServerName = resultSet->getString("pushServerName");
			if (!resultSet->isNull("pushServerPort"))
				pushServerPort = resultSet->getInt("pushServerPort");
			if (!resultSet->isNull("pushUri"))
				pushUri = resultSet->getString("pushUri");
			if (!resultSet->isNull("pushListenTimeout"))
				pushListenTimeout = resultSet->getInt("pushListenTimeout");
			if (!resultSet->isNull("captureLiveVideoDeviceNumber"))
				captureLiveVideoDeviceNumber = resultSet->getInt("captureLiveVideoDeviceNumber");
			if (!resultSet->isNull("captureLiveVideoInputFormat"))
				captureLiveVideoInputFormat = resultSet->getString("captureLiveVideoInputFormat");
			if (!resultSet->isNull("captureLiveFrameRate"))
				captureLiveFrameRate = resultSet->getInt("captureLiveFrameRate");
			if (!resultSet->isNull("captureLiveWidth"))
				captureLiveWidth = resultSet->getInt("captureLiveWidth");
			if (!resultSet->isNull("captureLiveHeight"))
				captureLiveHeight = resultSet->getInt("captureLiveHeight");
			if (!resultSet->isNull("captureLiveAudioDeviceNumber"))
				captureLiveAudioDeviceNumber = resultSet->getInt("captureLiveAudioDeviceNumber");
			if (!resultSet->isNull("captureLiveChannelsNumber"))
				captureLiveChannelsNumber = resultSet->getInt("captureLiveChannelsNumber");
			if (!resultSet->isNull("tvSourceTVConfKey"))
				tvSourceTVConfKey = resultSet->getInt64("tvSourceTVConfKey");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, sourceType, encodersPoolLabel, url,
			pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, pushListenTimeout,
			captureLiveVideoDeviceNumber, captureLiveVideoInputFormat,
            captureLiveFrameRate, captureLiveWidth, captureLiveHeight,
			captureLiveAudioDeviceNumber, captureLiveChannelsNumber,
			tvSourceTVConfKey);
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
    catch(ConfKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "ConfKeyNotFound SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "ConfKeyNotFound SQL exception"
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

tuple<string, string, string> MMSEngineDBFacade::getStreamDetails(
    int64_t workspaceKey, int64_t confKey
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getStreamDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		string		url;
		string		channelName;
		string		userData;
        {
            lastSQLCommand = string("select url, name, userData from MMS_Conf_Stream ")
				+ "where workspaceKey = ? and confKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", confKey: " + to_string(confKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Stream failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", confKey: " + to_string(confKey)
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            url = resultSet->getString("url");
            channelName = resultSet->getString("name");
            userData = resultSet->getString("userData");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(url, channelName, userData);
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

Json::Value MMSEngineDBFacade::addSourceTVStream(
	string type,
	int64_t serviceId,
	int64_t networkId,
	int64_t transportStreamId,
	string name,
	string satellite,
	int64_t frequency,
	string lnb,
	int videoPid,
	string audioPids,
	int audioItalianPid,
	int audioEnglishPid,
	int teletextPid,
	string modulation,
	string polarization,
	int64_t symbolRate,
	int64_t bandwidthInHz,
	string country,
	string deliverySystem
)
{
	string      lastSQLCommand;
	int64_t		confKey;

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
                "insert into MMS_Conf_SourceTVStream( "
				"type, serviceId, networkId, transportStreamId, "
				"name, satellite, frequency, lnb, "
				"videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid, "
				"modulation, polarization, symbolRate, bandwidthInHz, "
				"country, deliverySystem "
				") values ("
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, type);
			if (serviceId == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, serviceId);
			if (networkId == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, networkId);
			if (transportStreamId == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, transportStreamId);
			preparedStatement->setString(queryParameterIndex++, name);
			if (satellite == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, satellite);
            preparedStatement->setInt64(queryParameterIndex++, frequency);
			if (lnb == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, lnb);
			if (videoPid == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, videoPid);
			if (audioPids == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, audioPids);
			if (audioItalianPid == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, audioItalianPid);
			if (audioEnglishPid == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, audioEnglishPid);
			if (teletextPid == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, teletextPid);
			if (country == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, modulation);
			if (polarization == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, polarization);
			if (symbolRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt64(queryParameterIndex++, symbolRate);
			if (bandwidthInHz == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt64(queryParameterIndex++, bandwidthInHz);
			if (country == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, country);
			if (deliverySystem == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, deliverySystem);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", type: " + type
				+ ", serviceId: " + to_string(serviceId)
				+ ", networkId: " + to_string(networkId)
				+ ", transportStreamId: " + to_string(transportStreamId)
				+ ", name: " + name
				+ ", satellite: " + satellite
				+ ", frequency: " + to_string(frequency)
				+ ", lnb: " + lnb
				+ ", videoPid: " + to_string(videoPid)
				+ ", audioPids: " + audioPids
				+ ", audioItalianPid: " + to_string(audioItalianPid)
				+ ", audioEnglishPid: " + to_string(audioEnglishPid)
				+ ", teletextPid: " + to_string(teletextPid)
				+ ", modulation: " + modulation
				+ ", polarization: " + polarization
				+ ", symbolRate: " + to_string(symbolRate)
				+ ", bandwidthInHz: " + to_string(bandwidthInHz)
				+ ", country: " + country
				+ ", deliverySystem: " + deliverySystem
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			confKey = getLastInsertId(conn);
        }

		Json::Value sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceTVStreamRoot = getSourceTVStreamList (
				confKey, start, rows, type, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceTVStreamsRoot[0];
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

Json::Value MMSEngineDBFacade::modifySourceTVStream(
	int64_t confKey,

	bool typeToBeModified, string type,
	bool serviceIdToBeModified, int64_t serviceId,
	bool networkIdToBeModified, int64_t networkId,
	bool transportStreamIdToBeModified, int64_t transportStreamId,
	bool nameToBeModified, string name,
	bool satelliteToBeModified, string satellite,
	bool frequencyToBeModified, int64_t frequency,
	bool lnbToBeModified, string lnb,
	bool videoPidToBeModified, int videoPid,
	bool audioPidsToBeModified, string audioPids,
	bool audioItalianPidToBeModified, int audioItalianPid,
	bool audioEnglishPidToBeModified, int audioEnglishPid,
	bool teletextPidToBeModified, int teletextPid,
	bool modulationToBeModified, string modulation,
	bool polarizationToBeModified, string polarization,
	bool symbolRateToBeModified, int64_t symbolRate,
	bool bandwidthInHzToBeModified, int64_t bandwidthInHz,
	bool countryToBeModified, string country,
	bool deliverySystemToBeModified, string deliverySystem
)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = ?");
				oneParameterPresent = true;
			}

			if (serviceIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("serviceId = ?");
				oneParameterPresent = true;
			}

			if (networkIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("networkId = ?");
				oneParameterPresent = true;
			}

			if (transportStreamIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("transportStreamId = ?");
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = ?");
				oneParameterPresent = true;
			}

			if (satelliteToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("satellite = ?");
				oneParameterPresent = true;
			}

			if (frequencyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("frequency = ?");
				oneParameterPresent = true;
			}

			if (lnbToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("lnb = ?");
				oneParameterPresent = true;
			}

			if (videoPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("videoPid = ?");
				oneParameterPresent = true;
			}

			if (audioPidsToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioPids = ?");
				oneParameterPresent = true;
			}

			if (audioItalianPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioItalianPid = ?");
				oneParameterPresent = true;
			}

			if (audioEnglishPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioEnglishPid = ?");
				oneParameterPresent = true;
			}

			if (teletextPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("teletextPid = ?");
				oneParameterPresent = true;
			}

			if (modulationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("modulation = ?");
				oneParameterPresent = true;
			}

			if (polarizationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("polarization = ?");
				oneParameterPresent = true;
			}

			if (symbolRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("symbolRate = ?");
				oneParameterPresent = true;
			}

			if (bandwidthInHzToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("bandwidthInHz = ?");
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = ?");
				oneParameterPresent = true;
			}

			if (deliverySystemToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("deliverySystem = ?");
				oneParameterPresent = true;
			}

			if (!oneParameterPresent)
            {
                string errorMessage = __FILEREF__ + "Wrong input, no parameters to be updated"
                        + ", confKey: " + to_string(confKey)
                        + ", oneParameterPresent: " + to_string(oneParameterPresent)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            lastSQLCommand = 
                string("update MMS_Conf_SourceTVStream ") + setSQL + " "
				"where confKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (typeToBeModified)
			{
				if (type == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, type);
			}
			if (serviceIdToBeModified)
			{
				if (serviceId == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, serviceId);
			}
			if (networkIdToBeModified)
			{
				if (networkId == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, networkId);
			}
			if (transportStreamIdToBeModified)
			{
				if (transportStreamId == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, transportStreamId);
			}
			if (nameToBeModified)
			{
				if (name == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, name);
			}
			if (satelliteToBeModified)
			{
				if (satellite == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, satellite);
			}
			if (frequencyToBeModified)
			{
				if (frequency == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, frequency);
			}
			if (lnbToBeModified)
			{
				if (lnb == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, lnb);
			}
			if (videoPidToBeModified)
			{
				if (videoPid == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, videoPid);
			}
			if (audioPidsToBeModified)
			{
				if (audioPids == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, audioPids);
			}
			if (audioItalianPidToBeModified)
			{
				if (audioItalianPid == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, audioItalianPid);
			}
			if (audioEnglishPidToBeModified)
			{
				if (audioEnglishPid == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, audioEnglishPid);
			}
			if (teletextPidToBeModified)
			{
				if (teletextPid == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, teletextPid);
			}
			if (modulationToBeModified)
			{
				if (modulation == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, modulation);
			}
			if (polarizationToBeModified)
			{
				if (polarization == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, polarization);
			}
			if (symbolRateToBeModified)
			{
				if (symbolRate == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, symbolRate);
			}
			if (bandwidthInHzToBeModified)
			{
				if (bandwidthInHz == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
				else
					preparedStatement->setInt64(queryParameterIndex++, bandwidthInHz);
			}
			if (countryToBeModified)
			{
				if (country == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, country);
			}
			if (deliverySystemToBeModified)
			{
				if (deliverySystem == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, deliverySystem);
			}
            preparedStatement->setInt64(queryParameterIndex++, confKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", type (" + to_string(typeToBeModified) + "): " + type
				+ ", serviceId: " + to_string(serviceId)
				+ ", networkId: " + to_string(networkId)
				+ ", transportStreamId: " + to_string(transportStreamId)
				+ ", name (" + to_string(nameToBeModified) + "): " + name
				+ ", satellite (" + to_string(satelliteToBeModified) + "): " + satellite
				+ ", frequency: " + to_string(frequency)
				+ ", lnb (" + to_string(lnbToBeModified) + "): " + lnb
				+ ", videoPid: " + to_string(videoPid)
				+ ", audioPids (" + to_string(audioPidsToBeModified) + "): " + audioPids
				+ ", audioItalianPid: " + to_string(audioItalianPid)
				+ ", audioEnglishPid: " + to_string(audioEnglishPid)
				+ ", teletextPid: " + to_string(teletextPid)
				+ ", modulation (" + to_string(modulationToBeModified) + "): " + modulation
				+ ", polarization (" + to_string(polarizationToBeModified) + "): " + polarization
				+ ", symbolRate: " + to_string(symbolRate)
				+ ", bandwidthInHz: " + to_string(bandwidthInHz)
				+ ", country (" + to_string(countryToBeModified) + "): " + country
				+ ", deliverySystem (" + to_string(deliverySystemToBeModified) + "): " + deliverySystem
				+ ", confKey: " + to_string(confKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

		Json::Value sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceTVStreamRoot = getSourceTVStreamList (
				confKey, start, rows, type, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceTVStreamsRoot[0];
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

void MMSEngineDBFacade::removeSourceTVStream(
	int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_SourceTVStream where confKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getSourceTVStreamList (
	int64_t confKey,
	int start, int rows,
	string type, int64_t serviceId, string name, int64_t frequency, string lnb,
	int videoPid, string audioPids,
	string nameOrder)
{
    string      lastSQLCommand;
    Json::Value streamListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getSourceTVStreamList"
            + ", confKey: " + to_string(confKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", type: " + type
            + ", frequency: " + to_string(frequency)
            + ", lnb: " + lnb
            + ", serviceId: " + to_string(serviceId)
            + ", name: " + name
            + ", videoPid: " + to_string(videoPid)
            + ", audioPids: " + audioPids
            + ", nameOrder: " + nameOrder
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;

            if (confKey != -1)
			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}
            
			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}
            
            if (type != "")
			{
				field = "type";
				requestParametersRoot[field] = type;
			}
            
            if (serviceId != -1)
			{
				field = "serviceId";
				requestParametersRoot[field] = serviceId;
			}
            
            if (name != "")
			{
				field = "name";
				requestParametersRoot[field] = name;
			}

            if (frequency != -1)
			{
				field = "frequency";
				requestParametersRoot[field] = frequency;
			}
            
            if (lnb != "")
			{
				field = "lnb";
				requestParametersRoot[field] = lnb;
			}
            
            if (videoPid != -1)
			{
				field = "videoPid";
				requestParametersRoot[field] = videoPid;
			}
            
            if (audioPids != "")
			{
				field = "audioPids";
				requestParametersRoot[field] = audioPids;
			}

            if (nameOrder != "")
			{
				field = "nameOrder";
				requestParametersRoot[field] = nameOrder;
			}

            field = "requestParameters";
            streamListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere;
        if (confKey != -1)
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.confKey = ? ");
			else
				sqlWhere += ("and sc.confKey = ? ");
		}
        if (type != "")
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.type = ? ");
			else
				sqlWhere += ("and sc.type = ? ");
		}
        if (serviceId != -1)
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.serviceId = ? ");
			else
				sqlWhere += ("and sc.serviceId = ? ");
		}
        if (name != "")
		{
			if (sqlWhere == "")
				sqlWhere += ("LOWER(sc.name) like LOWER(?) ");
			else
				sqlWhere += ("and LOWER(sc.name) like LOWER(?) ");
		}
        if (frequency != -1)
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.frequency = ? ");
			else
				sqlWhere += ("and sc.frequency = ? ");
		}
        if (lnb != "")
		{
			if (sqlWhere == "")
				sqlWhere += ("LOWER(sc.lnb) like LOWER(?) ");
			else
				sqlWhere += ("and LOWER(sc.lnb) like LOWER(?) ");
		}
        if (videoPid != -1)
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.videoPid = ? ");
			else
				sqlWhere += ("and sc.videoPid = ? ");
		}
        if (audioPids != "")
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.audioPids = ? ");
			else
				sqlWhere += ("and sc.audioPids = ? ");
		}
		if (sqlWhere != "")
			sqlWhere = string ("where ") + sqlWhere;

        Json::Value responseRoot;
        {
			lastSQLCommand = string("select count(*) from MMS_Conf_SourceTVStream sc ")
				+ sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
            if (type != "")
                preparedStatement->setString(queryParameterIndex++, type);
            if (serviceId != -1)
				preparedStatement->setInt64(queryParameterIndex++, serviceId);
            if (name != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + name + "%");
            if (frequency != -1)
				preparedStatement->setInt64(queryParameterIndex++, frequency);
            if (lnb != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + lnb + "%");
            if (videoPid != -1)
				preparedStatement->setInt(queryParameterIndex++, videoPid);
            if (audioPids != "")
                preparedStatement->setString(queryParameterIndex++, audioPids);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", type: " + type
				+ ", serviceId: " + to_string(serviceId)
				+ ", name: " + "%" + name + "%"
				+ ", frequency: " + to_string(frequency)
				+ ", lnb: " + "%" + lnb + "%"
				+ ", videoPid: " + to_string(videoPid)
				+ ", audioPids: " + audioPids
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value streamsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (nameOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by sc.name " + nameOrder + " ";

			lastSQLCommand = 
				string("select sc.confKey, sc.type, sc.serviceId, "
					"sc.networkId, sc.transportStreamId, sc.name, sc.satellite, "
					"sc.frequency, sc.lnb, sc.videoPid, sc.audioPids, "
					"sc.audioItalianPid, sc.audioEnglishPid, sc.teletextPid, "
					"sc.modulation, sc.polarization, sc.symbolRate, sc.bandwidthInHz, "
					"sc.country, sc.deliverySystem "
					"from MMS_Conf_SourceTVStream sc ") 
				+ sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
            if (type != "")
                preparedStatement->setString(queryParameterIndex++, type);
            if (serviceId != -1)
				preparedStatement->setInt64(queryParameterIndex++, serviceId);
            if (name != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + name + "%");
            if (frequency != -1)
				preparedStatement->setInt64(queryParameterIndex++, frequency);
            if (lnb != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + lnb + "%");
            if (videoPid != -1)
				preparedStatement->setInt(queryParameterIndex++, videoPid);
            if (audioPids != "")
                preparedStatement->setString(queryParameterIndex++, audioPids);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", type: " + type
				+ ", serviceId: " + to_string(serviceId)
				+ ", name: " + "%" + name + "%"
				+ ", frequency: " + to_string(frequency)
				+ ", lnb: " + "%" + lnb + "%"
				+ ", videoPid: " + to_string(videoPid)
				+ ", audioPids: " + audioPids
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value streamRoot;

                field = "confKey";
                streamRoot[field] = resultSet->getInt64("confKey");

                field = "type";
                streamRoot[field] = static_cast<string>(resultSet->getString("type"));

                field = "serviceId";
				if (resultSet->isNull("serviceId"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("serviceId");

                field = "networkId";
				if (resultSet->isNull("networkId"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("networkId");

                field = "transportStreamId";
				if (resultSet->isNull("transportStreamId"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("transportStreamId");

                field = "name";
                streamRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "satellite";
				if (resultSet->isNull("satellite"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("satellite"));

                field = "frequency";
                streamRoot[field] = resultSet->getInt64("frequency");

                field = "lnb";
				if (resultSet->isNull("lnb"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("lnb"));

                field = "videoPid";
				if (resultSet->isNull("videoPid"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt("videoPid");

                field = "audioPids";
				if (resultSet->isNull("audioPids"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("audioPids"));

                field = "audioItalianPid";
				if (resultSet->isNull("audioItalianPid"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt("audioItalianPid");

                field = "audioEnglishPid";
				if (resultSet->isNull("audioEnglishPid"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt("audioEnglishPid");

                field = "teletextPid";
				if (resultSet->isNull("teletextPid"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt("teletextPid");

                field = "modulation";
				if (resultSet->isNull("modulation"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("modulation"));

                field = "polarization";
				if (resultSet->isNull("polarization"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("polarization"));

                field = "symbolRate";
				if (resultSet->isNull("symbolRate"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("symbolRate");

                field = "bandwidthInHz";
				if (resultSet->isNull("bandwidthInHz"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = resultSet->getInt64("bandwidthInHz");

				field = "country";
				if (resultSet->isNull("country"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("country"));

                field = "deliverySystem";
				if (resultSet->isNull("deliverySystem"))
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = static_cast<string>(resultSet->getString("deliverySystem"));

                streamsRoot.append(streamRoot);
            }
        }

        field = "sourceTVStreams";
        responseRoot[field] = streamsRoot;

        field = "response";
        streamListRoot[field] = responseRoot;

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
    
    return streamListRoot;
}

tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
	MMSEngineDBFacade::getSourceTVStreamDetails(
	int64_t confKey, bool warningIfMissing
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        _logger->info(__FILEREF__ + "getTVStreamDetails"
            + ", confKey: " + to_string(confKey)
        );

        conn = connectionPool->borrow();
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		string type;
		int64_t serviceId;
		int64_t frequency;
		int64_t symbolRate;
		int64_t bandwidthInHz;
		string modulation;
		int videoPid;
		int audioItalianPid;
        {
			lastSQLCommand = "select type, serviceId, frequency, symbolRate, "
				"bandwidthInHz, modulation, videoPid, audioItalianPid "
				"from MMS_Conf_SourceTVStream "
				"where confKey = ?"
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage = __FILEREF__ + "Configuration is not found"
                    + ", confKey: " + to_string(confKey)
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);                    
            }

			type = resultSet->getString("type");
			serviceId = resultSet->getInt64("serviceId");
			frequency = resultSet->getInt64("frequency");
			if (resultSet->isNull("symbolRate"))
				symbolRate = -1;
			else
				symbolRate = resultSet->getInt64("symbolRate");
			if (resultSet->isNull("bandwidthInHz"))
				bandwidthInHz = -1;
			else
				bandwidthInHz = resultSet->getInt64("bandwidthInHz");
			if (!resultSet->isNull("bandwidthInHz"))
				modulation = resultSet->getString("modulation");
			videoPid = resultSet->getInt("videoPid");
			audioItalianPid = resultSet->getInt("audioItalianPid");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(type, serviceId, frequency, symbolRate, bandwidthInHz,
			modulation, videoPid, audioItalianPid);
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
    catch(ConfKeyNotFound e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "ConfKeyNotFound SQL exception"
                + ", lastSQLCommand: " + lastSQLCommand
                + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
            );
        else
            _logger->error(__FILEREF__ + "ConfKeyNotFound SQL exception"
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

int64_t MMSEngineDBFacade::addAWSChannelConf(
	int64_t workspaceKey,
	string label,
	string channelId, string rtmpURL, string playURL,
	string type)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
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
                "insert into MMS_Conf_AWSChannel(workspaceKey, label, channelId, "
				"rtmpURL, playURL, type, reservedByIngestionJobKey) values ("
                "?, ?, ?, ?, ?, ?, NULL)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, channelId);
            preparedStatement->setString(queryParameterIndex++, rtmpURL);
            preparedStatement->setString(queryParameterIndex++, playURL);
            preparedStatement->setString(queryParameterIndex++, type);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", channelId: " + channelId
				+ ", rtmpURL: " + rtmpURL
				+ ", playURL: " + playURL
				+ ", type: " + type
				+ ", elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyAWSChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
	string channelId, string rtmpURL, string playURL,
	string type)
{
    string      lastSQLCommand;
    
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
				"update MMS_Conf_AWSChannel set label = ?, channelId = ?, rtmpURL = ?, "
				"playURL = ?, type = ? "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, channelId);
            preparedStatement->setString(queryParameterIndex++, rtmpURL);
            preparedStatement->setString(queryParameterIndex++, playURL);
            preparedStatement->setString(queryParameterIndex++, type);
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", channelId: " + channelId
				+ ", rtmpURL: " + rtmpURL
				+ ", playURL: " + playURL
				+ ", type: " + type
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeAWSChannelConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_AWSChannel where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getAWSChannelConfList (
	int64_t workspaceKey)
{
    string      lastSQLCommand;
    Json::Value awsChannelConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getAWSChannelConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = connectionPool->borrow();	
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
            awsChannelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_Conf_AWSChannel ")
                    + sqlWhere;

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
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value awsChannelRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				string ("select confKey, label, channelId, rtmpURL, playURL, ")
				+ "type, reservedByIngestionJobKey "
				+ "from MMS_Conf_AWSChannel "
                + sqlWhere;

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
            while (resultSet->next())
            {
                Json::Value awsChannelConfRoot;

                field = "confKey";
                awsChannelConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                awsChannelConfRoot[field] = static_cast<string>(
					resultSet->getString("label"));

                field = "channelId";
                awsChannelConfRoot[field] = static_cast<string>(
					resultSet->getString("channelId"));

                field = "rtmpURL";
                awsChannelConfRoot[field] = static_cast<string>(
					resultSet->getString("rtmpURL"));

                field = "playURL";
                awsChannelConfRoot[field] = static_cast<string>(
					resultSet->getString("playURL"));

                field = "type";
                awsChannelConfRoot[field] = static_cast<string>(
					resultSet->getString("type"));

                field = "reservedByIngestionJobKey";
				if (resultSet->isNull("reservedByIngestionJobKey"))
					awsChannelConfRoot[field] = Json::nullValue;
				else
					awsChannelConfRoot[field]
						= resultSet->getInt64("reservedByIngestionJobKey");

                awsChannelRoot.append(awsChannelConfRoot);
            }
        }

        field = "awsChannelConf";
        responseRoot[field] = awsChannelRoot;

        field = "response";
        awsChannelConfListRoot[field] = responseRoot;

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
    
    return awsChannelConfListRoot;
}

tuple<string, string, string, bool>
	MMSEngineDBFacade::reserveAWSChannel(
	int64_t workspaceKey, string label, string type,
	int64_t ingestionJobKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "reserveAWSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", label: " + label
			+ ", type: " + type
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

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

		int64_t reservedConfKey;
		string reservedChannelId;
		string reservedRtmpURL;
		string reservedPlayURL;
		int64_t reservedByIngestionJobKey = -1;

		{
			if (label == "")
				lastSQLCommand =
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel " 
					"where workspaceKey = ? and type = ? "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = ?)"
					"for update";
			else
				lastSQLCommand =
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel " 
					"where workspaceKey = ? and type = ? "
					"and label = ? "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = ?) "
					"for update";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setString(queryParameterIndex++, type);
            if (label != "")
				preparedStatement->setString(queryParameterIndex++, label);
			preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", type: " + type
				+ ", label: " + label
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
			{
				string errorMessage = string("No ") + type + " AWS Channel found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = resultSet->getInt64("confKey");
			reservedChannelId = resultSet->getString("channelId");
			reservedRtmpURL = resultSet->getString("rtmpURL");
			reservedPlayURL = resultSet->getString("playURL");
			if (!resultSet->isNull("reservedByIngestionJobKey"))
				reservedByIngestionJobKey = resultSet->getInt64("reservedByIngestionJobKey");
		}

		if (reservedByIngestionJobKey == -1)
        {
			lastSQLCommand = 
				"update MMS_Conf_AWSChannel set reservedByIngestionJobKey = ? "
				"where confKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, reservedConfKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", confKey: " + to_string(reservedConfKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(reservedConfKey)
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

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedChannelId, reservedRtmpURL,
			reservedPlayURL, channelAlreadyReserved);
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

string MMSEngineDBFacade::releaseAWSChannel(
	int64_t workspaceKey, int64_t ingestionJobKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "releaseAWSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t reservedConfKey;
		string reservedChannelId;

        {
			lastSQLCommand =
				"select confKey, channelId from MMS_Conf_AWSChannel " 
				"where workspaceKey = ? and reservedByIngestionJobKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
			{
				string errorMessage = string("No AWS Channel found")
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = resultSet->getInt64("confKey");
			reservedChannelId = resultSet->getString("channelId");
		}

        {
			lastSQLCommand = 
				"update MMS_Conf_AWSChannel set reservedByIngestionJobKey = NULL "
				"where confKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, reservedConfKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(reservedConfKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(reservedConfKey)
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
        connectionPool->unborrow(conn);
		conn = nullptr;


		return reservedChannelId;
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

int64_t MMSEngineDBFacade::addFTPConf(
    int64_t workspaceKey,
    string label,
    string server, int port, string userName, string password, string remoteDirectory)
{
    string      lastSQLCommand;
    int64_t     confKey;
    
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", server: " + server
				+ ", port: " + to_string(port)
				+ ", userName: " + userName
				+ ", password: " + password
				+ ", remoteDirectory: " + remoteDirectory
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", server: " + server
				+ ", port: " + to_string(port)
				+ ", userName: " + userName
				+ ", password: " + password
				+ ", remoteDirectory: " + remoteDirectory
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeFTPConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_FTP where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getFTPConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value ftpConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getFTPConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = connectionPool->borrow();	
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    return ftpConfListRoot;
}

tuple<string, int, string, string, string> MMSEngineDBFacade::getFTPByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
    string      lastSQLCommand;
	tuple<string, int, string, string, string> ftp;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getFTPByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = connectionPool->borrow();	
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", addresses: " + addresses
				+ ", subject: " + subject
				+ ", message: " + message
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", addresses: " + addresses
				+ ", subject: " + subject
				+ ", message: " + message
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

void MMSEngineDBFacade::removeEMailConf(
    int64_t workspaceKey,
    int64_t confKey)
{
    string      lastSQLCommand;
    
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
                "delete from MMS_Conf_EMail where confKey = ? and workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, confKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", confKey: " + to_string(confKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
}

Json::Value MMSEngineDBFacade::getEMailConfList (
        int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value emailConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEMailConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        conn = connectionPool->borrow();	
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    return emailConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getEMailByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
    string      lastSQLCommand;
	tuple<string, string, string> email;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {        
        _logger->info(__FILEREF__ + "getEMailByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = connectionPool->borrow();	
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
    
    return email;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getStreamInputRoot(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	string configurationLabel,
	int maxWidth, string userAgent, string otherInputOptions,
	Json::Value drawTextDetailsRoot
)
{
	Json::Value streamInputRoot;

    try
    {
		int64_t confKey = -1;
		string streamSourceType;
		string encodersPoolLabel;
		string pullUrl;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		string pushServerName;	// indica il nome del server (public or internal)
		int pushServerPort = -1;
		string pushUri;
		int pushListenTimeout = -1;
		int captureVideoDeviceNumber = -1;
		string captureVideoInputFormat;
		int captureFrameRate = -1;
		int captureWidth = -1;
		int captureHeight = -1;
		int captureAudioDeviceNumber = -1;
		int captureChannelsNumber = -1;
		int64_t tvSourceTVConfKey = -1;

		{
			bool warningIfMissing = false;
			tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
				int, string, int, int, int, int, int, int64_t>
				channelConfDetails = getStreamDetails(
				workspace->_workspaceKey, configurationLabel, warningIfMissing);
			tie(confKey, streamSourceType,
				encodersPoolLabel,
				pullUrl,
				pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri,
				pushListenTimeout,
				captureVideoDeviceNumber,
				captureVideoInputFormat,
				captureFrameRate, captureWidth, captureHeight,
				captureAudioDeviceNumber, captureChannelsNumber,
				tvSourceTVConfKey) = channelConfDetails;

			// default is IP_PULL
			if (streamSourceType == "")
				streamSourceType = "IP_PULL";
		}

		string tvType;
		int64_t tvServiceId = -1;
		int64_t tvFrequency = -1;
		int64_t tvSymbolRate = -1;
		int64_t tvBandwidthInHz = -1;
		string tvModulation;
		int tvVideoPid = -1;
		int tvAudioItalianPid = -1;
		string liveURL;

		if (streamSourceType == "IP_PULL")
		{
			liveURL = pullUrl;

			string youTubePrefix1 ("https://www.youtube.com/");
			string youTubePrefix2 ("https://youtu.be/");
			if (
				(liveURL.size() >= youTubePrefix1.size()
					&& 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1))
				||
				(liveURL.size() >= youTubePrefix2.size()
					&& 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2))
				)
			{
				liveURL = getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
			}
		}
		else if (streamSourceType == "IP_PUSH")
		{
			liveURL = pushProtocol + "://" + pushServerName
				+ ":" + to_string(pushServerPort) + pushUri;
		}
		else if (streamSourceType == "TV")
		{
			bool warningIfMissing = false;
			tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
				tvChannelConfDetails = getSourceTVStreamDetails(
				tvSourceTVConfKey, warningIfMissing);

			tie(tvType, tvServiceId, tvFrequency,
				tvSymbolRate, tvBandwidthInHz, tvModulation,
				tvVideoPid, tvAudioItalianPid) = tvChannelConfDetails;
		}

		string field = "confKey";
		streamInputRoot[field] = confKey;

		field = "configurationLabel";
		streamInputRoot[field] = configurationLabel;

		field = "streamSourceType";
		streamInputRoot[field] = streamSourceType;

		field = "pushEncoderKey";
		streamInputRoot[field] = pushEncoderKey;

		field = "pushServerName";
		streamInputRoot[field] = pushServerName;

		field = "encodersPoolLabel";
		streamInputRoot[field] = encodersPoolLabel;

		field = "url";
		streamInputRoot[field] = liveURL;

		field = "drawTextDetails";
		streamInputRoot[field] = drawTextDetailsRoot;

		if (maxWidth != -1)
		{
			field = "maxWidth";
			streamInputRoot[field] = maxWidth;
		}

		if (userAgent != "")
		{
			field = "userAgent";
			streamInputRoot[field] = userAgent;
		}

		if (otherInputOptions != "")
		{
			field = "otherInputOptions";
			streamInputRoot[field] = otherInputOptions;
		}

		if (streamSourceType == "IP_PUSH")
		{
			field = "pushListenTimeout";
			streamInputRoot[field] = pushListenTimeout;
		}

		if (streamSourceType == "CaptureLive")
		{
			field = "captureVideoDeviceNumber";
			streamInputRoot[field] = captureVideoDeviceNumber;

			field = "captureVideoInputFormat";
			streamInputRoot[field] = captureVideoInputFormat;

			field = "captureFrameRate";
			streamInputRoot[field] = captureFrameRate;

			field = "captureWidth";
			streamInputRoot[field] = captureWidth;

			field = "captureHeight";
			streamInputRoot[field] = captureHeight;

			field = "captureAudioDeviceNumber";
			streamInputRoot[field] = captureAudioDeviceNumber;

			field = "captureChannelsNumber";
			streamInputRoot[field] = captureChannelsNumber;
		}

		if (streamSourceType == "TV")
		{
			field = "tvType";
			streamInputRoot[field] = tvType;

			field = "tvServiceId";
			streamInputRoot[field] = tvServiceId;

			field = "tvFrequency";
			streamInputRoot[field] = tvFrequency;

			field = "tvSymbolRate";
			streamInputRoot[field] = tvSymbolRate;

			field = "tvBandwidthInHz";
			streamInputRoot[field] = tvBandwidthInHz;

			field = "tvModulation";
			streamInputRoot[field] = tvModulation;

			field = "tvVideoPid";
			streamInputRoot[field] = tvVideoPid;

			field = "tvAudioItalianPid";
			streamInputRoot[field] = tvAudioItalianPid;
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getStreamInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getStreamInputRoot failed"
        );
        
        throw e;
    }

	return streamInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getVodInputRoot(
	MMSEngineDBFacade::ContentType vodContentType,
	vector<tuple<int64_t, string, string, string>>& sources,
	Json::Value drawTextDetailsRoot
)
{
	Json::Value vodInputRoot;

    try
    {
		string field = "vodContentType";
		vodInputRoot[field] = MMSEngineDBFacade::toString(vodContentType);

		Json::Value sourcesRoot(Json::arrayValue);

		for (tuple<int64_t, string, string, string> source: sources)
		{
			int64_t physicalPathKey;
			string mediaItemTitle;
			string sourcePhysicalPathName;
			string sourcePhysicalDeliveryURL;

			tie(physicalPathKey, mediaItemTitle, sourcePhysicalPathName,
				sourcePhysicalDeliveryURL) = source;


			Json::Value sourceRoot;

			field = "mediaItemTitle";
			sourceRoot[field] = mediaItemTitle;

			field = "sourcePhysicalPathName";
			sourceRoot[field] = sourcePhysicalPathName;

			field = "physicalPathKey";
			sourceRoot[field] = physicalPathKey;

			field = "sourcePhysicalDeliveryURL";
			sourceRoot[field] = sourcePhysicalDeliveryURL;

			sourcesRoot.append(sourceRoot);
		}

		field = "sources";
		vodInputRoot[field] = sourcesRoot;

		field = "drawTextDetails";
		vodInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
        );
        
        throw e;
    }

	return vodInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getCountdownInputRoot(
	string mmsSourceVideoAssetPathName,
	string mmsSourceVideoAssetDeliveryURL,
	int64_t physicalPathKey,
	int64_t videoDurationInMilliSeconds,
	Json::Value drawTextDetailsRoot
)
{
	Json::Value countdownInputRoot;

    try
    {
		string field = "mmsSourceVideoAssetPathName";
		countdownInputRoot[field] = mmsSourceVideoAssetPathName;

		field = "mmsSourceVideoAssetDeliveryURL";
		countdownInputRoot[field] = mmsSourceVideoAssetDeliveryURL;

		field = "physicalPathKey";
		countdownInputRoot[field] = physicalPathKey;

		field = "videoDurationInMilliSeconds";
		countdownInputRoot[field] = videoDurationInMilliSeconds;

		field = "drawTextDetails";
		countdownInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getCountdownInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
        );
        
        throw e;
    }

	return countdownInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getDirectURLInputRoot(
	string url, Json::Value drawTextDetailsRoot
)
{
	Json::Value directURLInputRoot;

    try
    {
		string field = "url";
		directURLInputRoot[field] = url;

		field = "drawTextDetails";
		directURLInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "getDirectURLInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "getDirectURLInputRoot failed"
        );
        
        throw e;
    }

	return directURLInputRoot;
}

string MMSEngineDBFacade::getStreamingYouTubeLiveURL(
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t confKey,
	string liveURL
)
{

	string streamingYouTubeLiveURL;

	long hoursFromLastCalculatedURL;
	pair<long, string> lastYouTubeURLDetails;
	try
	{
		lastYouTubeURLDetails = getLastYouTubeURLDetails(workspace, ingestionJobKey, confKey);

		string lastCalculatedURL;

		tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

		long retrieveStreamingYouTubeURLPeriodInHours = 5;	// 5 hours

		_logger->info(__FILEREF__
			+ "check youTubeURLCalculate"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
			+ ", retrieveStreamingYouTubeURLPeriodInHours: " + to_string(retrieveStreamingYouTubeURLPeriodInHours)
		);
		if (hoursFromLastCalculatedURL < retrieveStreamingYouTubeURLPeriodInHours)
			streamingYouTubeLiveURL = lastCalculatedURL;
	}
	catch(runtime_error e)
	{
		string errorMessage = __FILEREF__
			+ "youTubeURLCalculate. getLastYouTubeURLDetails failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", YouTube URL: " + streamingYouTubeLiveURL
		;
		_logger->error(errorMessage);
	}

	if (streamingYouTubeLiveURL == "")
	{
		try
		{
			FFMpeg ffmpeg (_configuration, _logger);
			pair<string, string> streamingLiveURLDetails =
				ffmpeg.retrieveStreamingYouTubeURL(ingestionJobKey, liveURL);

			tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

			_logger->info(__FILEREF__ + "youTubeURLCalculate. Retrieve streaming YouTube URL"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", confKey: " + to_string(confKey)
				+ ", initial YouTube URL: " + liveURL
				+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
				+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
			);
		}
		catch(runtime_error e)
		{
			// in case ffmpeg.retrieveStreamingYouTubeURL fails
			// we will use the last saved URL
			tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

			string errorMessage = __FILEREF__
				+ "youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", confKey: " + to_string(confKey)
				+ ", YouTube URL: " + streamingYouTubeLiveURL
			;
			_logger->error(errorMessage);

			try
			{
				string firstLineOfErrorMessage;
				{
					string firstLine;
					stringstream ss(errorMessage);
					if (getline(ss, firstLine))
						firstLineOfErrorMessage = firstLine;
					else
						firstLineOfErrorMessage = errorMessage;
				}

				appendIngestionJobErrorMessage(ingestionJobKey, firstLineOfErrorMessage);
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				);
			}

			if (streamingYouTubeLiveURL == "")
			{
				// 2020-04-21: let's go ahead because it would be managed
				// the killing of the encodingJob
				// 2020-09-17: it does not have sense to continue
				//	if we do not have the right URL (m3u8)
				throw YouTubeURLNotRetrieved();
			}
		}

		if (streamingYouTubeLiveURL != "")
		{
			try
			{
				updateChannelDataWithNewYouTubeURL(workspace, ingestionJobKey,
					confKey, streamingYouTubeLiveURL);
			}
			catch(runtime_error e)
			{
				string errorMessage = __FILEREF__
					+ "youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(confKey)
					+ ", YouTube URL: " + streamingYouTubeLiveURL
				;
				_logger->error(errorMessage);
			}
		}
	}
	else
	{
		_logger->info(__FILEREF__ + "youTubeURLCalculate. Reuse a previous streaming YouTube URL"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", initial YouTube URL: " + liveURL
			+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
			+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
		);
	}

	return streamingYouTubeLiveURL;
}

pair<long,string> MMSEngineDBFacade::getLastYouTubeURLDetails(
	shared_ptr<Workspace> workspace, int64_t ingestionKey, int64_t confKey
)
{
	long hoursFromLastCalculatedURL = -1;
	string lastCalculatedURL;

	try
	{
		tuple<string, string, string> channelDetails = getStreamDetails(
			workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot = JSONUtils::toJson(ingestionKey, -1, channelData);


		string field;

		Json::Value mmsDataRoot;
		{
			field = "mmsData";
			if (!JSONUtils::isMetadataPresent(channelDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no mmsData present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", confKey: " + to_string(confKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			mmsDataRoot = channelDataRoot[field];
		}

		Json::Value youTubeURLsRoot(Json::arrayValue);
		{
			field = "youTubeURLs";
			if (!JSONUtils::isMetadataPresent(mmsDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no youTubeURLs present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", confKey: " + to_string(confKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			youTubeURLsRoot = mmsDataRoot[field];
		}

		if (youTubeURLsRoot.size() == 0)
		{
			_logger->info(__FILEREF__ + "no youTubeURL present"                
				+ ", ingestionKey: " + to_string(ingestionKey)
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", confKey: " + to_string(confKey)
			);

			return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
		}

		{
			Json::Value youTubeLiveURLRoot = youTubeURLsRoot[youTubeURLsRoot.size() - 1];

			time_t tNow;
			{
				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmNow;

				localtime_r (&utcNow, &tmNow);
				tNow = mktime(&tmNow);
			}

			time_t tLastCalculatedURLTime;
			{
				unsigned long       ulYear;
				unsigned long		ulMonth;
				unsigned long		ulDay;
				unsigned long		ulHour;
				unsigned long		ulMinutes;
				unsigned long		ulSeconds;
				int					sscanfReturn;

				field = "timestamp";
				string timestamp = JSONUtils::asString(youTubeLiveURLRoot, field, "");

				if ((sscanfReturn = sscanf (timestamp.c_str(),
					"%4lu-%2lu-%2lu %2lu:%2lu:%2lu",
					&ulYear,
					&ulMonth,
					&ulDay,
					&ulHour,
					&ulMinutes,
					&ulSeconds)) != 6)
				{
					string errorMessage = __FILEREF__ + "timestamp has a wrong format (sscanf failed)"                
						+ ", ingestionKey: " + to_string(ingestionKey)
						+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
						+ ", confKey: " + to_string(confKey)
						+ ", sscanfReturn: " + to_string(sscanfReturn)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmLastCalculatedURL;

				localtime_r (&utcNow, &tmLastCalculatedURL);

				tmLastCalculatedURL.tm_year	= ulYear - 1900;
				tmLastCalculatedURL.tm_mon	= ulMonth - 1;
				tmLastCalculatedURL.tm_mday	= ulDay;
				tmLastCalculatedURL.tm_hour	= ulHour;
				tmLastCalculatedURL.tm_min	= ulMinutes;
				tmLastCalculatedURL.tm_sec	= ulSeconds;

				tLastCalculatedURLTime = mktime(&tmLastCalculatedURL);
			}

			hoursFromLastCalculatedURL = (tNow - tLastCalculatedURLTime) / 3600;

			field = "youTubeURL";
			lastCalculatedURL = JSONUtils::asString(youTubeLiveURLRoot, field, "");
		}

		return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
	}
	catch(...)
	{
		string errorMessage = string("getLastYouTubeURLDetails failed")
			+ ", ingestionKey: " + to_string(ingestionKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", confKey: " + to_string(confKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineDBFacade::updateChannelDataWithNewYouTubeURL(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	int64_t confKey, string streamingYouTubeLiveURL
)
{
	try
	{
		tuple<string, string, string> channelDetails = getStreamDetails(
			workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot = JSONUtils::toJson(ingestionJobKey, -1, channelData);

		// add streamingYouTubeLiveURL info to the channelData
		{
			string field;

			Json::Value youTubeLiveURLRoot;
			{
				char strNow[64];
				{
					time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

					tm tmNow;

					localtime_r (&utcNow, &tmNow);
					sprintf (strNow, "%04d-%02d-%02d %02d:%02d:%02d",
						tmNow. tm_year + 1900,
						tmNow. tm_mon + 1,
						tmNow. tm_mday,
						tmNow. tm_hour,
						tmNow. tm_min,
						tmNow. tm_sec);
				}
				field = "timestamp";
				youTubeLiveURLRoot[field] = strNow;

				field = "youTubeURL";
				youTubeLiveURLRoot[field] = streamingYouTubeLiveURL;
			}

			Json::Value mmsDataRoot;
			{
				field = "mmsData";
				if (JSONUtils::isMetadataPresent(channelDataRoot, field))
					mmsDataRoot = channelDataRoot[field];
			}

			Json::Value previousYouTubeURLsRoot(Json::arrayValue);
			{
				field = "youTubeURLs";
				if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
					previousYouTubeURLsRoot = mmsDataRoot[field];
			}

			Json::Value youTubeURLsRoot(Json::arrayValue);

			// maintain the last 10 URLs
			int youTubeURLIndex;
			if (previousYouTubeURLsRoot.size() > 10)
				youTubeURLIndex = 10;
			else
				youTubeURLIndex = previousYouTubeURLsRoot.size();
			for (; youTubeURLIndex >= 0; youTubeURLIndex--)
				youTubeURLsRoot.append(previousYouTubeURLsRoot[youTubeURLIndex]);
			youTubeURLsRoot.append(youTubeLiveURLRoot);

			field = "youTubeURLs";
			mmsDataRoot[field] = youTubeURLsRoot;

			field = "mmsData";
			channelDataRoot[field] = mmsDataRoot;
		}

		bool labelToBeModified = false;
		string label;
		bool sourceTypeToBeModified = false;
		string sourceType;
		bool encodersPoolToBeModified = false;
		int64_t encodersPoolKey;
		bool urlToBeModified = false;
		string url;
		bool pushProtocolToBeModified = false;
		string pushProtocol;
		bool pushEncoderKeyToBeModified = false;
		int64_t pushEncoderKey = -1;
		bool pushServerNameToBeModified = false;
		string pushServerName;
		bool pushServerPortToBeModified = false;
		int pushServerPort = -1;
		bool pushUriToBeModified = false;
		string pushUri;
		bool pushListenTimeoutToBeModified = false;
		int pushListenTimeout = -1;
		bool captureVideoDeviceNumberToBeModified = false;
		int captureVideoDeviceNumber = -1;
		bool captureVideoInputFormatToBeModified = false;
		string captureVideoInputFormat;
		bool captureFrameRateToBeModified = false;
		int captureFrameRate = -1;
		bool captureWidthToBeModified = false;
		int captureWidth = -1;
		bool captureHeightToBeModified = false;
		int captureHeight = -1;
		bool captureAudioDeviceNumberToBeModified = false;
		int captureAudioDeviceNumber = -1;
		bool captureChannelsNumberToBeModified = false;
		int captureChannelsNumber = -1;
		bool tvSourceTVConfKeyToBeModified = false;
		int64_t tvSourceTVConfKey = -1;
		bool typeToBeModified = false;
		string type;
		bool descriptionToBeModified = false;
		string description;
		bool nameToBeModified = false;
		string name;
		bool regionToBeModified = false;
		string region;
		bool countryToBeModified = false;
		string country;
		bool imageToBeModified = false;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		bool positionToBeModified = false;
		int position = -1;
		bool channelDataToBeModified = true;

		modifyStream(
			confKey,
			workspace->_workspaceKey,
			labelToBeModified, label,
			sourceTypeToBeModified, sourceType,
			encodersPoolToBeModified, encodersPoolKey,
			urlToBeModified, url,
			pushProtocolToBeModified, pushProtocol,
			pushEncoderKeyToBeModified, pushEncoderKey,
			pushServerNameToBeModified, pushServerName,
			pushServerPortToBeModified, pushServerPort,
			pushUriToBeModified, pushUri,
			pushListenTimeoutToBeModified, pushListenTimeout,
			captureVideoDeviceNumberToBeModified, captureVideoDeviceNumber,
			captureVideoInputFormatToBeModified, captureVideoInputFormat,
			captureFrameRateToBeModified, captureFrameRate,
			captureWidthToBeModified, captureWidth,
			captureHeightToBeModified, captureHeight,
			captureAudioDeviceNumberToBeModified, captureAudioDeviceNumber,
			captureChannelsNumberToBeModified, captureChannelsNumber,
			tvSourceTVConfKeyToBeModified, tvSourceTVConfKey,
			typeToBeModified, type,
			descriptionToBeModified, description,
			nameToBeModified, name,
			regionToBeModified, region,
			countryToBeModified, country,
			imageToBeModified, imageMediaItemKey, imageUniqueName,
			positionToBeModified, position,
			channelDataToBeModified, channelDataRoot);
	}
	catch(...)
	{
		string errorMessage = string("updateChannelDataWithNewYouTubeURL failed")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", confKey: " + to_string(confKey)
			+ ", streamingYouTubeLiveURL: " + streamingYouTubeLiveURL
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

