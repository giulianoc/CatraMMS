
#include "MMSEngineDBFacade.h"

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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
					+ ", label: " + label
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);

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
        DeliveryTechnology deliveryTechnology,
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
            deliveryTechnology,
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
        DeliveryTechnology deliveryTechnology,
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                encodingProfileKey     = resultSet->getInt64("encodingProfileKey");
                
                lastSQLCommand = 
                    "update MMS_EncodingProfile set deliveryTechnology = ?, jsonProfile = ? where encodingProfileKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++,
						MMSEngineDBFacade::toString(deliveryTechnology));
                preparedStatement->setString(queryParameterIndex++, jsonProfile);
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", deliveryTechnology: " + MMSEngineDBFacade::toString(deliveryTechnology)
					+ ", jsonProfile: " + jsonProfile
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }
            else
            {
                lastSQLCommand = 
                    "insert into MMS_EncodingProfile ("
                    "encodingProfileKey, workspaceKey, label, contentType, deliveryTechnology, jsonProfile) values ("
                    "NULL, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                    preparedStatement->setString(queryParameterIndex++, label);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                preparedStatement->setString(queryParameterIndex++,
						MMSEngineDBFacade::toString(deliveryTechnology));
                preparedStatement->setString(queryParameterIndex++, jsonProfile);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
					+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
					+ ", deliveryTechnology: " + MMSEngineDBFacade::toString(deliveryTechnology)
					+ ", jsonProfile: " + jsonProfile
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);

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
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (!resultSet->next())
                {
                    {
                        lastSQLCommand = 
                            "select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
							+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
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
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (!resultSet->next())
                {
                    {
                        lastSQLCommand = 
                            "select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
							+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
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
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                        "select ep.encodingProfileKey, ep.contentType, ep.label, ep.deliveryTechnology, ep.jsonProfile "
                        "from MMS_EncodingProfilesSetMapping epsm, MMS_EncodingProfile ep "
                        "where epsm.encodingProfileKey = ep.encodingProfileKey and "
                        "epsm.encodingProfilesSetKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfile (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfile->setInt64(queryParameterIndex++, localEncodingProfilesSetKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> resultSetProfile (preparedStatementProfile->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", localEncodingProfilesSetKey: " + to_string(localEncodingProfilesSetKey)
						+ ", resultSetProfile->rowsCount: " + to_string(resultSetProfile->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    while (resultSetProfile->next())
                    {
                        Json::Value encodingProfileRoot;
                        
                        field = "encodingProfileKey";
                        encodingProfileRoot[field] = resultSetProfile->getInt64("encodingProfileKey");
                
                        field = "label";
                        encodingProfileRoot[field] = static_cast<string>(resultSetProfile->getString("label"));

                        field = "contentType";
                        encodingProfileRoot[field] = static_cast<string>(resultSetProfile->getString("contentType"));
                        
                        field = "deliveryTechnology";
                        encodingProfileRoot[field] = static_cast<string>(
								resultSetProfile->getString("deliveryTechnology"));

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
        bool contentTypePresent, ContentType contentType,
		string label
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
            + ", label: " + label
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
            
            if (label != "")
            {
                field = "label";
                requestParametersRoot[field] = label;
            }
            
            field = "requestParameters";
            contentListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where (workspaceKey = ? or workspaceKey is null) ");
        if (encodingProfileKey != -1)
            sqlWhere += ("and encodingProfileKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and contentType = ? ");
        if (label != "")
            sqlWhere += ("and label like ? ");
        
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
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", label: " + label
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

        Json::Value encodingProfilesRoot(Json::arrayValue);
        {                    
            lastSQLCommand = 
                string ("select workspaceKey, encodingProfileKey, label, contentType, deliveryTechnology, jsonProfile from MMS_EncodingProfile ") 
                + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodingProfileKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", label: " + label
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value encodingProfileRoot;

                field = "global";
				if (resultSet->isNull("workspaceKey"))
					encodingProfileRoot[field] = true;
				else
					encodingProfileRoot[field] = false;

                field = "encodingProfileKey";
                encodingProfileRoot[field] = resultSet->getInt64("encodingProfileKey");

                field = "label";
                encodingProfileRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "contentType";
                encodingProfileRoot[field] = static_cast<string>(resultSet->getString("contentType"));

                field = "deliveryTechnology";
                encodingProfileRoot[field] = static_cast<string>(
						resultSet->getString("deliveryTechnology"));

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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

int64_t MMSEngineDBFacade::getEncodingProfileKeyByLabel (
	int64_t workspaceKey,
    MMSEngineDBFacade::ContentType contentType,
    string encodingProfileLabel,
	bool contentTypeToBeUsed
)
{

	int64_t		encodingProfileKey;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			if (contentTypeToBeUsed)
				lastSQLCommand = 
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = ? or workspaceKey is null) and contentType = ? and label = ?";
			else
				lastSQLCommand = 
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = ? or workspaceKey is null) and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (contentTypeToBeUsed)
				preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setString(queryParameterIndex++, encodingProfileLabel);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ (contentTypeToBeUsed ? (string(", contentType: ") + MMSEngineDBFacade::toString(contentType)) : "")
				+ ", encodingProfileLabel: " + encodingProfileLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                encodingProfileKey = resultSet->getInt64("encodingProfileKey");
				if (!contentTypeToBeUsed && resultSet->next())
				{
					string errorMessage = __FILEREF__ + "contentType has to be used because the label is not unique"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                        + ", contentTypeToBeUsed: " + to_string(contentTypeToBeUsed)
                        + ", encodingProfileLabel: " + encodingProfileLabel
                        + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
            }
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey not found "
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
					+ ", contentTypeToBeUsed: " + to_string(contentTypeToBeUsed)
					+ ", encodingProfileLabel: " + encodingProfileLabel
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

	return encodingProfileKey;
}

tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
	MMSEngineDBFacade::getEncodingProfileDetailsByKey(
    int64_t workspaceKey, int64_t encodingProfileKey
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

		string label;
		MMSEngineDBFacade::ContentType contentType;
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		string jsonProfile;
        {
            lastSQLCommand = 
                "select label, contentType, deliveryTechnology, jsonProfile from MMS_EncodingProfile where "
				"(workspaceKey = ? or workspaceKey is null) and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                label = resultSet->getString("label");
				contentType = MMSEngineDBFacade::toContentType(
					resultSet->getString("contentType"));
				deliveryTechnology = MMSEngineDBFacade::toDeliveryTechnology(
					resultSet->getString("deliveryTechnology"));
                jsonProfile = resultSet->getString("jsonProfile");
            }
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey not found "
                        + ", workspaceKey: " + to_string(workspaceKey)
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

		return make_tuple(label, contentType, deliveryTechnology, jsonProfile);
    }
    catch(sql::SQLException se)
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

