
#include "MMSEngineDBFacade.h"
#include "JSONUtils.h"
#include "StringUtils.h"


void MMSEngineDBFacade::getExpiredMediaItemKeysCheckingDependencies(
        string processorMMS,
        vector<tuple<shared_ptr<Workspace>,int64_t, int64_t>>& mediaItemKeyOrPhysicalPathKeyToBeRemoved,
        int maxEntriesNumber)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    // bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		// 2021-09-23: I removed TRANSACTION and FOR UPDATE because I saw we may have deadlock when a MediaItem is added

        // autoCommit = false;
        // // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        // {
        //     lastSQLCommand = 
        //         "START TRANSACTION";

        //     shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
        //     statement->execute(lastSQLCommand);
        // }

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies (MediaItemKeys expired)"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);

		// 1. MediaItemKeys expired
        int start = 0;
        bool noMoreRowsReturned = false;
        while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber &&
                !noMoreRowsReturned)
        {
            lastSQLCommand = 
				"select workspaceKey, mediaItemKey, ingestionJobKey, retentionInMinutes, title, "
				"DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem where "
				"DATE_ADD(ingestionDate, INTERVAL retentionInMinutes MINUTE) < NOW() "
				"and processorMMSForRetention is null "
				"limit ? offset ?";	// for update"; see comment marked as 2021-09-23
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, maxEntriesNumber);
            preparedStatement->setInt(queryParameterIndex++, start);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            noMoreRowsReturned = true;
            start += maxEntriesNumber;
            while (resultSet->next())
            {
                noMoreRowsReturned = false;
                
                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
                int64_t workspaceKey = resultSet->getInt64("workspaceKey");
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                int64_t retentionInMinutes = resultSet->getInt64("retentionInMinutes");
                string ingestionDate = resultSet->getString("ingestionDate");
                string title = resultSet->getString("title");
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(conn, ingestionJobKey)
						> 0)
					ingestionDependingOnMediaItemKey = true;

                if (!ingestionDependingOnMediaItemKey)
                {
                    {
                        lastSQLCommand = 
                            "update MMS_MediaItem set processorMMSForRetention = ? "
							"where mediaItemKey = ? and processorMMSForRetention is null";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                        preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, mediaItemKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", processorMMS: " + processorMMS
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (rowsUpdated != 1)
                        {
							// may be another processor doing the same activity updates it
							// Really it should never happen because of the 'for update'
							// 2021-09-23: we do not have for update anymore

							continue;
							/*
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", processorMMS: " + processorMMS
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
							*/
                        }
                    }

                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    tuple<shared_ptr<Workspace>,int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey =
                            make_tuple(workspace, mediaItemKey, -1);

                    mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
                }
                else
                {
                    _logger->info(__FILEREF__ + "Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
                        + "ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", title: " + title
                        + ", ingestionDate: " + ingestionDate
                        + ", retentionInMinutes: " + to_string(retentionInMinutes)
                    );
                }
            }
        }

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies (PhysicalPathKeys expired)"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);

		// 1. PhysicalPathKeys expired
        start = 0;
        noMoreRowsReturned = false;
        while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber &&
                !noMoreRowsReturned)
        {
            lastSQLCommand = 
				"select mi.workspaceKey, mi.mediaItemKey, p.physicalPathKey, mi.ingestionJobKey, "
				"p.retentionInMinutes, mi.title, "
				"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p where "
				"mi.mediaItemKey = p.mediaItemKey "
				"and p.retentionInMinutes is not null "
				// PhysicalPathKey expired
				"and DATE_ADD(mi.ingestionDate, INTERVAL p.retentionInMinutes MINUTE) < NOW() "
				// MediaItemKey not expired
				"and DATE_ADD(mi.ingestionDate, INTERVAL mi.retentionInMinutes MINUTE) > NOW() "
				"and processorMMSForRetention is null "
				"limit ? offset ?";	// for update"; see comment marked as 2021-09-23
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, maxEntriesNumber);
            preparedStatement->setInt(queryParameterIndex++, start);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            noMoreRowsReturned = true;
            start += maxEntriesNumber;
            while (resultSet->next())
            {
                noMoreRowsReturned = false;
                
                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
                int64_t workspaceKey = resultSet->getInt64("workspaceKey");
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                int64_t physicalPathKey = resultSet->getInt64("physicalPathKey");
                int64_t physicalPathKeyRetentionInMinutes = resultSet->getInt64("retentionInMinutes");
                string ingestionDate = resultSet->getString("ingestionDate");
                string title = resultSet->getString("title");
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(conn, ingestionJobKey)
						> 0)
					ingestionDependingOnMediaItemKey = true;

                if (!ingestionDependingOnMediaItemKey)
                {
                    {
                        lastSQLCommand = 
                            "update MMS_MediaItem set processorMMSForRetention = ? "
							"where mediaItemKey = ? and processorMMSForRetention is null";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                        preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, mediaItemKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", processorMMS: " + processorMMS
							+ ", mediaItemKey: " + to_string(mediaItemKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (rowsUpdated != 1)
                        {
							// may be another processor doing the same activity updates it
							// Really it should never happen because of the 'for update'
							// 2021-09-23: we do not have for update anymore

							continue;
							/*
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", processorMMS: " + processorMMS
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
							*/
                        }
                    }

                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    tuple<shared_ptr<Workspace>,int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey =
                            make_tuple(workspace, mediaItemKey, physicalPathKey);

                    mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
                }
                else
                {
                    _logger->info(__FILEREF__ + "Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
                        + "ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", title: " + title
                        + ", ingestionDate: " + ingestionDate
                        + ", physicalPathKeyRetentionInMinutes: " + to_string(physicalPathKeyRetentionInMinutes)
                    );
                }
            }
        }

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);
        
        // // conn->_sqlConnection->commit(); OR execute COMMIT
        // {
        //     lastSQLCommand = 
        //         "COMMIT";

        //     shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
        //     statement->execute(lastSQLCommand);
        // }
        // autoCommit = true;
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
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
                // // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                // if (!autoCommit)
                // {
                //     shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                //     statement->execute("ROLLBACK");
                // }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(runtime_error& e)
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
                // // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                // if (!autoCommit)
                // {
                //     shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                //     statement->execute("ROLLBACK");
                // }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                // if (!autoCommit)
                // {
                //     shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                //     statement->execute("ROLLBACK");
                // }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
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
            catch(exception& e)
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

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
	int64_t ingestionJobKey, bool fromMaster
)
{
    int			dependenciesNumber;
    string      lastSQLCommand;
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

		dependenciesNumber = getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
			conn, ingestionJobKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

    return dependenciesNumber;
}

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
	shared_ptr<MySQLConnection> conn,
	int64_t ingestionJobKey
)
{
    int			dependenciesNumber;
    string      lastSQLCommand;

    try
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSetDependency (preparedStatementDependency->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSetDependency->rowsCount: " + to_string(resultSetDependency->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSetDependency->next())
			{
				dependenciesNumber	= resultSetDependency->getInt(1);
			}
			else
			{
				string errorMessage ("select count(*) failed");

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );
        
        throw e;
    }
    
    return dependenciesNumber;
}

