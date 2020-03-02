
#include <random>
#include "JSONUtils.h"
#include "catralibraries/Encrypt.h"
#include "MMSEngineDBFacade.h"


int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
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
			if (workspaceKey == -1)
				lastSQLCommand = 
					"select workflowLibraryKey from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = ?";
			else
				lastSQLCommand = 
					"select workflowLibraryKey from MMS_WorkflowLibrary "
					"where workspaceKey = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (workspaceKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, label);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                workflowLibraryKey     = resultSet->getInt64("workflowLibraryKey");

                lastSQLCommand =
                    "update MMS_WorkflowLibrary set thumbnailMediaItemKey = ?, jsonWorkflow = ? "
					"where workflowLibraryKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
				if (thumbnailMediaItemKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, thumbnailMediaItemKey);
                preparedStatement->setString(queryParameterIndex++, jsonWorkflow);
                preparedStatement->setInt64(queryParameterIndex++, workflowLibraryKey);

                preparedStatement->executeUpdate();
            }
            else
            {
                lastSQLCommand = 
					"insert into MMS_WorkflowLibrary ("
						"workflowLibraryKey, workspaceKey, label, thumbnailMediaItemKey, jsonWorkflow) values ("
						"NULL, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
				if (workspaceKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, label);
				if (thumbnailMediaItemKey == -1)
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
				else
					preparedStatement->setInt64(queryParameterIndex++, thumbnailMediaItemKey);

                preparedStatement->setString(queryParameterIndex++, jsonWorkflow);

                preparedStatement->executeUpdate();

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
    int64_t workspaceKey, int64_t workflowLibraryKey)
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
			if (workspaceKey == -1)
				lastSQLCommand = 
					"delete from MMS_WorkflowLibrary where workflowLibraryKey = ? "
					"and workspaceKey is null";
			else
				lastSQLCommand = 
					"delete from MMS_WorkflowLibrary where workflowLibraryKey = ? "
					"and workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workflowLibraryKey);
			if (workspaceKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated == 0)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", workflowLibraryKey: " + to_string(workflowLibraryKey)
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
                string ("select workspaceKey, workflowLibraryKey, label, "
					"thumbnailMediaItemKey, jsonWorkflow from MMS_WorkflowLibrary ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value workflowLibraryRoot;

                field = "global";
				if (resultSet->isNull("workspaceKey"))
					workflowLibraryRoot[field] = true;
				else
					workflowLibraryRoot[field] = false;

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
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

