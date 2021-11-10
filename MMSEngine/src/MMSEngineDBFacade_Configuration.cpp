
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

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

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
                "update MMS_Conf_YouTube set label = ?, refreshToken = ? "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
                string ("select confKey, label, refreshToken from MMS_Conf_YouTube ") 
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", pageToken: " + pageToken
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", pageToken: " + pageToken
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

        Json::Value facebookRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select confKey, label, pageToken from MMS_Conf_Facebook ") 
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

Json::Value MMSEngineDBFacade::addChannelConf(
    int64_t workspaceKey,
    string label,
	string sourceType,
	string url,
	string pushProtocol,
	string pushServerName,
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
	int64_t satSourceSATConfKey,
	string type,
	string description,
	string name,
	string region,
	string country,
	int64_t imageMediaItemKey,
	string imageUniqueName,
	int position,
	Json::Value channelData)
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
			string sChannelData;
			if (channelData != Json::nullValue)
			{
				Json::StreamWriterBuilder wbuilder;
				sChannelData = Json::writeString(wbuilder, channelData);
			}

            lastSQLCommand = 
                "insert into MMS_Conf_IPChannel(workspaceKey, label, sourceType, "
				"url, "
				"pushProtocol, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"satSourceSATConfKey, "
				"type, description, name, "
				"region, country, imageMediaItemKey, imageUniqueName, "
				"position, channelData) values ("
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
				"?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setString(queryParameterIndex++, sourceType);
			if (url == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, url);

			if (pushProtocol == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, pushProtocol);
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
			if (satSourceSATConfKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, satSourceSATConfKey);

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
			if (sChannelData == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, sChannelData);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", url: " + url
				+ ", type: " + type
				+ ", description: " + description
				+ ", name: " + name
				+ ", region: " + region
				+ ", country: " + country
				+ ", imageMediaItemKey: " + to_string(imageMediaItemKey)
				+ ", imageUniqueName: " + imageUniqueName
				+ ", position: " + to_string(position)
				+ ", sChannelData: " + sChannelData
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            confKey = getLastInsertId(conn);
        }

		Json::Value channelConfRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			string url;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value channelConfListRoot = getChannelConfList (
				workspaceKey, confKey,
				start, rows, label, url, type, name, region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(channelConfListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = channelConfListRoot[field];

			field = "channelConf";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			channelConfRoot = responseRoot[field];

			if (channelConfRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return channelConfRoot[0];
    }
    catch(sql::SQLException se)
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

Json::Value MMSEngineDBFacade::modifyChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
    bool labelToBeModified, string label,

	bool sourceTypeToBeModified, string sourceType,
	bool urlToBeModified, string url,
	bool pushProtocolToBeModified, string pushProtocol,
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
	bool satSourceSATConfKeyToBeModified, int64_t satSourceSATConfKey,

	bool typeToBeModified, string type,
	bool descriptionToBeModified, string description,
	bool nameToBeModified, string name,
	bool regionToBeModified, string region,
	bool countryToBeModified, string country,
	bool imageToBeModified, int64_t imageMediaItemKey, string imageUniqueName,
	bool positionToBeModified, int position,
	bool channelDataToBeModified, Json::Value channelData)
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

			if (satSourceSATConfKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("satSourceSATConfKey = ?");
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

			string sChannelData;
			if (channelDataToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("channelData = ?");
				oneParameterPresent = true;

				if (channelData != Json::nullValue)
				{
					Json::StreamWriterBuilder wbuilder;
					sChannelData = Json::writeString(wbuilder, channelData);
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
                string("update MMS_Conf_IPChannel ") + setSQL + " "
				"where confKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (labelToBeModified)
				preparedStatement->setString(queryParameterIndex++, label);
			if (sourceTypeToBeModified)
				preparedStatement->setString(queryParameterIndex++, sourceType);
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
			if (satSourceSATConfKeyToBeModified)
			{
				if (satSourceSATConfKey == -1)
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::BIGINT);
				else
					preparedStatement->setInt(queryParameterIndex++,
						satSourceSATConfKey);
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
			if (channelDataToBeModified)
			{
				if (sChannelData == "")
					preparedStatement->setNull(queryParameterIndex++,
						sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, sChannelData);
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
				+ ", url (" + to_string(urlToBeModified) + "): " + url
				+ ", pushProtocol (" + to_string(pushProtocolToBeModified) + "): "
					+ pushProtocol
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
				+ ", sChannelData (" + to_string(channelDataToBeModified) + "): "
					+ sChannelData
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

		Json::Value channelConfRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			string url;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value channelConfListRoot = getChannelConfList (
				workspaceKey, confKey,
				start, rows, label, url, type, name, region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(channelConfListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = channelConfListRoot[field];

			field = "channelConf";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			channelConfRoot = responseRoot[field];

			if (channelConfRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return channelConfRoot[0];
	}
    catch(sql::SQLException se)
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

void MMSEngineDBFacade::removeChannelConf(
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
                "delete from MMS_Conf_IPChannel where confKey = ? and workspaceKey = ?";
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

Json::Value MMSEngineDBFacade::getChannelConfList (
	int64_t workspaceKey, int64_t liveURLKey,
	int start, int rows,
	string label, string url, string type, string name, string region, string country,
	string labelOrder	// "" or "asc" or "desc"
)
{
    string      lastSQLCommand;
    Json::Value channelConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getLiveURLConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", liveURLKey: " + to_string(liveURLKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", label: " + label
            + ", url: " + url
            + ", type: " + type
            + ", name: " + name
            + ", region: " + region
            + ", country: " + country
            + ", labelOrder: " + labelOrder
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
            
            if (url != "")
			{
				field = "url";
				requestParametersRoot[field] = url;
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
            channelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where workspaceKey = ? ");
        if (liveURLKey != -1)
			sqlWhere += ("and confKey = ? ");
        if (label != "")
            sqlWhere += ("and LOWER(label) like LOWER(?) ");
        if (url != "")
            sqlWhere += ("and url like ? ");
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
                string("select count(*) from MMS_Conf_IPChannel ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (liveURLKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, liveURLKey);
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            if (url != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + url + "%");
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

        Json::Value channelRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

            lastSQLCommand = 
                string("select confKey, label, sourceType, url, "
						"pushProtocol, pushServerName, pushServerPort, pushUri, "
						"pushListenTimeout, captureLiveVideoDeviceNumber, "
						"captureLiveVideoInputFormat, captureLiveFrameRate, captureLiveWidth, "
						"captureLiveHeight, captureLiveAudioDeviceNumber, "
						"captureLiveChannelsNumber, satSourceSATConfKey, "
						"type, description, name, "
						"region, country, "
						"imageMediaItemKey, imageUniqueName, position, channelData "
						"from MMS_Conf_IPChannel ") 
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
                preparedStatement->setString(queryParameterIndex++,
					string("%") + label + "%");
            if (url != "")
                preparedStatement->setString(queryParameterIndex++,
					string("%") + url + "%");
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
				+ ", url: " + "%" + url + "%"
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
                Json::Value channelConfRoot;

                field = "confKey";
                channelConfRoot[field] = resultSet->getInt64("confKey");

                field = "label";
                channelConfRoot[field] = static_cast<string>(
					resultSet->getString("label"));

                field = "sourceType";
                channelConfRoot[field] = static_cast<string>(
					resultSet->getString("sourceType"));

                field = "url";
				if (resultSet->isNull("url"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(
						resultSet->getString("url"));

                field = "pushProtocol";
				if (resultSet->isNull("pushProtocol"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(
						resultSet->getString("pushProtocol"));

                field = "pushServerName";
				if (resultSet->isNull("pushServerName"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(
						resultSet->getString("pushServerName"));

                field = "pushServerPort";
				if (resultSet->isNull("pushServerPort"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("pushServerPort");

                field = "pushUri";
				if (resultSet->isNull("pushUri"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(
						resultSet->getString("pushUri"));

                field = "pushListenTimeout";
				if (resultSet->isNull("pushListenTimeout"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("pushListenTimeout");

                field = "captureLiveVideoDeviceNumber";
				if (resultSet->isNull("captureLiveVideoDeviceNumber"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] =
						resultSet->getInt("captureLiveVideoDeviceNumber");

                field = "captureLiveVideoInputFormat";
				if (resultSet->isNull("captureLiveVideoInputFormat"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(
						resultSet->getString("captureLiveVideoInputFormat"));

                field = "captureLiveFrameRate";
				if (resultSet->isNull("captureLiveFrameRate"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("captureLiveFrameRate");

                field = "captureLiveWidth";
				if (resultSet->isNull("captureLiveWidth"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("captureLiveWidth");

                field = "captureLiveHeight";
				if (resultSet->isNull("captureLiveHeight"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("captureLiveHeight");

                field = "captureLiveAudioDeviceNumber";
				if (resultSet->isNull("captureLiveAudioDeviceNumber"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] =
						resultSet->getInt("captureLiveAudioDeviceNumber");

                field = "captureLiveChannelsNumber";
				if (resultSet->isNull("captureLiveChannelsNumber"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("captureLiveChannelsNumber");

                field = "satSourceSATConfKey";
				if (resultSet->isNull("satSourceSATConfKey"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("satSourceSATConfKey");

                field = "type";
				if (resultSet->isNull("type"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("type"));

                field = "description";
				if (resultSet->isNull("description"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("description"));

                field = "name";
				if (resultSet->isNull("name"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "region";
				if (resultSet->isNull("region"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("region"));

                field = "country";
				if (resultSet->isNull("country"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("country"));

                field = "imageMediaItemKey";
				if (resultSet->isNull("imageMediaItemKey"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("imageMediaItemKey");

                field = "imageUniqueName";
				if (resultSet->isNull("imageUniqueName"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("imageUniqueName"));

                field = "position";
				if (resultSet->isNull("position"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("position");

                field = "channelData";
				if (resultSet->isNull("channelData"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("channelData"));

                channelRoot.append(channelConfRoot);
            }
        }

        field = "channelConf";
        responseRoot[field] = channelRoot;

        field = "response";
        channelConfListRoot[field] = responseRoot;

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
    
    return channelConfListRoot;
}

tuple<int64_t, string, string, string, string, int, string, int,
	int, string, int, int, int, int, int, int64_t>
	MMSEngineDBFacade::getChannelConfDetails(
    int64_t workspaceKey, string label,
	bool warningIfMissing
)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        _logger->info(__FILEREF__ + "getChannelConfDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t confKey;
		string sourceType;
		string url;
		string pushProtocol;
		string pushServerName;
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
		int64_t satSourceSATConfKey = -1;
		{
			lastSQLCommand = "select confKey, sourceType, "
				"url, "
				"pushProtocol, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, satSourceSATConfKey, captureLiveVideoDeviceNumber, "
				"captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"satSourceSATConfKey "
				"from MMS_Conf_IPChannel "
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
			if (!resultSet->isNull("url"))
				url = resultSet->getString("url");
			if (!resultSet->isNull("pushProtocol"))
				pushProtocol = resultSet->getString("pushProtocol");
			if (!resultSet->isNull("pushServerName"))
				pushServerName = resultSet->getString("pushServerName");
			if (!resultSet->isNull("pushServerPort"))
				pushServerPort = resultSet->getInt("pushServerPort");
			if (!resultSet->isNull("pushUri"))
				pushUri = resultSet->getString("pushUri");
			if (!resultSet->isNull("pushListenTimeout"))
				pushListenTimeout = resultSet->getInt("pushListenTimeout");
			if (!resultSet->isNull("satSourceSATConfKey"))
				satSourceSATConfKey = resultSet->getInt64("satSourceSATConfKey");
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
			if (!resultSet->isNull("satSourceSATConfKey"))
				satSourceSATConfKey = resultSet->getInt64("satSourceSATConfKey");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, sourceType, url,
			pushProtocol, pushServerName, pushServerPort, pushUri, pushListenTimeout,
			captureLiveVideoDeviceNumber, captureLiveVideoInputFormat,
            captureLiveFrameRate, captureLiveWidth, captureLiveHeight,
			captureLiveAudioDeviceNumber, captureLiveChannelsNumber,
			satSourceSATConfKey);
    }
    catch(sql::SQLException se)
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

tuple<string, string, string> MMSEngineDBFacade::getChannelConfDetails(
    int64_t workspaceKey, int64_t confKey
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {        
        _logger->info(__FILEREF__ + "getChannelConfDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		string		url;
		string		channelName;
		string		liveURLData;
        {
            lastSQLCommand = string("select url, name, channelData from MMS_Conf_IPChannel ")
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
                string errorMessage = __FILEREF__ + "select from MMS_Conf_IPChannel failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", confKey: " + to_string(confKey)
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            url = resultSet->getString("url");
            channelName = resultSet->getString("name");
            liveURLData = resultSet->getString("channelData");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(url, channelName, liveURLData);
    }
    catch(sql::SQLException se)
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

Json::Value MMSEngineDBFacade::addSourceSATChannelConf(
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
	string country,
	string deliverySystem
)
{
	string      lastSQLCommand;
	int64_t		confKey;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Conf_SourceSATChannel(serviceId, networkId, transportStreamId, "
				"name, satellite, frequency, lnb, "
				"videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid, "
				"modulation, polarization, symbolRate, country, deliverySystem "
				") values ("
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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
				+ ", country: " + country
				+ ", deliverySystem: " + deliverySystem
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			confKey = getLastInsertId(conn);
        }

		Json::Value sourceSATChannelConfRoot;
		{
			int start = 0;
			int rows = 1;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceSATChannelConfRoot = getSourceSATChannelConfList (
				confKey, start, rows, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceSATChannelConfRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceSATChannelConfRoot[field];

			field = "sourceSATChannelConf";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceSATChannelConfRoot = responseRoot[field];

			if (sourceSATChannelConfRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceSATChannelConfRoot[0];
    }
    catch(sql::SQLException se)
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

Json::Value MMSEngineDBFacade::modifySourceSATChannelConf(
	int64_t confKey,

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
	bool countryToBeModified, string country,
	bool deliverySystemToBeModified, string deliverySystem
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
        
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

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
                string("update MMS_Conf_SourceSATChannel ") + setSQL + " "
				"where confKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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

		Json::Value sourceSATChannelConfRoot;
		{
			int start = 0;
			int rows = 1;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceSATChannelConfRoot = getSourceSATChannelConfList (
				confKey, start, rows, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceSATChannelConfRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceSATChannelConfRoot[field];

			field = "sourceSATChannelConf";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceSATChannelConfRoot = responseRoot[field];

			if (sourceSATChannelConfRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceSATChannelConfRoot[0];
    }
    catch(sql::SQLException se)
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

void MMSEngineDBFacade::removeSourceSATChannelConf(
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
                "delete from MMS_Conf_SourceSATChannel where confKey = ?";
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

Json::Value MMSEngineDBFacade::getSourceSATChannelConfList (
	int64_t confKey,
	int start, int rows,
	int64_t serviceId, string name, int64_t frequency, string lnb,
	int videoPid, string audioPids,
	string nameOrder)
{
    string      lastSQLCommand;
    Json::Value channelConfListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getSourceSATChannelConfList"
            + ", confKey: " + to_string(confKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", frequency: " + to_string(frequency)
            + ", lnb: " + lnb
            + ", serviceId: " + to_string(serviceId)
            + ", name: " + name
            + ", videoPid: " + to_string(videoPid)
            + ", audioPids: " + audioPids
            + ", nameOrder: " + nameOrder
        );
        
        conn = _connectionPool->borrow();	
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
            channelConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere;
        if (confKey != -1)
		{
			if (sqlWhere == "")
				sqlWhere += ("sc.confKey = ? ");
			else
				sqlWhere += ("and sc.confKey = ? ");
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
			lastSQLCommand = string("select count(*) from MMS_Conf_SourceSATChannel sc ")
				+ sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
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

        Json::Value channelRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (nameOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by sc.name " + nameOrder + " ";

			lastSQLCommand = 
				string("select sc.confKey, sc.serviceId, sc.networkId, sc.transportStreamId, sc.name, sc.satellite, "
					"sc.frequency, sc.lnb, sc.videoPid, sc.audioPids, "
					"sc.audioItalianPid, sc.audioEnglishPid, sc.teletextPid, "
					"sc.modulation, sc.polarization, sc.symbolRate, "
					"sc.country, sc.deliverySystem "
					"from MMS_Conf_SourceSATChannel sc ") 
				+ sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            if (confKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, confKey);
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
                Json::Value channelConfRoot;

                field = "confKey";
                channelConfRoot[field] = resultSet->getInt64("confKey");

                field = "serviceId";
				if (resultSet->isNull("serviceId"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("serviceId");

                field = "networkId";
				if (resultSet->isNull("networkId"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("networkId");

                field = "transportStreamId";
				if (resultSet->isNull("transportStreamId"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("transportStreamId");

                field = "name";
                channelConfRoot[field] = static_cast<string>(resultSet->getString("name"));

                field = "satellite";
                channelConfRoot[field] = static_cast<string>(resultSet->getString("satellite"));

                field = "frequency";
                channelConfRoot[field] = resultSet->getInt64("frequency");

                field = "lnb";
				if (resultSet->isNull("lnb"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("lnb"));

                field = "videoPid";
				if (resultSet->isNull("videoPid"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("videoPid");

                field = "audioPids";
				if (resultSet->isNull("audioPids"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("audioPids"));

                field = "audioItalianPid";
				if (resultSet->isNull("audioItalianPid"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("audioItalianPid");

                field = "audioEnglishPid";
				if (resultSet->isNull("audioEnglishPid"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("audioEnglishPid");

                field = "teletextPid";
				if (resultSet->isNull("teletextPid"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt("teletextPid");

                field = "modulation";
				if (resultSet->isNull("modulation"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("modulation"));

                field = "polarization";
				if (resultSet->isNull("polarization"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("polarization"));

                field = "symbolRate";
				if (resultSet->isNull("symbolRate"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = resultSet->getInt64("symbolRate");

				field = "country";
				if (resultSet->isNull("country"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("country"));

                field = "deliverySystem";
				if (resultSet->isNull("deliverySystem"))
					channelConfRoot[field] = Json::nullValue;
				else
					channelConfRoot[field] = static_cast<string>(resultSet->getString("deliverySystem"));

                channelRoot.append(channelConfRoot);
            }
        }

        field = "sourceSATChannelConf";
        responseRoot[field] = channelRoot;

        field = "response";
        channelConfListRoot[field] = responseRoot;

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
    
    return channelConfListRoot;
}

tuple<int64_t, int64_t, int64_t, string, int, int>
	MMSEngineDBFacade::getSourceSATChannelConfDetails(
	int64_t confKey, bool warningIfMissing
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        _logger->info(__FILEREF__ + "getSATChannelConfDetails"
            + ", confKey: " + to_string(confKey)
        );

        conn = _connectionPool->borrow();
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		int64_t serviceId;
		int64_t frequency;
		int64_t symbolRate;
		string modulation;
		int videoPid;
		int audioItalianPid;
        {
			lastSQLCommand = "select serviceId, frequency, symbolRate, "
				"modulation, videoPid, audioItalianPid "
				"from MMS_Conf_SourceSATChannel "
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

			serviceId = resultSet->getInt64("serviceId");
			frequency = resultSet->getInt64("frequency");
			symbolRate = resultSet->getInt64("symbolRate");
			modulation = resultSet->getString("modulation");
			videoPid = resultSet->getInt("videoPid");
			audioItalianPid = resultSet->getInt("audioItalianPid");
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(serviceId, frequency, symbolRate, modulation,
			videoPid, audioItalianPid);
    }
    catch(sql::SQLException se)
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