Json::Value MMSEngineDBFacade::updateMediaItem (
	int64_t workspaceKey,
	int64_t mediaItemKey,
	bool titleModified, string newTitle,
	bool userDataModified, string newUserData,
	bool retentionInMinutesModified, int64_t newRetentionInMinutes,
	bool tagsModified, Json::Value tagsRoot,
	bool uniqueNameModified, string newUniqueName,
	bool admin
	)
{
    Json::Value mediaItemRoot;
    string		lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		if (titleModified || userDataModified || retentionInMinutesModified)
        {
			string setSQL;

			if (titleModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "title = ?";
			}

			if (userDataModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "userData = ?";
			}

			if (retentionInMinutesModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "retentionInMinutes = ?";
			}
			setSQL = "set " + setSQL + " ";

			lastSQLCommand = 
				string("update MMS_MediaItem ") + setSQL
				+ "where mediaItemKey = ? "
				// 2021-02: in case the user is not the owner and it is a shared workspace
				//		the workspacekey will not match
				// 2021-03: I think the above comment is wrong, the user, in that case,
				//		will use an APIKey of the correct workspace, even if this is shared.
				//		So let's add again the below sql condition
				+ "and workspaceKey = ? ";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (titleModified)
				preparedStatement->setString(queryParameterIndex++, newTitle);
			if (userDataModified)
			{
				if (newUserData == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, newUserData);
			}
			if (retentionInMinutesModified)
				preparedStatement->setInt64(queryParameterIndex++, newRetentionInMinutes);
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newTitle: " + newTitle
				+ ", newUserData: " + newUserData
				+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			/*
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
						+ ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", newTitle: " + newTitle
						+ ", newUserData: " + newUserData
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
			*/
		}

		if (tagsModified)
		{
			// we should have a transaction here
			removeTags(conn, mediaItemKey);
			addTags(conn, mediaItemKey, tagsRoot);
		}

		if (uniqueNameModified)
        {
			bool allowUniqueNameOverride = false;

			manageExternalUniqueName(
				conn, workspaceKey,
				mediaItemKey,

				allowUniqueNameOverride,
				newUniqueName
			);
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {        
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
    
	string uniqueName;
	int64_t physicalPathKey = -1;
	vector<int64_t> otherMediaItemsKey;
	int start = 0;
	int rows = 1;
	bool contentTypePresent = false;
	ContentType contentType;
	// bool startAndEndIngestionDatePresent = false;
	string startIngestionDate;
	string endIngestionDate;
	string title;
	int liveRecordingChunk = -1;
	int64_t recordingCode = -1;
	int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
	int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
	string jsonCondition;
	vector<string> tagsIn;
	vector<string> tagsNotIn;
	string orderBy;
	string jsonOrderBy;
	Json::Value responseFields = Json::nullValue;

	Json::Value mediaItemsListRoot = getMediaItemsList (
		workspaceKey, mediaItemKey, uniqueName, physicalPathKey,
		otherMediaItemsKey,
        start, rows,
        contentTypePresent, contentType,
        // startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate,
        title, liveRecordingChunk,
		recordingCode,
		utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		jsonCondition,
		tagsIn, tagsNotIn,
        orderBy,
		jsonOrderBy,
		responseFields,
		admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true);

    return mediaItemsListRoot;
}

Json::Value MMSEngineDBFacade::updatePhysicalPath (
	int64_t workspaceKey,
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	int64_t newRetentionInMinutes,
	bool admin
	)
{
    Json::Value mediaItemRoot;
    string		lastSQLCommand;

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
                "update MMS_PhysicalPath set retentionInMinutes = ? "
                "where physicalPathKey = ? and mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (newRetentionInMinutes == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, newRetentionInMinutes);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			/*
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
			*/
        }


        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {        
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
    
	string uniqueName;
	int64_t localPhysicalPathKey = -1;
	vector<int64_t> otherMediaItemsKey;
	int start = 0;
	int rows = 1;
	bool contentTypePresent = false;
	ContentType contentType;
	// bool startAndEndIngestionDatePresent = false;
	string startIngestionDate;
	string endIngestionDate;
	string title;
	int liveRecordingChunk = -1;
	int64_t recordingCode = -1;
	int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
	int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
	string jsonCondition;
	vector<string> tagsIn;
	vector<string> tagsNotIn;
	string orderBy;
	string jsonOrderBy;
	Json::Value responseFields = Json::nullValue;

	Json::Value mediaItemsListRoot = getMediaItemsList (
		workspaceKey, mediaItemKey, uniqueName, localPhysicalPathKey,
		otherMediaItemsKey,
        start, rows,
        contentTypePresent, contentType,
        // startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate,
        title, liveRecordingChunk,
		recordingCode,
		utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		jsonCondition,
		tagsIn, tagsNotIn,
        orderBy,
		jsonOrderBy,
        responseFields,
		admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true);

    return mediaItemsListRoot;
}

Json::Value MMSEngineDBFacade::getMediaItemsList (
        int64_t workspaceKey, int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string orderBy,			// i.e.: "", mi.ingestionDate desc, mi.title asc
		string jsonOrderBy,		// i.e.: "", JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc
        Json::Value responseFields,
		bool admin,
		bool fromMaster
)
{
    string      lastSQLCommand;
	string		temporaryTableName;
    Json::Value mediaItemsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getMediaItemsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", uniqueName: " + uniqueName
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
            // + ", startAndEndIngestionDatePresent: " + to_string(startAndEndIngestionDatePresent)
            + ", startIngestionDate: " + startIngestionDate
            + ", endIngestionDate: " + endIngestionDate
            + ", title: " + title
            + ", tagsIn.size(): " + to_string(tagsIn.size())
            + ", tagsNotIn.size(): " + to_string(tagsNotIn.size())
            + ", otherMediaItemsKey.size(): " + to_string(otherMediaItemsKey.size())
            + ", liveRecordingChunk: " + to_string(liveRecordingChunk)
            + ", recordingCode: " + to_string(recordingCode)
            + ", jsonCondition: " + jsonCondition
            + ", orderBy: " + orderBy
            + ", jsonOrderBy: " + jsonOrderBy
        );
        
        conn = connectionPool->borrow();	
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
            
            if (uniqueName != "")
            {
                field = "uniqueName";
                requestParametersRoot[field] = uniqueName;
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
            
            if (startIngestionDate != "")
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;
            }
            if (endIngestionDate != "")
            {
                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (title != "")
            {
                field = "title";
                requestParametersRoot[field] = title;
            }

            if (tagsIn.size() > 0)
			{
				Json::Value tagsRoot(Json::arrayValue);

				for (int tagIndex = 0; tagIndex < tagsIn.size(); tagIndex++)
					tagsRoot.append(tagsIn[tagIndex]);

                field = "tagsIn";
                requestParametersRoot[field] = tagsRoot;
			}

            if (tagsNotIn.size() > 0)
			{
				Json::Value tagsRoot(Json::arrayValue);

				for (int tagIndex = 0; tagIndex < tagsNotIn.size(); tagIndex++)
					tagsRoot.append(tagsNotIn[tagIndex]);

                field = "tagsNotIn";
                requestParametersRoot[field] = tagsRoot;
			}

            if (otherMediaItemsKey.size() > 0)
			{
				Json::Value otherMediaItemsKeyRoot(Json::arrayValue);

				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					otherMediaItemsKeyRoot.append(otherMediaItemsKey[mediaItemIndex]);

                field = "otherMediaItemsKey";
                requestParametersRoot[field] = otherMediaItemsKeyRoot;
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

            if (recordingCode != -1)
            {
                field = "recordingCode";
                requestParametersRoot[field] = recordingCode;
            }

            if (orderBy != "")
            {
                field = "orderBy";
                requestParametersRoot[field] = orderBy;
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
        if (mediaItemKey == -1)
        {
			if (physicalPathKey != -1)
			{
				lastSQLCommand = 
					string("select mediaItemKey from MMS_PhysicalPath where physicalPathKey = ?");

				shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					newMediaItemKey = resultSet->getInt64("mediaItemKey");
				}
				else
				{
					string errorMessage (__FILEREF__ + "getMediaItemsList: requested physicalPathKey does not exist"
						+ ", physicalPathKey: " + to_string(physicalPathKey)
						);
					_logger->error(errorMessage);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0;	// let's force a MIK that does not exist
				}
			}
			else if (uniqueName != "")
			{
				lastSQLCommand = "select mediaItemKey from MMS_ExternalUniqueName "
							"where workspaceKey = ? and uniqueName = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, uniqueName);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", uniqueName: " + uniqueName
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					newMediaItemKey = resultSet->getInt64("mediaItemKey");
				}
				else
				{
					string errorMessage (__FILEREF__ + "getMediaItemsList: requested uniqueName does not exist"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", uniqueName: " + uniqueName
						);
					_logger->error(errorMessage);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0;	// let's force a MIK that does not exist
				}
			}
        }

		pair<shared_ptr<sql::ResultSet>, int64_t>	resultSetAndNumFound;
		if (tagsIn.size() > 0 || tagsNotIn.size() > 0)
		{
			{
				temporaryTableName = "MMS_MediaItemFilter";

				_logger->info(__FILEREF__ + "getMediaItemsList temporary table name"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", temporaryTableName: " + temporaryTableName
				);
			}
        
			chrono::system_clock::time_point startTags = chrono::system_clock::now();
			// getMediaItemsList_withTagsCheck creates a temporary table
			resultSetAndNumFound = getMediaItemsList_withTagsCheck (
				conn, workspaceKey, temporaryTableName, newMediaItemKey, otherMediaItemsKey,
				start, rows,
				contentTypePresent, contentType,
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate,
				title, liveRecordingChunk,
				recordingCode,
				utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
				jsonCondition,
				tagsIn, tagsNotIn,
				orderBy,
				jsonOrderBy,
				admin
			);
			_logger->info(__FILEREF__ + "@MMS statistics@ - with tags"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", start: " + to_string(start)
				+ ", rows: " + to_string(rows)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now()
					- startTags).count()) + "@"
			);
		}
		else
		{
			chrono::system_clock::time_point startTags = chrono::system_clock::now();
			resultSetAndNumFound = getMediaItemsList_withoutTagsCheck (
					conn, workspaceKey, newMediaItemKey, otherMediaItemsKey, start, rows,
					contentTypePresent, contentType,
					// startAndEndIngestionDatePresent,
					startIngestionDate, endIngestionDate,
					title, liveRecordingChunk,
					recordingCode,
					utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
					jsonCondition,
					orderBy,
					jsonOrderBy,
					admin
				);
			_logger->info(__FILEREF__ + "@MMS statistics@ - without tags"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", start: " + to_string(start)
				+ ", rows: " + to_string(rows)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<
					chrono::milliseconds>(chrono::system_clock::now()
					- startTags).count()) + "@"
			);
		}

		shared_ptr<sql::ResultSet> resultSet;
		int64_t	numFound;

		tie(resultSet, numFound) = resultSetAndNumFound;
        
        Json::Value responseRoot;
        {
			field = "numFound";
			responseRoot[field] = numFound;
        }

        Json::Value mediaItemsRoot(Json::arrayValue);
        {
			chrono::system_clock::time_point startSqlResultSet = chrono::system_clock::now();
            while (resultSet->next())
            {
                Json::Value mediaItemRoot;

                int64_t localMediaItemKey = resultSet->getInt64("mediaItemKey");

                field = "mediaItemKey";
                mediaItemRoot[field] = localMediaItemKey;

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "title"))
				{
					string localTitle = static_cast<string>(resultSet->getString("title"));

					// a printf is used to pring into the output, so % has to be changed to %%
					for (int titleIndex = localTitle.length() - 1; titleIndex >= 0; titleIndex--)
					{
						if (localTitle[titleIndex] == '%')
							localTitle.replace(titleIndex, 1, "%%");
					}

					field = "title";
					mediaItemRoot[field] = localTitle;
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "deliveryFileName"))
				{
					field = "deliveryFileName";
					if (resultSet->isNull("deliveryFileName"))
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = static_cast<string>(
							resultSet->getString("deliveryFileName"));
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "ingester"))
				{
					field = "ingester";
					if (resultSet->isNull("ingester"))
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = static_cast<string>(
							resultSet->getString("ingester"));
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "userData"))
				{
					field = "userData";
					if (resultSet->isNull("userData"))
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = static_cast<string>(
							resultSet->getString("userData"));
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "ingestionDate"))
				{
					field = "ingestionDate";
					mediaItemRoot[field] = static_cast<string>(
						resultSet->getString("ingestionDate"));
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "startPublishing"))
				{
					field = "startPublishing";
					mediaItemRoot[field] = static_cast<string>(
						resultSet->getString("startPublishing"));
				}
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "endPublishing"))
				{
					field = "endPublishing";
					mediaItemRoot[field] = static_cast<string>(
						resultSet->getString("endPublishing"));
				}

				ContentType contentType = MMSEngineDBFacade::toContentType(
					resultSet->getString("contentType"));
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "contentType"))
				{
					field = "contentType";
					mediaItemRoot[field] = static_cast<string>(
						resultSet->getString("contentType"));
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "retentionInMinutes"))
				{
					field = "retentionInMinutes";
					mediaItemRoot[field] = resultSet->getInt64("retentionInMinutes");
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "providerName"))
				{
					int64_t contentProviderKey = resultSet->getInt64("contentProviderKey");
                
					{
						lastSQLCommand = 
							"select name from MMS_ContentProvider where contentProviderKey = ?";

						shared_ptr<sql::PreparedStatement> preparedStatementProvider (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementProvider->setInt64(queryParameterIndex++,
							contentProviderKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						shared_ptr<sql::ResultSet> resultSetProviders (
							preparedStatementProvider->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", contentProviderKey: " + to_string(contentProviderKey)
							+ ", resultSetProviders->rowsCount: "
								+ to_string(resultSetProviders->rowsCount())
							+ ", elapsed (millisecs): @" + to_string(
								chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						if (resultSetProviders->next())
						{
							field = "providerName";
							mediaItemRoot[field] = static_cast<string>(
								resultSetProviders->getString("name"));
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
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "uniqueName"))
				{
					{
						lastSQLCommand = 
							"select uniqueName from MMS_ExternalUniqueName "
							"where workspaceKey = ? and mediaItemKey = ?";

						shared_ptr<sql::PreparedStatement> preparedStatementUniqueName (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementUniqueName->setInt64(queryParameterIndex++,
							workspaceKey);
						preparedStatementUniqueName->setInt64(queryParameterIndex++,
							localMediaItemKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						shared_ptr<sql::ResultSet> resultSetUniqueName (
							preparedStatementUniqueName->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", workspaceKey: " + to_string(workspaceKey)
							+ ", localMediaItemKey: " + to_string(localMediaItemKey)
							+ ", resultSetUniqueName->rowsCount: "
								+ to_string(resultSetUniqueName->rowsCount())
							+ ", elapsed (millisecs): @" + to_string(
								chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						if (resultSetUniqueName->next())
						{
							field = "uniqueName";
							mediaItemRoot[field] = static_cast<string>(
								resultSetUniqueName->getString("uniqueName"));
						}
						else
						{
							field = "uniqueName";
							mediaItemRoot[field] = string("");
						}
					}
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "tags"))
				{
					{
						Json::Value mediaItemTagsRoot(Json::arrayValue);
                    
						lastSQLCommand = 
							"select name from MMS_Tag where mediaItemKey = ?";

						shared_ptr<sql::PreparedStatement> preparedStatementTags (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementTags->setInt64(queryParameterIndex++,
							localMediaItemKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						shared_ptr<sql::ResultSet> resultSetTags (
							preparedStatementTags->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", localMediaItemKey: " + to_string(localMediaItemKey)
							+ ", resultSetTags->rowsCount: "
								+ to_string(resultSetTags->rowsCount())
							+ ", elapsed (millisecs): @" + to_string(
								chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						while (resultSetTags->next())
						{
							mediaItemTagsRoot.append(static_cast<string>(
								resultSetTags->getString("name")));
						}
                    
						field = "tags";
						mediaItemRoot[field] = mediaItemTagsRoot;
					}
				}

				// CrossReferences
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "crossReferences"))
				{
					// if (contentType == ContentType::Video)
					{
						Json::Value mediaItemReferencesRoot(Json::arrayValue);
                    
						{
							lastSQLCommand = 
								"select sourceMediaItemKey, type, parameters "
								"from MMS_CrossReference "
								"where targetMediaItemKey = ?";
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";

							shared_ptr<sql::PreparedStatement> preparedStatementCrossReferences (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementCrossReferences->setInt64(queryParameterIndex++,
								localMediaItemKey);
							chrono::system_clock::time_point startSql
								= chrono::system_clock::now();
							shared_ptr<sql::ResultSet> resultSetCrossReferences (
									preparedStatementCrossReferences->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", localMediaItemKey: " + to_string(localMediaItemKey)
								+ ", resultSetCrossReferences->rowsCount: "
									+ to_string(resultSetCrossReferences->rowsCount())
								+ ", elapsed (millisecs): @" + to_string(
									chrono::duration_cast<chrono::milliseconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							while (resultSetCrossReferences->next())
							{
								Json::Value crossReferenceRoot;

								field = "sourceMediaItemKey";
								crossReferenceRoot[field]
									= resultSetCrossReferences->getInt64("sourceMediaItemKey");

								field = "type";
								crossReferenceRoot[field] = static_cast<string>(
									resultSetCrossReferences->getString("type"));

								if (!resultSetCrossReferences->isNull("parameters"))
								{
									string crossReferenceParameters =
										resultSetCrossReferences->getString("parameters");
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot
											= JSONUtils::toJson(-1, -1, crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.append(crossReferenceRoot);
							}
						}
                    
						{
							lastSQLCommand = 
								"select type, targetMediaItemKey, parameters "
								"from MMS_CrossReference "
								"where sourceMediaItemKey = ?";
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";

							shared_ptr<sql::PreparedStatement> preparedStatementCrossReferences (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementCrossReferences->setInt64(queryParameterIndex++,
								localMediaItemKey);
							chrono::system_clock::time_point startSql
								= chrono::system_clock::now();
							shared_ptr<sql::ResultSet> resultSetCrossReferences (
								preparedStatementCrossReferences->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", localMediaItemKey: " + to_string(localMediaItemKey)
								+ ", resultSetCrossReferences->rowsCount: "
									+ to_string(resultSetCrossReferences->rowsCount())
								+ ", elapsed (millisecs): @" + to_string(
									chrono::duration_cast<chrono::milliseconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							while (resultSetCrossReferences->next())
							{
								Json::Value crossReferenceRoot;

								field = "type";
								crossReferenceRoot[field] = static_cast<string>(
									resultSetCrossReferences->getString("type"));

								field = "targetMediaItemKey";
								crossReferenceRoot[field] =
									resultSetCrossReferences->getInt64("targetMediaItemKey");

								if (!resultSetCrossReferences->isNull("parameters"))
								{
									string crossReferenceParameters =
										resultSetCrossReferences->getString("parameters");
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot
											= JSONUtils::toJson(-1, -1, crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.append(crossReferenceRoot);
							}
						}

						field = "crossReferences";
						mediaItemRoot[field] = mediaItemReferencesRoot;
					}
					/*
					else if (contentType == ContentType::Audio)
					{
					}
					*/
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "physicalPaths"))
                {
                    Json::Value mediaItemProfilesRoot(Json::arrayValue);
                    
                    lastSQLCommand = 
                        "select physicalPathKey, durationInMilliSeconds, bitRate, externalReadOnlyStorage, "
						"JSON_UNQUOTE(JSON_EXTRACT(deliveryInfo, '$.externalDeliveryTechnology')) as externalDeliveryTechnology, "
						"JSON_UNQUOTE(JSON_EXTRACT(deliveryInfo, '$.externalDeliveryURL')) as externalDeliveryURL, "
						"fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, retentionInMinutes, "
                        "DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                        "from MMS_PhysicalPath where mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfiles (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfiles->setInt64(queryParameterIndex++, localMediaItemKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> resultSetProfiles (preparedStatementProfiles->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", localMediaItemKey: " + to_string(localMediaItemKey)
						+ ", resultSetProfiles->rowsCount: " + to_string(resultSetProfiles->rowsCount())
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    while (resultSetProfiles->next())
                    {
                        Json::Value profileRoot;
                        
                        int64_t physicalPathKey = resultSetProfiles->getInt64("physicalPathKey");

                        field = "physicalPathKey";
                        profileRoot[field] = physicalPathKey;

                        field = "durationInMilliSeconds";
						if (resultSetProfiles->isNull("durationInMilliSeconds"))
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = resultSetProfiles->getInt64("durationInMilliSeconds");

                        field = "bitRate";
						if (resultSetProfiles->isNull("bitRate"))
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = resultSetProfiles->getInt64("bitRate");

                        field = "fileFormat";
                        string fileName = resultSetProfiles->getString("fileName");
                        size_t extensionIndex = fileName.find_last_of(".");
						string fileExtension;
                        if (extensionIndex == string::npos)
                        {
                            profileRoot[field] = Json::nullValue;
                        }
                        else
						{
							fileExtension = fileName.substr(extensionIndex + 1);
							if (fileExtension == "m3u8")
								profileRoot[field] = "hls";
							else
								profileRoot[field] = fileExtension;
						}

						if (admin)
						{
							field = "partitionNumber";
							profileRoot[field] = resultSetProfiles->getInt("partitionNumber");

							field = "relativePath";
							profileRoot[field] = static_cast<string>(resultSetProfiles->getString("relativePath"));

							field = "fileName";
							profileRoot[field] = fileName;
						}

						field = "externalReadOnlyStorage";
						profileRoot[field] = (resultSetProfiles->getInt("externalReadOnlyStorage") == 0 ? false : true);

						field = "externalDeliveryTechnology";
						string externalDeliveryTechnology;
                        if (resultSetProfiles->isNull("externalDeliveryTechnology"))
                            profileRoot[field] = Json::nullValue;
                        else
						{
							externalDeliveryTechnology = resultSetProfiles->getString("externalDeliveryTechnology");
                            profileRoot[field] = externalDeliveryTechnology;
						}

						field = "externalDeliveryURL";
                        if (resultSetProfiles->isNull("externalDeliveryURL"))
                            profileRoot[field] = Json::nullValue;
                        else
                            profileRoot[field] = static_cast<string>(resultSetProfiles->getString("externalDeliveryURL"));

                        field = "encodingProfileKey";
                        if (resultSetProfiles->isNull("encodingProfileKey"))
						{
                            profileRoot[field] = Json::nullValue;

							field = "deliveryTechnology";
							if (externalDeliveryTechnology == "hls")
							{
								profileRoot[field] =
									MMSEngineDBFacade::toString(MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming);
							}
							else
							{
								MMSEngineDBFacade::DeliveryTechnology deliveryTechnology =
									MMSEngineDBFacade::fileFormatToDeliveryTechnology(fileExtension);
								profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);
							}

							field = "encodingProfileLabel";
                            profileRoot[field] = Json::nullValue;
						}
                        else
						{
							int64_t encodingProfileKey = resultSetProfiles->getInt64("encodingProfileKey");

                            profileRoot[field] = encodingProfileKey;

							string label;
							MMSEngineDBFacade::ContentType contentType;
							MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;

                            tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
                                encodingProfileDetails = getEncodingProfileDetailsByKey(workspaceKey, encodingProfileKey);

                            tie(label, contentType, deliveryTechnology, ignore) = encodingProfileDetails;

							field = "deliveryTechnology";
                            profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);

							field = "encodingProfileLabel";
                            profileRoot[field] = label;
						}

                        field = "sizeInBytes";
                        profileRoot[field] = resultSetProfiles->getInt64("sizeInBytes");

                        field = "creationDate";
                        profileRoot[field] = static_cast<string>(resultSetProfiles->getString("creationDate"));

                        field = "retentionInMinutes";
						if (resultSetProfiles->isNull("retentionInMinutes"))
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = resultSetProfiles->getInt64("retentionInMinutes");

                        if (contentType == ContentType::Video)
                        {
							vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							getVideoDetails(localMediaItemKey, physicalPathKey, fromMaster,
								videoTracks, audioTracks);
                            _logger->info(__FILEREF__ + "getVideoDetails"
                                + ", mediaItemKey: " + to_string(localMediaItemKey)
                                + ", physicalPathKey: " + to_string(physicalPathKey)
                                + ", videoTracks.size: " + to_string(videoTracks.size())
                                + ", audioTracks.size: " + to_string(audioTracks.size())
							);

							{
								Json::Value videoTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack: videoTracks)
								{
									int64_t videoTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									int width;
									int height;
									string avgFrameRate;
									string codecName;
									long bitRate;
									string profile;

									tie(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
										avgFrameRate, codecName, bitRate, profile) = videoTrack;

									Json::Value videoTrackRoot;

									field = "videoTrackKey";
									videoTrackRoot[field] = videoTrackKey;

									field = "trackIndex";
									videoTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									videoTrackRoot[field] = durationInMilliSeconds;

									field = "width";
									videoTrackRoot[field] = width;

									field = "height";
									videoTrackRoot[field] = height;

									field = "avgFrameRate";
									videoTrackRoot[field] = avgFrameRate;

									field = "codecName";
									videoTrackRoot[field] = codecName;

									field = "bitRate";
									videoTrackRoot[field] = (int64_t) bitRate;

									field = "profile";
									videoTrackRoot[field] = profile;

									videoTracksRoot.append(videoTrackRoot);
								}

								field = "videoTracks";
								profileRoot[field] = videoTracksRoot;
							}

							{
								Json::Value audioTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack: audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName,
										sampleRate, channels, language) = audioTrack;

									Json::Value audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t) bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t) sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t) channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.append(audioTrackRoot);
								}

								field = "audioTracks";
								profileRoot[field] = audioTracksRoot;
							}
                        }
                        else if (contentType == ContentType::Audio)
                        {
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							getAudioDetails(localMediaItemKey, physicalPathKey, fromMaster, audioTracks);

							{
								Json::Value audioTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, long, string, long, int, string>
									audioTrack: audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate,
										codecName, sampleRate, channels, language) = audioTrack;

									Json::Value audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t) bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t) sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t) channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.append(audioTrackRoot);
								}

								field = "audioTracks";
								profileRoot[field] = audioTracksRoot;
							}
                        }
                        else if (contentType == ContentType::Image)
                        {
                            int width;
                            int height;
                            string format;
                            int quality;

                            tuple<int,int,string,int>
                                imageDetails = getImageDetails(localMediaItemKey, physicalPathKey,
									fromMaster);

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
			_logger->info(__FILEREF__ + "@SQL statistics@ - mediaItems"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", start: " + to_string(start)
				+ ", rows: " + to_string(rows)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSqlResultSet).count()) + "@"
			);
        }

        field = "mediaItems";
        responseRoot[field] = mediaItemsRoot;

        field = "response";
        mediaItemsListRoot[field] = responseRoot;

		if (tagsIn.size() > 0 || tagsNotIn.size() > 0)
		{
			lastSQLCommand = 
				string("drop temporary table ") + temporaryTableName;
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
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
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception& e)
            {
                _logger->error(__FILEREF__ + "exception"
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
    catch(runtime_error& e)
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
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception& e)
            {
                _logger->error(__FILEREF__ + "exception"
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
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException& se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception& e)
            {
                _logger->error(__FILEREF__ + "exception"
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
    
    return mediaItemsListRoot;
}

pair<shared_ptr<sql::ResultSet>, int64_t> MMSEngineDBFacade::getMediaItemsList_withoutTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
        string orderBy,			// i.e.: "", mi.ingestionDate desc, mi.title asc
		string jsonOrderBy,		// i.e.: "", JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc
		bool admin
)
{
    string						lastSQLCommand;
    
    try
    {
        string sqlWhere;
		sqlWhere = string ("where mi.workspaceKey = ? and mi.markedAsRemoved = 0 ");
        if (mediaItemKey != -1)
		{
			if (otherMediaItemsKey.size() > 0)
			{
				sqlWhere += ("and mi.mediaItemKey in (");
				sqlWhere += to_string(mediaItemKey);
				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
				{
					sqlWhere += (", " + to_string(otherMediaItemsKey[mediaItemIndex]));
				}
				sqlWhere += ") ";
			}
			else
				sqlWhere += ("and mi.mediaItemKey = ? ");
		}
        if (contentTypePresent)
            sqlWhere += ("and mi.contentType = ? ");
        // if (startAndEndIngestionDatePresent)
        //     sqlWhere += ("and mi.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and mi.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (startIngestionDate != "")
            sqlWhere += ("and mi.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (endIngestionDate != "")
            sqlWhere += ("and mi.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (title != "")
            sqlWhere += ("and LOWER(mi.title) like LOWER(?) ");		// LOWER was used because the column is using utf8_bin that is case sensitive
		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk
		 *  1: look for liveRecordingChunk
		 */
        if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
				sqlWhere += ("and JSON_EXTRACT(userData, '$.mmsData.liveRecordingChunk') is NULL ");
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and JSON_EXTRACT(userData, '$.mmsData.liveRecordingChunk') is not NULL ");
				// sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
		}
		if (recordingCode != -1)
			sqlWhere += ("and mi.recordingCode_virtual = ? ");

		if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
		{
			// SC: Start Chunk
			// PS: Playout Start, PE: Playout End
			// --------------SC--------------SC--------------SC--------------SC
			//                       PS-------------------------------PE
			
			sqlWhere += ("and ( ");

			// first chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodStartTimeInMilliSeconds
			sqlWhere += ("(mi.utcStartTimeInMilliSecs_virtual <= ? and ? < mi.utcEndTimeInMilliSecs_virtual) ");

			sqlWhere += ("or ");

			// internal chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += ("(? <= mi.utcStartTimeInMilliSecs_virtual and mi.utcEndTimeInMilliSecs_virtual <= ?) ");

			sqlWhere += ("or ");

			// last chunk of the cut
			// utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += ("(mi.utcStartTimeInMilliSecs_virtual < ? and ? <= mi.utcEndTimeInMilliSecs_virtual) ");

			sqlWhere += (") ");
		}

        if (jsonCondition != "")
            sqlWhere += ("and " + jsonCondition + " ");
        
		int64_t numFound;
        {
			lastSQLCommand = 
				string("select count(*) from MMS_MediaItem mi ")
				+ sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (mediaItemKey != -1)
			{
				if (otherMediaItemsKey.size() == 0)
					preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			}
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            // if (startAndEndIngestionDatePresent)
            // {
            //     preparedStatement->setString(queryParameterIndex++, startIngestionDate);
            //     preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            // }
            if (startIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
            if (endIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (recordingCode != -1)
				preparedStatement->setInt64(queryParameterIndex++, recordingCode);
			if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
			{
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", title: " + "%" + title + "%"
				+ ", recordingCode: " + to_string(recordingCode)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                numFound = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage (__FILEREF__ + "select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        {
			string orderByCondition;
			if (orderBy == "" && jsonOrderBy == "")
			{
				orderByCondition = " ";
			}
			else if (orderBy == "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + " ";
			}
			else if (orderBy != "" && jsonOrderBy == "")
			{
				orderByCondition = "order by " + orderBy + " ";
			}
			else // if (orderBy != "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + ", " + orderBy + " ";
			}

          	lastSQLCommand = 
           		string("select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
           			"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
           			"DATE_FORMAT(convert_tz(mi.startPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startPublishing, "
           			"DATE_FORMAT(convert_tz(mi.endPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endPublishing, "
           			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi ")
           			+ sqlWhere
           			+ orderByCondition
           			+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (mediaItemKey != -1)
			{
				if (otherMediaItemsKey.size() == 0)
					preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			}
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            // if (startAndEndIngestionDatePresent)
            // {
            //     preparedStatement->setString(queryParameterIndex++, startIngestionDate);
            //     preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            // }
            if (startIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
            if (endIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (recordingCode != -1)
				preparedStatement->setInt64(queryParameterIndex++, recordingCode);
			if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
			{
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
			}
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet(preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", title: " + "%" + title + "%"
				+ ", recordingCode: " + to_string(recordingCode)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);


			return make_pair(resultSet, numFound);
        }
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    } 
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    } 
}

pair<shared_ptr<sql::ResultSet>, int64_t> MMSEngineDBFacade::getMediaItemsList_withTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, string temporaryTableName,
		int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string orderBy,			// i.e.: "", mi.ingestionDate desc, mi.title asc
		string jsonOrderBy,		// i.e.: "", JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc
		bool admin
)
{
    string      lastSQLCommand;
    

    try
    {
		string tagSeparator = "__SEP__";

		string tagsGroupCondition;
        {
			string tagsInGroupCondition;
			if (tagsIn.size() > 0)
			{
				for (string tag: tagsIn)
				{
					tag = tagSeparator + tag + tagSeparator;

					if (tagsInGroupCondition == "")
						tagsInGroupCondition = "f.tagsGroup like '%" + tag + "%' ";
					else
						tagsInGroupCondition += ("or f.tagsGroup like '%" + tag + "%' ");
				}
			}

			string tagsNotInGroupCondition;
			if (tagsNotIn.size() > 0)
			{
				for (string tag: tagsNotIn)
				{
					tag = tagSeparator + tag + tagSeparator;

					if (tagsNotInGroupCondition == "")
						tagsNotInGroupCondition = "f.tagsGroup not like '%" + tag + "%' ";
					else
						tagsNotInGroupCondition += ("and f.tagsGroup not like '%" + tag + "%' ");
				}
			}
			if (tagsInGroupCondition == "" && tagsNotInGroupCondition == "")
				;
			else if (tagsInGroupCondition == "" && tagsNotInGroupCondition != "")
				tagsGroupCondition = string("(") + tagsNotInGroupCondition + ") ";
			else if (tagsInGroupCondition != "" && tagsNotInGroupCondition == "")
				tagsGroupCondition = string("(") + tagsInGroupCondition + ") ";
			else
				tagsGroupCondition = string("(") + tagsInGroupCondition + ") and (" + tagsNotInGroupCondition + ") ";
		}

		// create temporary table
		bool createdTemporaryTable = false;
		{
			string sqlWhere;
			sqlWhere = string ("where mi.mediaItemKey = t.mediaItemKey and mi.workspaceKey = ? ");
			if (mediaItemKey != -1)
			{
				if (otherMediaItemsKey.size() > 0)
				{
					sqlWhere += ("and mi.mediaItemKey in (");
					sqlWhere += to_string(mediaItemKey);
					for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					{
						sqlWhere += (", " + to_string(otherMediaItemsKey[mediaItemIndex]));
					}
					sqlWhere += ") ";
				}
				else
					sqlWhere += ("and mi.mediaItemKey = ? ");
			}
			if (contentTypePresent)
				sqlWhere += ("and mi.contentType = ? ");
			// if (startAndEndIngestionDatePresent)
			// 	sqlWhere += ("and mi.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and mi.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
			if (startIngestionDate != "")
				sqlWhere += ("and mi.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
			if (endIngestionDate != "")
				sqlWhere += ("and mi.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
			if (title != "")
				sqlWhere += ("and LOWER(mi.title) like LOWER(?) ");		// LOWER was used because the column is using utf8_bin that is case sensitive
			/*
			* liveRecordingChunk:
			* -1: no condition in select
			*  0: look for NO liveRecordingChunk
			*  1: look for liveRecordingChunk
			*/
			if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
			{
				if (liveRecordingChunk == 0)
					sqlWhere += ("and JSON_EXTRACT(mi.userData, '$.mmsData.liveRecordingChunk') is NULL ");
				else if (liveRecordingChunk == 1)
					sqlWhere += ("and JSON_EXTRACT(mi.userData, '$.mmsData.liveRecordingChunk') is not NULL ");
					// sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(mi.userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
			}
			if (recordingCode != -1)
				sqlWhere += ("and mi.recordingCode_virtual = ? ");
			if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
			{
				// SC: Start Chunk
				// PS: Playout Start, PE: Playout End
				// --------------SC--------------SC--------------SC--------------SC
				//                       PS-------------------------------PE
			
				sqlWhere += ("and ( ");

				// first chunk of the cut
				// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodStartTimeInMilliSeconds
				sqlWhere += ("(mi.utcStartTimeInMilliSecs_virtual <= ? and ? < mi.utcEndTimeInMilliSecs_virtual) ");

				sqlWhere += ("or ");

				// internal chunk of the cut
				// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
				sqlWhere += ("(? <= mi.utcStartTimeInMilliSecs_virtual and mi.utcEndTimeInMilliSecs_virtual <= ?) ");

				sqlWhere += ("or ");

				// last chunk of the cut
				// utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
				sqlWhere += ("(mi.utcStartTimeInMilliSecs_virtual < ? and ? <= mi.utcEndTimeInMilliSecs_virtual) ");

				sqlWhere += (") ");
			}
			if (jsonCondition != "")
				sqlWhere += ("and " + jsonCondition + " ");
        
			lastSQLCommand = 
				string("create temporary table ") + temporaryTableName + " select "
					+ "t.mediaItemKey, CONCAT('" + tagSeparator
						+ "', GROUP_CONCAT(t.name SEPARATOR '" + tagSeparator + "'), '"
						+ tagSeparator + "') tagsGroup "
					+ "from MMS_MediaItem mi, MMS_Tag t "
				+ sqlWhere
				+ "group by t.mediaItemKey "
			;
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (mediaItemKey != -1)
			{
				if (otherMediaItemsKey.size() == 0)
					preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			}
			if (contentTypePresent)
				preparedStatement->setString(queryParameterIndex++, toString(contentType));
			// if (startAndEndIngestionDatePresent)
			// {
			// 	preparedStatement->setString(queryParameterIndex++, startIngestionDate);
			// 	preparedStatement->setString(queryParameterIndex++, endIngestionDate);
			// }
            if (startIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
            if (endIngestionDate != "")
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (recordingCode != -1)
				preparedStatement->setInt64(queryParameterIndex++, recordingCode);
			if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
			{
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodStartTimeInMilliSeconds);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);

				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
				preparedStatement->setInt64(queryParameterIndex++, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", title: " + title
				+ ", recordingCode: " + to_string(recordingCode)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodStartTimeInMilliSeconds: " + to_string(utcCutPeriodStartTimeInMilliSeconds)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", utcCutPeriodEndTimeInMilliSecondsPlusOneSecond: " + to_string(utcCutPeriodEndTimeInMilliSecondsPlusOneSecond)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			createdTemporaryTable = true;
		}

		int64_t numFound;
		{
			lastSQLCommand = 
				string("select count(*) from ") + temporaryTableName + " f where "
				+ tagsGroupCondition;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                numFound = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage (__FILEREF__ + "select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
			string sqlWhere = string ("where mi.mediaItemKey = f.mediaItemKey and ")
				+ tagsGroupCondition
				;

			string orderByCondition;
			if (orderBy == "" && jsonOrderBy == "")
			{
				orderByCondition = " ";
			}
			else if (orderBy == "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + " ";
			}
			else if (orderBy != "" && jsonOrderBy == "")
			{
				orderByCondition = "order by " + orderBy + " ";
			}
			else // if (orderBy != "" && jsonOrderBy != "")
			{
				orderByCondition = "order by " + jsonOrderBy + ", " + orderBy + " ";
			}

          	lastSQLCommand = 
           		string("select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
           			"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
           			"DATE_FORMAT(convert_tz(mi.startPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startPublishing, "
           			"DATE_FORMAT(convert_tz(mi.endPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endPublishing, "
           			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi, " + temporaryTableName + " f ")
           			+ sqlWhere
           			+ orderByCondition
           			+ "limit ? offset ?";

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
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);


			return make_pair(resultSet, numFound);
        }
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    } 
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    } 
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
    int64_t referenceMediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having 'ts' file format and bigger size in case there are more than one)
	int64_t encodingProfileKey,
	bool warningIfMissing, bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    int64_t physicalPathKey = -1;

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

		if (encodingProfileKey != -1)
        {
			lastSQLCommand = string("") +
				"select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? "
				"and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceMediaItemKey);
			preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", referenceMediaItemKey: " + to_string(referenceMediaItemKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
                physicalPathKey = resultSet->getInt64("physicalPathKey");

			if (physicalPathKey == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/encodingProfileKey are not found"
                    + ", mediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }
        }
		else
		{
			tuple<int64_t, int, string, string, uint64_t, bool, int64_t> sourcePhysicalPathDetails =
				getSourcePhysicalPath(referenceMediaItemKey, warningIfMissing, fromMaster);
			tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = sourcePhysicalPathDetails;
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

    return physicalPathKey;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
        int64_t workspaceKey,
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel,
		bool warningIfMissing,
		bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    int64_t physicalPathKey = -1;
    
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

        int64_t encodingProfileKey = -1;
        {
            lastSQLCommand = 
                "select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = ? or workspaceKey is null) and "
				"contentType = ? and label = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, toString(contentType));
            preparedStatement->setString(queryParameterIndex++, encodingProfileLabel);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", contentType: " + toString(contentType)
				+ ", encodingProfileLabel: " + encodingProfileLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? "
				"and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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
    
    return physicalPathKey;
}

string MMSEngineDBFacade::getPhysicalPathDetails(
	int64_t physicalPathKey, bool warningIfMissing,
	bool fromMaster
)
{
    return "";
}

tuple<int64_t, int, string, string, uint64_t, bool, int64_t> MMSEngineDBFacade::getSourcePhysicalPath(
    int64_t mediaItemKey, bool warningIfMissing, bool fromMaster
)
{
    string      lastSQLCommand;

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

		int64_t physicalPathKeyWithEncodingProfile = -1;
		int64_t physicalPathKeyWithoutEncodingProfile = -1;

        int mmsPartitionNumberWithEncodingProfile;
        int mmsPartitionNumberWithoutEncodingProfile;

        bool externalReadOnlyStorageWithEncodingProfile;
        bool externalReadOnlyStorageWithoutEncodingProfile;

        string relativePathWithEncodingProfile;
        string relativePathWithoutEncodingProfile;

        string fileNameWithEncodingProfile;
        string fileNameWithoutEncodingProfile;

        uint64_t sizeInBytesWithEncodingProfile;
        uint64_t sizeInBytesWithoutEncodingProfile;

        int64_t durationInMilliSecondsWithEncodingProfile = 0;
        int64_t durationInMilliSecondsWithoutEncodingProfile = 0;

		uint64_t maxSizeInBytesWithEncodingProfile = -1;
		uint64_t maxSizeInBytesWithoutEncodingProfile = -1;

        {
			// 2023-01-23: l'ultima modifica fatta permette di inserire 'source content' specificando
			//	l'encoding profile (questo quando siamo sicuri che sia stato generato
			//	con uno specifico profilo). In questo nuovo scenario, molti 'source content' hanno specificato
			//	encodingProfileKey nella tabella MMS_PhysicalPath.
			//	Per cui viene tolta la condizione
			//		encodingProfileKey is null
			//	dalla select e viene cercato il source nel seguente modo:
			//	- se esiste un 'cource content' avente encodingProfileKey null viene considerato
			//		questo contenuto come 'source'
			//	- se non esiste viene cercato il source tra quelli con encodingProfileKey inizializzato
			lastSQLCommand = string("") +
				"select physicalPathKey, sizeInBytes, fileName, relativePath, partitionNumber, "
				"externalReadOnlyStorage, durationInMilliSeconds, encodingProfileKey "
				"from MMS_PhysicalPath where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			string selectedFileFormatWithEncodingProfile;
			string selectedFileFormatWithoutEncodingProfile;

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
				uint64_t localSizeInBytes = resultSet->getUInt64("sizeInBytes");

				string localFileName = resultSet->getString("fileName");
				string localFileFormat;
				size_t extensionIndex = localFileName.find_last_of(".");
				if (extensionIndex != string::npos)
					localFileFormat = localFileName.substr(extensionIndex + 1);

				if (resultSet->isNull("encodingProfileKey"))
				{
					if (maxSizeInBytesWithoutEncodingProfile != -1)
					{
						// this is the second or third... physicalPath
						// we are fore sure in the scenario encodingProfileKey == -1
						// So, in case we have more than one "source" physicalPath, we will select the 'ts' one
						// We prefer 'ts' because is easy and safe do activities like cut or concat
						if (selectedFileFormatWithoutEncodingProfile == "ts")
						{
							if (localFileFormat == "ts")
							{
								if (localSizeInBytes <= maxSizeInBytesWithoutEncodingProfile)
									continue;
							}
							else
							{
								continue;
							}
						}
						else
						{
							if (localSizeInBytes <= maxSizeInBytesWithoutEncodingProfile)
								continue;
						}
					}

					physicalPathKeyWithoutEncodingProfile = resultSet->getInt64("physicalPathKey");
					externalReadOnlyStorageWithoutEncodingProfile = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
					mmsPartitionNumberWithoutEncodingProfile = resultSet->getInt("partitionNumber");
					relativePathWithoutEncodingProfile = resultSet->getString("relativePath");
					fileNameWithoutEncodingProfile = resultSet->getString("fileName");
					sizeInBytesWithoutEncodingProfile = resultSet->getUInt64("sizeInBytes");
					if (!resultSet->isNull("durationInMilliSeconds"))
						durationInMilliSecondsWithoutEncodingProfile = resultSet->getInt64("durationInMilliSeconds");

					fileNameWithoutEncodingProfile = localFileName;
					maxSizeInBytesWithoutEncodingProfile = localSizeInBytes;
					selectedFileFormatWithoutEncodingProfile = localFileFormat;
				}
				else
				{
					if (maxSizeInBytesWithEncodingProfile != -1)
					{
						// this is the second or third... physicalPath
						// we are fore sure in the scenario encodingProfileKey == -1
						// So, in case we have more than one "source" physicalPath, we will select the 'ts' one
						// We prefer 'ts' because is easy and safe do activities like cut or concat
						if (selectedFileFormatWithEncodingProfile == "ts")
						{
							if (localFileFormat == "ts")
							{
								if (localSizeInBytes <= maxSizeInBytesWithEncodingProfile)
									continue;
							}
							else
							{
								continue;
							}
						}
						else
						{
							if (localSizeInBytes <= maxSizeInBytesWithEncodingProfile)
								continue;
						}
					}

					physicalPathKeyWithEncodingProfile = resultSet->getInt64("physicalPathKey");
					externalReadOnlyStorageWithEncodingProfile = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
					mmsPartitionNumberWithEncodingProfile = resultSet->getInt("partitionNumber");
					relativePathWithEncodingProfile = resultSet->getString("relativePath");
					fileNameWithEncodingProfile = resultSet->getString("fileName");
					sizeInBytesWithEncodingProfile = resultSet->getUInt64("sizeInBytes");
					if (!resultSet->isNull("durationInMilliSeconds"))
						durationInMilliSecondsWithEncodingProfile = resultSet->getInt64("durationInMilliSeconds");

					fileNameWithEncodingProfile = localFileName;
					maxSizeInBytesWithEncodingProfile = localSizeInBytes;
					selectedFileFormatWithEncodingProfile = localFileFormat;
				}
            }

			if (maxSizeInBytesWithoutEncodingProfile == -1 && maxSizeInBytesWithEncodingProfile == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
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

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		// senza encoding profile ha priorità rispetto a 'con encoding profile'
		if (maxSizeInBytesWithoutEncodingProfile != -1)
			return make_tuple(physicalPathKeyWithoutEncodingProfile, mmsPartitionNumberWithoutEncodingProfile,
				relativePathWithoutEncodingProfile, fileNameWithoutEncodingProfile,
				sizeInBytesWithoutEncodingProfile, externalReadOnlyStorageWithoutEncodingProfile,
				durationInMilliSecondsWithoutEncodingProfile);
		else
			return make_tuple(physicalPathKeyWithEncodingProfile, mmsPartitionNumberWithEncodingProfile,
				relativePathWithEncodingProfile, fileNameWithEncodingProfile,
				sizeInBytesWithEncodingProfile, externalReadOnlyStorageWithEncodingProfile,
				durationInMilliSecondsWithEncodingProfile);
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
	MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing,
	bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
		contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

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
            lastSQLCommand = 
                "select contentType, title, userData, ingestionJobKey, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
				"TIME_TO_SEC(TIMEDIFF(DATE_ADD(ingestionDate, INTERVAL retentionInMinutes MINUTE), NOW())) willBeRemovedInSeconds "
				"from MMS_MediaItem where workspaceKey = ? and mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                
                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

                int64_t willBeRemovedInSeconds = resultSet->getInt64("willBeRemovedInSeconds");

                contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey =
					make_tuple(contentType, title, userData, ingestionDate, willBeRemovedInSeconds, ingestionJobKey);
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

                throw MediaItemKeyNotFound(errorMessage);                    
            }
        }
                        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

        return contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t,
	string, string, int64_t>
	MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(
	int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing,
	bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t,
		string, string, int64_t> mediaItemDetails;

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
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, "
				"mi.ingestionJobKey, p.fileName, p.relativePath, p.durationInMilliSeconds, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), "
					"'%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
                "where mi.workspaceKey = ? and mi.mediaItemKey = p.mediaItemKey "
				"and p.physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @"
					+ to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                int64_t mediaItemKey = resultSet->getInt64("mediaItemKey");
                MMSEngineDBFacade::ContentType contentType
					= MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));

                string userData;
                if (!resultSet->isNull("userData"))
                    userData = resultSet->getString("userData");
                
                string title;
                if (!resultSet->isNull("title"))
                    title = resultSet->getString("title");

                string fileName = resultSet->getString("fileName");
                string relativePath = resultSet->getString("relativePath");

                string ingestionDate;
                if (!resultSet->isNull("ingestionDate"))
                    ingestionDate = resultSet->getString("ingestionDate");

                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
				int64_t durationInMilliSeconds = 0;
				if (!resultSet->isNull("durationInMilliSeconds"))
					durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");

                mediaItemDetails = make_tuple(mediaItemKey, contentType, title,
					userData, ingestionDate, ingestionJobKey, fileName, relativePath,
					durationInMilliSeconds);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
        
        return mediaItemDetails;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

void MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey(
	int64_t workspaceKey, int64_t referenceIngestionJobKey, 
	int maxLastMediaItemsToBeReturned,
	vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>>& mediaItemsDetails,
	bool warningIfMissing, bool fromMaster
)
{
    string      lastSQLCommand;
        
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

		IngestionType ingestionType;
        {
			lastSQLCommand =
				"select ingestionType from MMS_IngestionJob "
				"where ingestionJobKey = ? ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceIngestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				ingestionType     = MMSEngineDBFacade::toIngestionType(
					resultSet->getString("ingestionType"));
			}
			else
			{
				string errorMessage = __FILEREF__ + "IngestionJob is not found"
					+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
					+ ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->error(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);                    
			}
		}

        {
            // order by in the next select is important  to have the right order in case of dependency in a workflow
			/*
            lastSQLCommand = 
                "select ijo.mediaItemKey, ijo.physicalPathKey from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
				"where ijo.mediaItemKey = mi.mediaItemKey and ijo.ingestionJobKey = ? and "
				"(JSON_EXTRACT(userData, '$.mmsData.validated') is null or "	// in case of no live chunk MIK
					"JSON_EXTRACT(userData, '$.mmsData.validated') = true) "	// in case of live chunk MIK, we get only the one validated
				"order by ijo.mediaItemKey";
			*/
			// 2019-09-20: The Live-Recorder task now updates the Ingestion Status at the end of the task,
			// when main and backup management is finished (no MIKs with valitaded==false are present)
			// So we do not need anymore the above check
			string orderBy;
			if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
			{
				string segmenterType = "hlsSegmenter";
				// string segmenterType = "streamSegmenter";
				if (segmenterType == "hlsSegmenter")
					orderBy = "order by mi.utcStartTimeInMilliSecs_virtual desc ";
				else
					orderBy = "order by JSON_EXTRACT(mi.userData, '$.mmsData.utcChunkStartTime') desc ";
			}
			else
				orderBy = "order by ijo.mediaItemKey desc ";

			lastSQLCommand =
				string("select ijo.mediaItemKey, ijo.physicalPathKey ")
				+ "from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
				+ "where mi.workspaceKey = ? and ijo.mediaItemKey = mi.mediaItemKey "
				+ "and ijo.ingestionJobKey = ? "
				+ orderBy;
			if (maxLastMediaItemsToBeReturned != -1)
				lastSQLCommand += ("limit " + to_string(maxLastMediaItemsToBeReturned));
			/*
			lastSQLCommand =
				"select mediaItemKey, physicalPathKey "
				"from MMS_IngestionJobOutput "
				"where ingestionJobKey = ? order by mediaItemKey";
			*/
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, referenceIngestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> resultSetMediaItem (preparedStatementMediaItem->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", resultSetMediaItem->rowsCount: " + to_string(resultSetMediaItem->rowsCount())
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
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
						{
                            _logger->warn(errorMessage);

							continue;
						}
                        else
						{
                            _logger->error(errorMessage);

							throw MediaItemKeyNotFound(errorMessage);                    
						}
                    }
                }

				tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType 
					= make_tuple(mediaItemKey, physicalPathKey, contentType);
				mediaItemsDetails.insert(mediaItemsDetails.begin(),
					mediaItemKeyPhysicalPathKeyAndContentType);
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

pair<int64_t,MMSEngineDBFacade::ContentType>
	MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
    int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing,
	bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

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
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType "
				"from MMS_MediaItem mi, MMS_ExternalUniqueName eun "
                "where mi.mediaItemKey = eun.mediaItemKey "
                "and eun.workspaceKey = ? and eun.uniqueName = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, referenceUniqueName);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", referenceUniqueName: " + referenceUniqueName
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                mediaItemKeyAndContentType.first = resultSet->getInt64("mediaItemKey");
                mediaItemKeyAndContentType.second = MMSEngineDBFacade::toContentType(
						resultSet->getString("contentType"));
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
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
    {
        if (warningIfMissing)
            _logger->warn(__FILEREF__ + "MediaItemKeyNotFound"
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
            connectionPool->unborrow(conn);
			conn = nullptr;
        }
        
        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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
    
    return mediaItemKeyAndContentType;
}

int64_t MMSEngineDBFacade::getMediaDurationInMilliseconds(
	// mediaItemKey or physicalPathKey has to be initialized, the other has to be -1
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	bool fromMaster
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
	int64_t durationInMilliSeconds;

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

        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->isNull("durationInMilliSeconds"))
				{
					string errorMessage = __FILEREF__ + "duration is not found"
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}

				durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
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
            lastSQLCommand = 
				"select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->isNull("durationInMilliSeconds"))
				{
					string errorMessage = __FILEREF__ + "duration is not found"
						+ ", physicalPathKey: " + to_string(physicalPathKey)
						+ ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}

				durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey is not found"
					+ ", physicalPathKey: " + to_string(physicalPathKey)
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
    catch(MediaItemKeyNotFound& mnf)
    {
        string exceptionMessage(mnf.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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

        throw mnf;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

	return durationInMilliSeconds;
}

void MMSEngineDBFacade::getVideoDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	bool fromMaster,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>>& videoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
	)
{
    string      lastSQLCommand;
        
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

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        
		videoTracks.clear();
		audioTracks.clear();

        {
            lastSQLCommand = 
                "select videoTrackKey, trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
                "codecName, profile, bitRate "
                "from MMS_VideoTrack where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                int64_t videoTrackKey = resultSet->getInt64("videoTrackKey");
                int trackIndex = -1;
				if (!resultSet->isNull("trackIndex"))
					trackIndex = resultSet->getInt("trackIndex");
                int64_t durationInMilliSeconds = -1;
				if (!resultSet->isNull("durationInMilliSeconds"))
					durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                long bitRate = -1;
				if (!resultSet->isNull("bitRate"))
					bitRate = resultSet->getInt("bitRate");
                string codecName;
				if (!resultSet->isNull("codecName"))
					codecName = resultSet->getString("codecName");
                string profile;
				if (!resultSet->isNull("profile"))
					profile = resultSet->getString("profile");
                int width = -1;
				if (!resultSet->isNull("width"))
					width = resultSet->getInt("width");
                int height = -1;
				if (!resultSet->isNull("height"))
					height = resultSet->getInt("height");
                string avgFrameRate;
				if (!resultSet->isNull("avgFrameRate"))
					avgFrameRate = resultSet->getString("avgFrameRate");

				videoTracks.push_back(make_tuple(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
					avgFrameRate, codecName, bitRate, profile));
            }
        }

        {
            lastSQLCommand = 
                "select audioTrackKey, trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language "
                "from MMS_AudioTrack where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                int64_t audioTrackKey = resultSet->getInt64("audioTrackKey");
                int trackIndex;
				if (!resultSet->isNull("trackIndex"))
					trackIndex = resultSet->getInt("trackIndex");
                int64_t durationInMilliSeconds;
				if (!resultSet->isNull("durationInMilliSeconds"))
					durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                long bitRate = -1;
				if (!resultSet->isNull("bitRate"))
					bitRate = resultSet->getInt("bitRate");
                string codecName;
				if (!resultSet->isNull("codecName"))
					codecName = resultSet->getString("codecName");
                long sampleRate = -1;
				if (!resultSet->isNull("sampleRate"))
					sampleRate = resultSet->getInt("sampleRate");
                int channels = -1;
				if (!resultSet->isNull("channels"))
					channels = resultSet->getInt("channels");
                string language;
				if (!resultSet->isNull("language"))
					language = resultSet->getString("language");

				audioTracks.push_back(make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language));
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(MediaItemKeyNotFound& mnf)
    {
        string exceptionMessage(mnf.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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

        throw mnf;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

void MMSEngineDBFacade::getAudioDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	bool fromMaster,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
	)
{
    string      lastSQLCommand;
        
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

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        
		audioTracks.clear();

        {
            lastSQLCommand = 
                "select audioTrackKey, trackIndex, durationInMilliSeconds, "
				"codecName, bitRate, sampleRate, channels, language "
                "from MMS_AudioTrack where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                int64_t audioTrackKey = resultSet->getInt64("audioTrackKey");
                int trackIndex;
				if (!resultSet->isNull("trackIndex"))
					trackIndex = resultSet->getInt("trackIndex");
                int64_t durationInMilliSeconds;
				if (!resultSet->isNull("durationInMilliSeconds"))
					durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                long bitRate = -1;
				if (!resultSet->isNull("bitRate"))
					bitRate = resultSet->getInt("bitRate");
                string codecName;
				if (!resultSet->isNull("codecName"))
					codecName = resultSet->getString("codecName");
                long sampleRate = -1;
				if (!resultSet->isNull("sampleRate"))
					sampleRate = resultSet->getInt("sampleRate");
                int channels = -1;
				if (!resultSet->isNull("channels"))
					channels = resultSet->getInt("channels");
                string language;
				if (!resultSet->isNull("language"))
					language = resultSet->getString("language");

				audioTracks.push_back(make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds,
					bitRate, codecName, sampleRate, channels, language));
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
    {
        _logger->error(__FILEREF__ + "MediaItemKeyNotFound"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

tuple<int,int,string,int> MMSEngineDBFacade::getImageDetails(
    int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster
)
{
    string      lastSQLCommand;
        
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

        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
        
        return make_tuple(width, height, format, quality);
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(MediaItemKeyNotFound& e)
    {
        _logger->error(__FILEREF__ + "MediaItemKeyNotFound"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

pair<int64_t,int64_t> MMSEngineDBFacade::saveSourceContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,        
        MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfileKey,
        Json::Value parametersRoot,
		bool externalReadOnlyStorage,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,

        // video-audio
		tuple<int64_t, long, Json::Value>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

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

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

	string title = "";
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

        _logger->info(__FILEREF__ + "Retrieving contentProviderKey");
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
			contentProviderName = JSONUtils::asString(parametersRoot, "contentProviderName", _defaultContentProviderName);

            lastSQLCommand = 
                "select contentProviderKey from MMS_ContentProvider where workspaceKey = ? and name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setString(queryParameterIndex++, contentProviderName);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", contentProviderName: " + contentProviderName
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
            string ingester = "";
            string userData = "";
            string metadata = "";
            string deliveryFileName = "";
            string sContentType;
            int64_t retentionInMinutes = _contentRetentionInMinutesDefaultValue;
            // string encodingProfilesSet;

            string field = "title";
            title = JSONUtils::asString(parametersRoot, field, "");
            
            field = "ingester";
			ingester = JSONUtils::asString(parametersRoot, field, "");

            field = "userData";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
				// 2020-03-15: when it is set by the GUI it arrive here as a string
				if ((parametersRoot[field]).type() == Json::stringValue)
				{
					userData = JSONUtils::asString(parametersRoot, field, "");

					// _logger->error(__FILEREF__ + "STRING AAAAAAAAAAA"
					// 	+ ", userData: " + userData
					// );
				}
				else
				{
					userData = JSONUtils::toString(parametersRoot[field]);

					// _logger->error(__FILEREF__ + "NO STRING AAAAAAAAAAA"
					// 	+ ", userData: " + userData
					// );
				}
            }

            field = "deliveryFileName";
			deliveryFileName = JSONUtils::asString(parametersRoot, field, "");

            field = "retention";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
                string retention = JSONUtils::asString(parametersRoot, field, "1d");
				retentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
            }

            string startPublishing = "NOW";
            string endPublishing = "FOREVER";
            {
                field = "publishing";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value publishingRoot = parametersRoot[field];

                    field = "startPublishing";
					startPublishing = JSONUtils::asString(publishingRoot, field, "NOW");

                    field = "endPublishing";
					endPublishing = JSONUtils::asString(publishingRoot, field, "FOREVER");
                }
                
                if (startPublishing == "NOW")
                {
                    tm          tmDateTime;
                    // char        strDateTime [64];
                    string strDateTime;

                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    time_t utcTime = chrono::system_clock::to_time_t(now);

					// 2019-03-31: in case startPublishing is wrong, check how gmtime is used in MMSEngineDBFacade_Lock.cpp
                	gmtime_r (&utcTime, &tmDateTime);

                    /*
                    sprintf (strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);
                            */
				strDateTime = fmt::format(
					"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
					tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
				);

                    startPublishing = strDateTime;
                }

                if (endPublishing == "FOREVER")
                {
                    tm          tmDateTime;
                    // char        strDateTime [64];
                    string strDateTime;

                    chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 10);

                    time_t utcTime = chrono::system_clock::to_time_t(forever);

					// 2019-03-31: in case startPublishing is wrong, check how gmtime is used in MMSEngineDBFacade_Lock.cpp
                	gmtime_r (&utcTime, &tmDateTime);

                    /*
                    sprintf (strDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);
                            */
				strDateTime = fmt::format(
					"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
					tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
				);

                    endPublishing = strDateTime;
                }
            }
            
            lastSQLCommand = 
                "insert into MMS_MediaItem (mediaItemKey, workspaceKey, contentProviderKey, title, ingester, "
				"userData, "
                "deliveryFileName, ingestionJobKey, ingestionDate, contentType, startPublishing, endPublishing, "
				"retentionInMinutes, markedAsRemoved, processorMMSForRetention) values ("
                "NULL, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, "
                "convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone), "
                "convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone), "
                "?, 0, NULL)";

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
            preparedStatement->setInt64(queryParameterIndex++, retentionInMinutes);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", contentProviderKey: " + to_string(contentProviderKey)
				+ ", title: " + title
				+ ", ingester: " + ingester
				+ ", userData: " + userData
				+ ", deliveryFileName: " + deliveryFileName
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
				+ ", startPublishing: " + startPublishing
				+ ", endPublishing: " + endPublishing
				+ ", retentionInMinutes: " + to_string(retentionInMinutes)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
        
        int64_t mediaItemKey = getLastInsertId(conn);

		// tags
        {
			string field = "tags";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				Json::Value tagsRoot = parametersRoot[field];
				addTags(conn, mediaItemKey, tagsRoot);
			}
        }

        {
            string uniqueName;
            if (JSONUtils::isMetadataPresent(parametersRoot, "uniqueName"))
                uniqueName = JSONUtils::asString(parametersRoot, "uniqueName", "");

            if (uniqueName != "")
            {
				bool allowUniqueNameOverride = false;
				allowUniqueNameOverride =
					JSONUtils::asBool(parametersRoot, "allowUniqueNameOverride", false);

				manageExternalUniqueName(conn, workspace->_workspaceKey, mediaItemKey,
					allowUniqueNameOverride, uniqueName);
            }
        }

		// cross references
		{
			string field = "crossReference";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
                Json::Value crossReferenceRoot = parametersRoot[field];

				field = "type";
				MMSEngineDBFacade::CrossReferenceType crossReferenceType =
					MMSEngineDBFacade::toCrossReferenceType(JSONUtils::asString(crossReferenceRoot, field, ""));

				int64_t sourceMediaItemKey;
				int64_t targetMediaItemKey;

				if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;

					targetMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfPoster)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::PosterOfVideo;

					targetMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;

					targetMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else
				{
					sourceMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					targetMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}

                Json::Value crossReferenceParametersRoot;
				field = "parameters";
				if (JSONUtils::isMetadataPresent(crossReferenceRoot, field))
				{
					crossReferenceParametersRoot = crossReferenceRoot[field];
				}

				addCrossReference (conn, ingestionJobKey,
						sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
						crossReferenceParametersRoot);
			}
		}

		string externalDeliveryTechnology;
		string externalDeliveryURL;
		{
            string field = "externalDeliveryTechnology";
			externalDeliveryTechnology = JSONUtils::asString(parametersRoot, field, "");

            field = "externalDeliveryURL";
			externalDeliveryURL = JSONUtils::asString(parametersRoot, field, "");
		}

		int64_t physicalItemRetentionInMinutes = -1;
		{
            string field = "physicalItemRetention";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
                string retention = JSONUtils::asString(parametersRoot, field, "1d");
				physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
            }
		}

		int64_t physicalPathKey = -1;
		{
			int64_t sourceIngestionJobKey = -1;

			// 2023-08-10: ogni MediaItem viene generato da un IngestionJob
			//	e la tabella MMS_IngestionJobOutput viene aggiornata con la coppia
			//	(ingestionJobKey, mediaItemKey)
			//	Ci sono casi dove serve associare il mediaItemKey anche ad un altro ingestionJobKey, esempi:
			//	- il LiveRecording inserisce il chunk con il Task Add-Content (IngestionJob) ma serve
			//		anche associare il mediaItemKey con l'ingestionJob del Task Live-Recorder
			//	- il Live-Cut genera un workflow per la creazione del file (Task Cut). Il mediaItemKey, oltre al Task Cut
			//		deve essere associato all'ingestionJob del Live-Cut
			//	Questi due inserimenti vengono eseguiti dal metodo addIngestionJobOutput che riceve in input:
			//	- ingestionJobKey: è il Job che genera il mediaItem
			//	- sourceIngestionJobKey: è il Job iniziale che ha richiesto la generazione del MediaItem (es: Live-Recorder, Live-Cut, ...)

			{
                string field = "userData";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value userDataRoot = parametersRoot[field];

                    field = "mmsData";
                    if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						Json::Value mmsDataRoot = userDataRoot[field];

						field = "ingestionJobKey";
						if (JSONUtils::isMetadataPresent(mmsDataRoot, "liveRecordingChunk"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveRecordingChunk"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "generatedFrame"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["generatedFrame"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "externalTranscoder"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["externalTranscoder"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "liveCut"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveCut"], field, -1);
					}
				}
			}

			physicalPathKey = saveVariantContentMetadata(
				conn,

				workspace->_workspaceKey,
				ingestionJobKey,
				sourceIngestionJobKey,
				mediaItemKey,
				externalReadOnlyStorage,
				externalDeliveryTechnology,
				externalDeliveryURL,
				mediaSourceFileName,
				relativePath,
				mmsPartitionIndexUsed,
				sizeInBytes,
				encodingProfileKey,
				physicalItemRetentionInMinutes,

				// video-audio
				mediaInfoDetails,
				videoTracks,
				audioTracks,

				// image
				imageWidth,
				imageHeight,
				imageFormat,
				imageQuality
			);
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

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
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

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", currentDirLevel1: " + to_string(currentDirLevel1)
					+ ", currentDirLevel2: " + to_string(currentDirLevel2)
					+ ", currentDirLevel3: " + to_string(currentDirLevel3)
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
        
        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
    }
    catch(DeadlockFound& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL (deadlock) exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", exceptionMessage: " + exceptionMessage
			+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", title: " + title
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", exceptionMessage: " + exceptionMessage
			+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", title: " + title
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", title: " + title
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", title: " + title
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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

    return mediaItemKeyAndPhysicalPathKey;
}

int64_t MMSEngineDBFacade::parseRetention(string retention)
{
	int64_t retentionInMinutes = -1;

	string localRetention = StringUtils::trim(retention);

	if (localRetention == "0")
		retentionInMinutes = 0;
	else if (localRetention.length() > 1)
	{
		switch (localRetention.back())
		{
			case 's':   // seconds
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) / 60;

				break;
			case 'm':   // minutes
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1));

				break;
			case 'h':   // hours
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 60;

				break;
			case 'd':   // days
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 1440;

				break;
			case 'M':   // month
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 30);

				break;
			case 'y':   // year
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 365);

				break;
		}
	}

	return retentionInMinutes;
}

void MMSEngineDBFacade::manageExternalUniqueName(
	shared_ptr<MySQLConnection> conn,
	int64_t workspaceKey,
	int64_t mediaItemKey,

	bool allowUniqueNameOverride,
	string uniqueName
)
{
	string      lastSQLCommand;

	try
	{
		if (uniqueName == "")
		{
			/*
			string errorMessage = __FILEREF__ + "uniqueName is empty"
				+ ", uniqueName: " + uniqueName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
			*/

			// delete it if present
			{
				lastSQLCommand = 
					string("delete from MMS_ExternalUniqueName ")
					+ "where workspaceKey = ? and mediaItemKey = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}

			return;
		}

		// look if it is an insert (we do NOT have one) or an update (we already have one)
		string currentUniqueName;
		{
			lastSQLCommand = 
				"select uniqueName from MMS_ExternalUniqueName "
				"where workspaceKey = ? and mediaItemKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKEy: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
				currentUniqueName = resultSet->getString("uniqueName");
		}

		if (currentUniqueName == "")
		{
			// insert

			if (allowUniqueNameOverride)
			{
				lastSQLCommand = 
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = ? and uniqueName = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, uniqueName);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", uniqueName: " + uniqueName
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					int64_t mediaItemKeyOfCurrentUniqueName = resultSet->getInt64("mediaItemKey");

					{
						lastSQLCommand = 
							string("update MMS_ExternalUniqueName ")
							+ "set uniqueName = concat(uniqueName, '-', " + to_string(mediaItemKey)
								+ ", '-', CAST(UNIX_TIMESTAMP(CURTIME(3)) * 1000 as unsigned)) "
							+ "where workspaceKey = ? and uniqueName = ?";

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
						preparedStatement->setString(queryParameterIndex++, uniqueName);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", workspaceKey: " + to_string(workspaceKey)
							+ ", uniqueName: " + uniqueName
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
					}

					{
						lastSQLCommand = 
							string("update MMS_MediaItem ")
							+ "set markedAsRemoved = 1 "
							+ "where workspaceKey = ? and mediaItemKey = ? ";

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
						preparedStatement->setInt64(queryParameterIndex++, mediaItemKeyOfCurrentUniqueName);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", workspaceKey: " + to_string(workspaceKey)
							+ ", mediaItemKeyOfCurrentUniqueName: " + to_string(mediaItemKeyOfCurrentUniqueName)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
					}
				}
			}

			{
				lastSQLCommand = 
					"insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) "
					"values (?, ?, ?)";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
				preparedStatement->setString(queryParameterIndex++, uniqueName);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", uniqueName: " + uniqueName
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}
		}
		else
		{
			// update

			if (allowUniqueNameOverride)
			{
				lastSQLCommand = 
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = ? and uniqueName = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, uniqueName);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", uniqueName: " + uniqueName
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					int64_t mediaItemKeyOfCurrentUniqueName = resultSet->getInt64("mediaItemKey");

					if (mediaItemKeyOfCurrentUniqueName != mediaItemKey)
					{
						{
							lastSQLCommand = 
								string("update MMS_ExternalUniqueName ")
								+ "set uniqueName = concat(uniqueName, '-', " + to_string(mediaItemKey)
									+ ", '-', CAST(UNIX_TIMESTAMP(CURTIME(3)) * 1000 as unsigned)) "
								+ "where workspaceKey = ? and uniqueName = ?";

							shared_ptr<sql::PreparedStatement> preparedStatement (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
							preparedStatement->setString(queryParameterIndex++, uniqueName);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = preparedStatement->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", workspaceKey: " + to_string(workspaceKey)
								+ ", uniqueName: " + uniqueName
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
						}

						{
							lastSQLCommand = 
								string("update MMS_MediaItem ")
								+ "set markedAsRemoved = 1 "
								+ "where workspaceKey = ? and mediaItemKey = ? ";

							shared_ptr<sql::PreparedStatement> preparedStatement (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
							preparedStatement->setInt64(queryParameterIndex++, mediaItemKeyOfCurrentUniqueName);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = preparedStatement->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", workspaceKey: " + to_string(workspaceKey)
								+ ", mediaItemKeyOfCurrentUniqueName: " + to_string(mediaItemKeyOfCurrentUniqueName)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
						}
					}
				}
			}

			{
				lastSQLCommand = 
					string("update MMS_ExternalUniqueName ")
					+ "set uniqueName = ? "
					+ "where workspaceKey = ? and mediaItemKey = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, uniqueName);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}
		}
	}
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", allowUniqueNameOverride: " + to_string(allowUniqueNameOverride)
			+ ", uniqueName: " + uniqueName
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", allowUniqueNameOverride: " + to_string(allowUniqueNameOverride)
			+ ", uniqueName: " + uniqueName
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", allowUniqueNameOverride: " + to_string(allowUniqueNameOverride)
			+ ", uniqueName: " + uniqueName
        );
        
        throw e;
    }
}

void MMSEngineDBFacade::addTags(
	shared_ptr<MySQLConnection> conn,
	int64_t mediaItemKey,
	Json::Value tagsRoot
)
{
	string      lastSQLCommand;

	try
	{
		// generally tagsRoot is an array of strings.
		// In case it comes from a WorkflowAsLibrary, it will be a string
		//	containing a json array of strings
		//	2021-08-29: we added the jsonArray type, so we do not expect anymore a string
		Json::Value localTagsRoot;

		localTagsRoot = tagsRoot;

		for (int tagIndex = 0; tagIndex < localTagsRoot.size(); tagIndex++)
		{
			string tag = JSONUtils::asString(localTagsRoot[tagIndex]);

			tag = StringUtils::trim(tag);

			if (tag == "")
				continue;

			lastSQLCommand = "select count(*) from MMS_Tag where "
				"mediaITemKey = ? and name = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			preparedStatement->setString(queryParameterIndex++, tag);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", tag: " + tag
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                if (resultSet->getInt64(1) == 0)
				{
					lastSQLCommand = 
						"insert into MMS_Tag (mediaItemKey, name) values ("
						"?, ?)";

					shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
					preparedStatement->setString(queryParameterIndex++, tag);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
						preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", tag: " + tag
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
				}
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
		}
	}
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
        );
        
        throw e;
    }
}

void MMSEngineDBFacade::removeTags(
	shared_ptr<MySQLConnection> conn,
	int64_t mediaItemKey
)
{
	string      lastSQLCommand;

	try
	{
		lastSQLCommand = 
			"delete from MMS_Tag where mediaItemKey = ?";

		shared_ptr<sql::PreparedStatement> preparedStatement (
		conn->_sqlConnection->prepareStatement(lastSQLCommand));
		int queryParameterIndex = 1;
		preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		int rowsUpdated = preparedStatement->executeUpdate();
		_logger->info(__FILEREF__ + "@SQL statistics@"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
				chrono::system_clock::now() - startSql).count()) + "@"
		);
		/*
		if (rowsUpdated != 1)
		{
		}
		*/
	}
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
			+ ", mediaItemKey: " + to_string(mediaItemKey)
        );
        
        throw e;
    }
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t sourceIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		tuple<int64_t, long, Json::Value>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

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
        
        physicalPathKey = saveVariantContentMetadata(
            conn,

            workspaceKey,
			ingestionJobKey,
			sourceIngestionJobKey,
            mediaItemKey,
			externalReadOnlyStorage,
			externalDeliveryTechnology,
			externalDeliveryURL,
            encodedFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,
			physicalItemRetentionPeriodInMinutes,

            // video-audio
			mediaInfoDetails,
			videoTracks,
			audioTracks,

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
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(runtime_error& e)
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    catch(exception& e)
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
            catch(sql::SQLException& se)
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
            catch(exception& e)
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
    
    return physicalPathKey;
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        shared_ptr<MySQLConnection> conn,

        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t sourceIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		tuple<int64_t, long, Json::Value>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

		string deliveryInfo;
		{
			if (externalDeliveryTechnology != "" || externalDeliveryURL != "")
			{
				Json::Value deliveryInfoRoot;

				string field = "externalDeliveryTechnology";
				deliveryInfoRoot[field] = externalDeliveryTechnology;

				field = "externalDeliveryURL";
				deliveryInfoRoot[field] = externalDeliveryURL;

                deliveryInfo = JSONUtils::toString(deliveryInfoRoot);                        
			}
		}

        int64_t durationInMilliSeconds;
        long bitRate;
		Json::Value metaDataRoot;
		string metaData;

		tie(durationInMilliSeconds, bitRate, metaDataRoot) = mediaInfoDetails;
		if (metaDataRoot != Json::nullValue)
			metaData = JSONUtils::toString(metaDataRoot);

        {
            int drm = 0;

			_logger->info(__FILEREF__ + "insert into MMS_PhysicalPath"
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", relativePath: " + relativePath
					+ ", encodedFileName: " + encodedFileName
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", deliveryInfo: " + deliveryInfo
					+ ", physicalItemRetentionPeriodInMinutes: " + to_string(physicalItemRetentionPeriodInMinutes)
					);
            lastSQLCommand = 
                "insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, externalReadOnlyStorage, "
				"fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, "
				"durationInMilliSeconds, bitRate, deliveryInfo, metaData, creationDate, retentionInMinutes) values ("
                                             "NULL,            ?,            ?,   ?, "
				"?,        ?,            ?,               ?,           ?, "
				"?,                      ?,       ?,            ?,        NOW(),        ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setInt(queryParameterIndex++, externalReadOnlyStorage ? 1 : 0);
            preparedStatement->setString(queryParameterIndex++, encodedFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, mmsPartitionIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
            if (encodingProfileKey == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
            else
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
			if (durationInMilliSeconds == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, durationInMilliSeconds);
			if (bitRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, bitRate);
            if (deliveryInfo == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryInfo);
            if (metaData == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, metaData);
			if (physicalItemRetentionPeriodInMinutes == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, physicalItemRetentionPeriodInMinutes);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", drm: " + to_string(drm)
				+ ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage ? 1 : 0)
				+ ", encodedFileName: " + encodedFileName
				+ ", relativePath: " + relativePath
				+ ", mmsPartitionIndexUsed: " + to_string(mmsPartitionIndexUsed)
				+ ", sizeInBytes: " + to_string(sizeInBytes)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
				+ ", bitRate: " + to_string(bitRate)
				+ ", deliveryInfo: " + deliveryInfo
				+ ", metaData: " + metaData
				+ ", physicalItemRetentionPeriodInMinutes: " + to_string(physicalItemRetentionPeriodInMinutes)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        physicalPathKey = getLastInsertId(conn);

		if (contentType == ContentType::Video || contentType == ContentType::Audio)
        {
			for (tuple<int, int64_t, string, string, int, int, string, long> videoTrack: videoTracks)
            {
				int videoTrackIndex;
				int64_t videoDurationInMilliSeconds;
				string videoCodecName;
				string videoProfile;
				int videoWidth;
				int videoHeight;
				string videoAvgFrameRate;
				long videoBitRate;

				tie(videoTrackIndex, videoDurationInMilliSeconds, videoCodecName, videoProfile,
					videoWidth, videoHeight, videoAvgFrameRate, videoBitRate) = videoTrack;

                lastSQLCommand = 
                    "insert into MMS_VideoTrack (videoTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
					"codecName, bitRate, profile) values ("
                    "NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
                if (videoTrackIndex == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, videoTrackIndex);
                if (videoDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, videoDurationInMilliSeconds);
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
                if (videoBitRate == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, videoBitRate);
                if (videoProfile == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, videoProfile);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", videoTrackIndex: " + to_string(videoTrackIndex)
					+ ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
					+ ", videoWidth: " + to_string(videoWidth)
					+ ", videoHeight: " + to_string(videoHeight)
					+ ", videoAvgFrameRate: " + videoAvgFrameRate
					+ ", videoCodecName: " + videoCodecName
					+ ", videoBitRate: " + to_string(videoBitRate)
					+ ", videoProfile: " + videoProfile
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }

			for(tuple<int, int64_t, string, long, int, long, string> audioTrack: audioTracks)
            {
				int audioTrackIndex;
				int64_t audioDurationInMilliSeconds;
				string audioCodecName;
				long audioSampleRate;
				int audioChannels;
				long audioBitRate;
				string language;


				tie(audioTrackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate,
					audioChannels, audioBitRate, language) = audioTrack;

                lastSQLCommand = 
                    "insert into MMS_AudioTrack (audioTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language) values ("
                    "NULL, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
                if (audioTrackIndex == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
                else
                    preparedStatement->setInt(queryParameterIndex++, audioTrackIndex);
                if (audioDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, audioDurationInMilliSeconds);
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
                if (language == "")
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
                else
                    preparedStatement->setString(queryParameterIndex++, language);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", audioTrackIndex: " + to_string(audioTrackIndex)
					+ ", audioDurationInMilliSeconds: " + to_string(audioDurationInMilliSeconds)
					+ ", audioCodecName: " + audioCodecName
					+ ", audioBitRate: " + to_string(audioBitRate)
					+ ", audioSampleRate: " + to_string(audioSampleRate)
					+ ", audioChannels: " + to_string(audioChannels)
					+ ", language: " + language
					+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }
		}
		else if (contentType == ContentType::Image)
        {
			lastSQLCommand = 
				"insert into MMS_ImageItemProfile (physicalPathKey, width, height, format, "
				"quality) values ("
				"?, ?, ?, ?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
			conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
			preparedStatement->setInt64(queryParameterIndex++, imageWidth);
			preparedStatement->setInt64(queryParameterIndex++, imageHeight);
			preparedStatement->setString(queryParameterIndex++, imageFormat);
			preparedStatement->setInt(queryParameterIndex++, imageQuality);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", imageWidth: " + to_string(imageWidth)
				+ ", imageHeight: " + to_string(imageHeight)
				+ ", imageFormat: " + imageFormat
				+ ", imageQuality: " + to_string(imageQuality)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
		}
		else
		{
			string errorMessage = __FILEREF__ + "ContentType is wrong"
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);                    
		}            

		addIngestionJobOutput(conn, ingestionJobKey, mediaItemKey, physicalPathKey,
			sourceIngestionJobKey);
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );
        
        throw e;
    }
    
    return physicalPathKey;
}

void MMSEngineDBFacade::addCrossReference (
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
	int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
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

		addCrossReference (conn, ingestionJobKey,
			sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
			crossReferenceParametersRoot);
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(DeadlockFound& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

void MMSEngineDBFacade::addCrossReference (
    shared_ptr<MySQLConnection> conn,
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
{
    
    string      lastSQLCommand;
    
    try
    {
		string crossReferenceParameters;
		{
            crossReferenceParameters = JSONUtils::toString(crossReferenceParametersRoot);
		}

        {
			if (crossReferenceParameters != "")
				lastSQLCommand = 
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values (?, ?, ?, ?)";
			else
				lastSQLCommand = 
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values (?, ?, ?, NULL)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);
			preparedStatement->setString(queryParameterIndex++, toString(crossReferenceType));
			preparedStatement->setInt64(queryParameterIndex++, targetMediaItemKey);
			if (crossReferenceParameters != "")
				preparedStatement->setString(queryParameterIndex++, crossReferenceParameters);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
				+ ", crossReferenceType: " + toString(crossReferenceType)
				+ ", targetMediaItemKey: " + to_string(targetMediaItemKey)
				+ ", crossReferenceParameters: " + crossReferenceParameters
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

		if (exceptionMessage.find("Deadlock found when trying to get lock") !=
			string::npos)
			throw DeadlockFound(exceptionMessage);
		else
			throw se;
    }
    catch(DeadlockFound& e)
    {
        _logger->error(__FILEREF__ + "DeadlockFound"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
}

void MMSEngineDBFacade::removePhysicalPath (
        int64_t physicalPathKey)
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
                "delete from MMS_PhysicalPath where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

void MMSEngineDBFacade::removeMediaItem (
        int64_t mediaItemKey)
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
                "delete from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {
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

Json::Value MMSEngineDBFacade::getTagsList (
	int64_t workspaceKey, int start, int rows,
	int liveRecordingChunk, bool contentTypePresent, ContentType contentType,
	string tagNameFilter, bool fromMaster
)
{
    string      lastSQLCommand;
    Json::Value tagsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getTagsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", liveRecordingChunk: " + to_string(liveRecordingChunk)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
            + ", tagNameFilter: " + tagNameFilter
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;

            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            field = "liveRecordingChunk";
            requestParametersRoot[field] = liveRecordingChunk;

            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }

            if (tagNameFilter != "")
            {
                field = "tagNameFilter";
                requestParametersRoot[field] = tagNameFilter;
            }
            
            field = "requestParameters";
            tagsListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere;
		sqlWhere = string ("where mi.mediaItemKey = t.mediaItemKey and mi.workspaceKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and mi.contentType = ? ");
        if (tagNameFilter != "")
            sqlWhere += ("and t.name like ? ");
		if (liveRecordingChunk == 0)
			sqlWhere += ("and JSON_EXTRACT(mi.userData, '$.mmsData.liveRecordingChunk') is NULL ");
		else if (liveRecordingChunk == 1)
			sqlWhere += ("and JSON_EXTRACT(mi.userData, '$.mmsData.liveRecordingChunk') is not NULL ");
			// sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(mi.userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
        
        Json::Value responseRoot;
        {
			lastSQLCommand = 
				string("select count(distinct t.name) from MMS_MediaItem mi, MMS_Tag t ")
				+ sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            if (tagNameFilter != "")
                preparedStatement->setString(queryParameterIndex++, "%" + tagNameFilter + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", tagNameFilter: " + tagNameFilter
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
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
        
        Json::Value tagsRoot(Json::arrayValue);
        {
        	lastSQLCommand = 
           		string("select distinct t.name from MMS_MediaItem mi, MMS_Tag t ")
            	+ sqlWhere
				+ "order by t.name "
       			+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (contentTypePresent)
                preparedStatement->setString(queryParameterIndex++, toString(contentType));
            if (tagNameFilter != "")
                preparedStatement->setString(queryParameterIndex++, "%" + tagNameFilter + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ (contentTypePresent ? (string(", contentType: ") + toString(contentType)) : "")
				+ ", tagNameFilter: " + tagNameFilter
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                tagsRoot.append(static_cast<string>(resultSet->getString("name")));
            }
        }

        field = "tags";
        responseRoot[field] = tagsRoot;

        field = "response";
        tagsListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {        
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
    
    return tagsListRoot;
}

void MMSEngineDBFacade::updateMediaItem(
		int64_t mediaItemKey,
        string processorMMSForRetention
        )
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "updateMediaItem"
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", processorMMSForRetention: " + processorMMSForRetention
			);

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "update MMS_MediaItem set processorMMSForRetention = ? "
				"where mediaItemKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (processorMMSForRetention == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, processorMMSForRetention);
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMSForRetention: " + processorMMSForRetention
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done"
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", processorMMSForRetention: " + processorMMSForRetention
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
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
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
    catch(exception& e)
    {        
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

