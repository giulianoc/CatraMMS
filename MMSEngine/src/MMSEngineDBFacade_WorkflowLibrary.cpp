
#include <random>
#include "JSONUtils.h"
#include "catralibraries/Encrypt.h"
#include "MMSEngineDBFacade.h"


int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	int64_t userKey,
	int64_t workspaceKey,
	string label,
	int64_t thumbnailMediaItemKey,
	string jsonWorkflow
)
{
	int64_t		workflowLibraryKey;
	string		lastSQLCommand;

	shared_ptr<MySQLConnection> conn = nullptr;

	try
	{
		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

		workflowLibraryKey = addUpdateWorkflowAsLibrary(
			conn,
			userKey,
			workspaceKey,
			label,
			thumbnailMediaItemKey,
			jsonWorkflow);

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
    
    return workflowLibraryKey;
}

int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	shared_ptr<MySQLConnection> conn,
	int64_t userKey,
	int64_t workspaceKey,
	string label,
	int64_t thumbnailMediaItemKey,
	string jsonWorkflow
)
{
	int64_t			workflowLibraryKey;

	string			lastSQLCommand;

    try
    {
        {
			// in case workspaceKey == -1 (MMS library), only ADMIN can add/update it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			if (workspaceKey == -1)
				lastSQLCommand = 
					"select workflowLibraryKey from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = ?";
			else
				lastSQLCommand = 
					"select workflowLibraryKey, creatorUserKey from MMS_WorkflowLibrary "
					"where workspaceKey = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (workspaceKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				// two options:
				// 1. MMS library: it is an admin user (check already done, update can be done
				// 2. NO MMS Library: only creatorUserKey can do the update
				//		We should add a rights like 'Allow Update Workflow Library even if not the creator'
				//		but this is not present yet
                workflowLibraryKey     = resultSet->getInt64("workflowLibraryKey");
				if (workspaceKey != -1)
				{
					int64_t creatorUserKey     = resultSet->getInt64("creatorUserKey");
					if (creatorUserKey != userKey)
					{
						string errorMessage = string("User does not have the permission to add/update MMS WorkflowAsLibrary")
							+ ", creatorUserKey: " + to_string(creatorUserKey)
							+ ", userKey: " + to_string(userKey)
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

                lastSQLCommand =
                    "update MMS_WorkflowLibrary set lastUpdateUserKey = ?, thumbnailMediaItemKey = ?, jsonWorkflow = ? "
					"where workflowLibraryKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
				if (userKey == -1)	// if (workspaceKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, userKey);
				if (thumbnailMediaItemKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, thumbnailMediaItemKey);
                preparedStatement->setString(queryParameterIndex++, jsonWorkflow);
                preparedStatement->setInt64(queryParameterIndex++, workflowLibraryKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", userKey: " + to_string(userKey)
					+ ", thumbnailMediaItemKey: " + to_string(thumbnailMediaItemKey)
					// + ", jsonWorkflow: " + jsonWorkflow
					+ ", workflowLibraryKey: " + to_string(workflowLibraryKey)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }
            else
            {
                lastSQLCommand = 
					"insert into MMS_WorkflowLibrary ("
						"workflowLibraryKey, workspaceKey, creatorUserKey, lastUpdateUserKey, "
						"label, thumbnailMediaItemKey, jsonWorkflow) values ("
						"NULL, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
				if (workspaceKey == -1)
				{
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
					// creatorUserKey
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				}
				else
				{
					preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
					// creatorUserKey
					preparedStatement->setInt64(queryParameterIndex++, userKey);
				}
				// lastUpdateUserKey
				preparedStatement->setInt64(queryParameterIndex++, userKey);
				preparedStatement->setString(queryParameterIndex++, label);
				if (thumbnailMediaItemKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, thumbnailMediaItemKey);

                preparedStatement->setString(queryParameterIndex++, jsonWorkflow);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", userKey: " + to_string(userKey)
					+ ", userKey: " + to_string(userKey)
					+ ", label: " + label
					+ ", thumbnailMediaItemKey: " + to_string(thumbnailMediaItemKey)
					+ ", jsonWorkflow: " + jsonWorkflow
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);

                workflowLibraryKey = getLastInsertId(conn);
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
    
	return workflowLibraryKey;
}

void MMSEngineDBFacade::removeWorkflowAsLibrary(
    int64_t userKey, int64_t workspaceKey, int64_t workflowLibraryKey)
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
			// in case workspaceKey == -1 (MMS library), only ADMIN can remove it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			if (workspaceKey == -1)
				lastSQLCommand = 
					"delete from MMS_WorkflowLibrary where workflowLibraryKey = ? "
					"and workspaceKey is null";
			else
				lastSQLCommand = 
					"delete from MMS_WorkflowLibrary where workflowLibraryKey = ? "
					"and creatorUserKey = ? and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workflowLibraryKey);
			if (workspaceKey != -1)
			{
				preparedStatement->setInt64(queryParameterIndex++, userKey);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			}

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workflowLibraryKey: " + to_string(workflowLibraryKey)
				+ ", userKey: " + to_string(userKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated == 0)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workflowLibraryKey: " + to_string(workflowLibraryKey)
                        + ", userKey: " + to_string(userKey)
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

Json::Value MMSEngineDBFacade::getWorkflowsAsLibraryList (
	int64_t workspaceKey
)
{
    string      lastSQLCommand;
    Json::Value workflowsLibraryRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getWorkflowsAsLibraryList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "requestParameters";
            workflowsLibraryRoot[field] = requestParametersRoot;
        }

        string sqlWhere = string ("where (workspaceKey = ? or workspaceKey is null) ");

        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_WorkflowLibrary ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
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

        Json::Value workflowsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                string ("select workspaceKey, workflowLibraryKey, creatorUserKey, label, "
					"thumbnailMediaItemKey, jsonWorkflow from MMS_WorkflowLibrary ") 
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
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value workflowLibraryRoot;

                field = "global";
				if (resultSet->isNull("workspaceKey"))
					workflowLibraryRoot[field] = true;
				else
				{
					workflowLibraryRoot[field] = false;

					field = "creatorUserKey";
					workflowLibraryRoot[field] = resultSet->getInt64("creatorUserKey");
				}

                field = "workflowLibraryKey";
                workflowLibraryRoot[field] = resultSet->getInt64("workflowLibraryKey");

                field = "label";
                workflowLibraryRoot[field] = static_cast<string>(resultSet->getString("label"));

				field = "thumbnailMediaItemKey";
				if (resultSet->isNull("thumbnailMediaItemKey"))
					workflowLibraryRoot[field] = Json::nullValue;
				else
					workflowLibraryRoot[field] = resultSet->getInt64("thumbnailMediaItemKey");

                {
                    string jsonWorkflow = resultSet->getString("jsonWorkflow");

                    Json::Value workflowRoot;
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(jsonWorkflow.c_str(),
                                jsonWorkflow.c_str() + jsonWorkflow.size(), 
                                &workflowRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = string("Json metadata failed during the parsing")
                                    + ", errors: " + errors
                                    + ", json data: " + jsonWorkflow
                                    ;
                            _logger->error(__FILEREF__ + errorMessage);

                            continue;
                        }
                    }
                    catch(exception e)
                    {
                        string errorMessage = string("Json metadata failed during the parsing"
                                ", json data: " + jsonWorkflow
                                );
                        _logger->error(__FILEREF__ + errorMessage);

                        continue;
                    }

                    field = "Variables";
					if (!JSONUtils::isMetadataPresent(workflowRoot, field))
						workflowLibraryRoot["variables"] = Json::nullValue;
					else
						workflowLibraryRoot["variables"] = workflowRoot[field];
                }

                workflowsRoot.append(workflowLibraryRoot);
            }
        }

        field = "workflowsLibrary";
        responseRoot[field] = workflowsRoot;

        field = "response";
        workflowsLibraryRoot[field] = responseRoot;

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
    
    return workflowsLibraryRoot;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent (
	int64_t workspaceKey,
	int64_t workflowLibraryKey
)
{
    string      lastSQLCommand;
    string		workflowLibraryContent;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getWorkflowAsLibraryContent"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", workflowLibraryKey: " + to_string(workflowLibraryKey)
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );


        {                    
            lastSQLCommand = "select jsonWorkflow from MMS_WorkflowLibrary "
					"where (workspaceKey = ? or workspaceKey is null) "
					"and workflowLibraryKey = ?"
					;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, workflowLibraryKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", workflowLibraryKey: " + to_string(workflowLibraryKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage = __FILEREF__ + "WorkflowLibrary was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", workflowLibraryKey: " + to_string(workflowLibraryKey)
				;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			workflowLibraryContent = resultSet->getString("jsonWorkflow");
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
    
    return workflowLibraryContent;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent (
	int64_t workspaceKey,
	string label
)
{
    string      lastSQLCommand;
    string		workflowLibraryContent;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getWorkflowAsLibraryContent"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );


        {
			if (workspaceKey == -1)
				lastSQLCommand = "select jsonWorkflow from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = ?"
					;
			else
				lastSQLCommand = "select jsonWorkflow from MMS_WorkflowLibrary "
					"where workspaceKey = ? and label = ?"
					;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (workspaceKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage = __FILEREF__ + "WorkflowLibrary was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			workflowLibraryContent = resultSet->getString("jsonWorkflow");
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
    
    return workflowLibraryContent;
}

