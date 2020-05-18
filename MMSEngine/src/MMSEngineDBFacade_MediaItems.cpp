
#include "MMSEngineDBFacade.h"
#include "JSONUtils.h"
#include "catralibraries/StringUtils.h"


void MMSEngineDBFacade::getExpiredMediaItemKeysCheckingDependencies(
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
                "select workspaceKey, mediaItemKey, ingestionJobKey, retentionInMinutes, title, "
				"DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem where "
                "DATE_ADD(ingestionDate, INTERVAL retentionInMinutes MINUTE) < NOW() "
                "and processorMMSForRetention is null "
                "limit ? offset ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
                int64_t retentionInMinutes = resultSet->getInt64("retentionInMinutes");
                string ingestionDate = resultSet->getString("ingestionDate");
                string title = resultSet->getString("title");
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(conn, ingestionJobKey)
						> 0)
					ingestionDependingOnMediaItemKey = true;
				/*
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
				*/

                if (!ingestionDependingOnMediaItemKey)
                {
                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    pair<shared_ptr<Workspace>,int64_t> workspaceAndMediaItemKey =
                            make_pair(workspace, mediaItemKey);

                    mediaItemKeyToBeRemoved.push_back(workspaceAndMediaItemKey);

                    {
                        lastSQLCommand = 
                            "update MMS_MediaItem set processorMMSForRetention = ? "
							"where mediaItemKey = ? and processorMMSForRetention is null";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
                        + ", title: " + title
                        + ", ingestionDate: " + ingestionDate
                        + ", retentionInMinutes: " + to_string(retentionInMinutes)
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
}

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
	int64_t ingestionJobKey
)
{
    int			dependenciesNumber;
    string      lastSQLCommand;
    shared_ptr<MySQLConnection> conn = nullptr;
    
    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		dependenciesNumber = getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
			conn, ingestionJobKey);

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

			shared_ptr<sql::ResultSet> resultSetDependency (preparedStatementDependency->executeQuery());
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
    
    return dependenciesNumber;
}

Json::Value MMSEngineDBFacade::getMediaItemsList (
        int64_t workspaceKey, int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string ingestionDateOrder,   // "" or "asc" or "desc"
		string jsonOrderBy,
		bool admin
)
{
    string      lastSQLCommand;
	string		temporaryTableName;
    Json::Value mediaItemsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

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
            + ", startAndEndIngestionDatePresent: " + to_string(startAndEndIngestionDatePresent)
            + ", startIngestionDate: " + (startAndEndIngestionDatePresent ? startIngestionDate : "")
            + ", endIngestionDate: " + (startAndEndIngestionDatePresent ? endIngestionDate : "")
            + ", title: " + title
            + ", tagsIn.size(): " + to_string(tagsIn.size())
            + ", tagsNotIn.size(): " + to_string(tagsNotIn.size())
            + ", otherMediaItemsKey.size(): " + to_string(otherMediaItemsKey.size())
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
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        
			// getMediaItemsList_withTagsCheck creates a temporary table
			resultSetAndNumFound = getMediaItemsList_withTagsCheck (
					conn, workspaceKey, temporaryTableName, newMediaItemKey, otherMediaItemsKey, start, rows,
					contentTypePresent, contentType,
					startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
					title, liveRecordingChunk, jsonCondition,
					tagsIn, tagsNotIn,
					ingestionDateOrder,   // "" or "asc" or "desc"
					jsonOrderBy,
					admin
				);
		}
		else
		{
			resultSetAndNumFound = getMediaItemsList_withoutTagsCheck (
					conn, workspaceKey, newMediaItemKey, otherMediaItemsKey, start, rows,
					contentTypePresent, contentType,
					startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
					title, liveRecordingChunk, jsonCondition,
					ingestionDateOrder,   // "" or "asc" or "desc"
					jsonOrderBy,
					admin
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
            while (resultSet->next())
            {
                Json::Value mediaItemRoot;

                int64_t localMediaItemKey = resultSet->getInt64("mediaItemKey");

                field = "mediaItemKey";
                mediaItemRoot[field] = localMediaItemKey;

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
                    lastSQLCommand = 
                        "select uniqueName from MMS_ExternalUniqueName "
						"where workspaceKey = ? and mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementUniqueName (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUniqueName->setInt64(queryParameterIndex++, workspaceKey);
                    preparedStatementUniqueName->setInt64(queryParameterIndex++, localMediaItemKey);
                    shared_ptr<sql::ResultSet> resultSetUniqueName (preparedStatementUniqueName->executeQuery());
                    if (resultSetUniqueName->next())
                    {
                        field = "uniqueName";
                        mediaItemRoot[field] = static_cast<string>(resultSetUniqueName->getString("uniqueName"));
                    }
					else
					{
                        field = "uniqueName";
                        mediaItemRoot[field] = string("");
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

				// CrossReferences
				{
					// if (contentType == ContentType::Video)
					{
						Json::Value mediaItemReferencesRoot(Json::arrayValue);
                    
						{
							lastSQLCommand = 
								"select sourceMediaItemKey, type, parameters from MMS_CrossReference "
								"where targetMediaItemKey = ?";
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";

							shared_ptr<sql::PreparedStatement> preparedStatementCrossReferences (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementCrossReferences->setInt64(queryParameterIndex++, localMediaItemKey);
							shared_ptr<sql::ResultSet> resultSetCrossReferences (preparedStatementCrossReferences->executeQuery());
							while (resultSetCrossReferences->next())
							{
								Json::Value crossReferenceRoot;

								field = "sourceMediaItemKey";
								crossReferenceRoot[field] = resultSetCrossReferences->getInt64("sourceMediaItemKey");

								field = "type";
								crossReferenceRoot[field] = static_cast<string>(resultSetCrossReferences->getString("type"));

								if (!resultSetCrossReferences->isNull("parameters"))
								{
									string crossReferenceParameters =
										resultSetCrossReferences->getString("parameters");
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot;
										try
										{
											Json::CharReaderBuilder builder;
											Json::CharReader* reader = builder.newCharReader();
											string errors;

											bool parsingSuccessful = reader->parse(
												crossReferenceParameters.c_str(),
												crossReferenceParameters.c_str() + crossReferenceParameters.size(), 
												&crossReferenceParametersRoot, &errors);
											delete reader;

											if (!parsingSuccessful)
											{
												string errorMessage = __FILEREF__ + "failed to parse the crossReferenceParameters"
													+ ", errors: " + errors
													+ ", crossReferenceParameters: " + crossReferenceParameters
												;
												_logger->error(errorMessage);

												throw runtime_error(errors);
											}
										}
										catch(runtime_error e)
										{
											string errorMessage = string("crossReferenceParameters json is not well format")
												+ ", crossReferenceParameters: " + crossReferenceParameters
												+ ", e.what(): " + e.what()
											;
											_logger->error(__FILEREF__ + errorMessage);

											throw runtime_error(errorMessage);
										}
										catch(exception e)
										{
											string errorMessage = string("crossReferenceParameters json is not well format")
												+ ", crossReferenceParameters: " + crossReferenceParameters
											;
											_logger->error(__FILEREF__ + errorMessage);

											throw runtime_error(errorMessage);
										}

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.append(crossReferenceRoot);
							}
						}
                    
						{
							lastSQLCommand = 
								"select type, targetMediaItemKey, parameters from MMS_CrossReference "
								"where sourceMediaItemKey = ?";
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";

							shared_ptr<sql::PreparedStatement> preparedStatementCrossReferences (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementCrossReferences->setInt64(queryParameterIndex++, localMediaItemKey);
							shared_ptr<sql::ResultSet> resultSetCrossReferences (preparedStatementCrossReferences->executeQuery());
							while (resultSetCrossReferences->next())
							{
								Json::Value crossReferenceRoot;

								field = "type";
								crossReferenceRoot[field] = static_cast<string>(resultSetCrossReferences->getString("type"));

								field = "targetMediaItemKey";
								crossReferenceRoot[field] = resultSetCrossReferences->getInt64("targetMediaItemKey");

								if (!resultSetCrossReferences->isNull("parameters"))
								{
									string crossReferenceParameters =
										resultSetCrossReferences->getString("parameters");
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot;
										try
										{
											Json::CharReaderBuilder builder;
											Json::CharReader* reader = builder.newCharReader();
											string errors;

											bool parsingSuccessful = reader->parse(
												crossReferenceParameters.c_str(),
												crossReferenceParameters.c_str() + crossReferenceParameters.size(), 
												&crossReferenceParametersRoot, &errors);
											delete reader;

											if (!parsingSuccessful)
											{
												string errorMessage = __FILEREF__ + "failed to parse the crossReferenceParameters"
													+ ", errors: " + errors
													+ ", crossReferenceParameters: " + crossReferenceParameters
												;
												_logger->error(errorMessage);

												throw runtime_error(errors);
											}
										}
										catch(runtime_error e)
										{
											string errorMessage = string("crossReferenceParameters json is not well format")
												+ ", crossReferenceParameters: " + crossReferenceParameters
												+ ", e.what(): " + e.what()
											;
											_logger->error(__FILEREF__ + errorMessage);

											throw runtime_error(errorMessage);
										}
										catch(exception e)
										{
											string errorMessage = string("crossReferenceParameters json is not well format")
												+ ", crossReferenceParameters: " + crossReferenceParameters
											;
											_logger->error(__FILEREF__ + errorMessage);

											throw runtime_error(errorMessage);
										}

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
						Json::Value mediaItemReferencesRoot(Json::arrayValue);
                    
						lastSQLCommand = 
							"select sourceMediaItemKey from MMS_CrossReference "
							"where type = 'imageOfAudio' and targetMediaItemKey = ?";

						shared_ptr<sql::PreparedStatement> preparedStatementCrossReferences (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementCrossReferences->setInt64(queryParameterIndex++, localMediaItemKey);
						shared_ptr<sql::ResultSet> resultSetCrossReferences (preparedStatementCrossReferences->executeQuery());
						while (resultSetCrossReferences->next())
						{
							mediaItemReferencesRoot.append(resultSetCrossReferences->getInt64("sourceMediaItemKey"));
						}
                    
						field = "imagesReferences";
						mediaItemRoot[field] = mediaItemReferencesRoot;
					}
					*/
				}

                {
                    Json::Value mediaItemProfilesRoot(Json::arrayValue);
                    
                    lastSQLCommand = 
                        "select physicalPathKey, durationInMilliSeconds, bitRate, externalReadOnlyStorage, "
						"JSON_UNQUOTE(JSON_EXTRACT(deliveryInfo, '$.externalDeliveryTechnology')) as externalDeliveryTechnology, "
						"JSON_UNQUOTE(JSON_EXTRACT(deliveryInfo, '$.externalDeliveryURL')) as externalDeliveryURL, "
						"fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, "
                        "DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                        "from MMS_PhysicalPath where mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfiles (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfiles->setInt64(queryParameterIndex++, localMediaItemKey);
                    shared_ptr<sql::ResultSet> resultSetProfiles (preparedStatementProfiles->executeQuery());
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
								string fileExtensionLowerCase;
								fileExtensionLowerCase.resize(fileExtension.size());
								transform(fileExtension.begin(), fileExtension.end(), fileExtensionLowerCase.begin(),
									[](unsigned char c){return tolower(c); } );

								if (fileExtensionLowerCase == "mp4" || fileExtensionLowerCase == "mov"
										|| fileExtensionLowerCase == "webm")
									profileRoot[field] =
										MMSEngineDBFacade::toString(MMSEngineDBFacade::DeliveryTechnology::DownloadAndStreaming);
								else if (fileExtensionLowerCase == "m3u8" || fileExtensionLowerCase == "hls")
									profileRoot[field] =
										MMSEngineDBFacade::toString(MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming);
								else
									profileRoot[field] = Json::nullValue;
							}
						}
                        else
						{
							int64_t encodingProfileKey = resultSetProfiles->getInt64("encodingProfileKey");

                            profileRoot[field] = encodingProfileKey;

							string label;
							MMSEngineDBFacade::ContentType contentType;
							MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;

                            tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology>
                                encodingProfileDetails = getEncodingProfileDetailsByKey(workspaceKey, encodingProfileKey);

                            tie(label, contentType, deliveryTechnology) = encodingProfileDetails;

							field = "deliveryTechnology";
                            profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);
						}

                        field = "sizeInBytes";
                        profileRoot[field] = resultSetProfiles->getInt64("sizeInBytes");

                        field = "creationDate";
                        profileRoot[field] = static_cast<string>(resultSetProfiles->getString("creationDate"));

                        if (contentType == ContentType::Video)
                        {
							vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							getVideoDetails(localMediaItemKey, physicalPathKey, videoTracks, audioTracks);
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

							getAudioDetails(localMediaItemKey, physicalPathKey, audioTracks);

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

		if (tagsIn.size() > 0 || tagsNotIn.size() > 0)
		{
			lastSQLCommand = 
				string("drop temporary table ") + temporaryTableName;
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->executeUpdate();
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
            try
            {
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->executeUpdate();
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
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
                _logger->error(__FILEREF__ + "exception"
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
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->executeUpdate();
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
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
                _logger->error(__FILEREF__ + "exception"
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
				if ((tagsIn.size() > 0 || tagsNotIn.size() > 0) && temporaryTableName != "")
				{
					lastSQLCommand = 
						string("drop temporary table IF EXISTS ") + temporaryTableName;
					shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->executeUpdate();
				}

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception"
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
                _logger->error(__FILEREF__ + "exception"
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
    
    return mediaItemsListRoot;
}

pair<shared_ptr<sql::ResultSet>, int64_t> MMSEngineDBFacade::getMediaItemsList_withoutTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, string jsonCondition,
        string ingestionDateOrder,   // "" or "asc" or "desc"
		string jsonOrderBy,
		bool admin
)
{
    string						lastSQLCommand;
    
    try
    {
        string sqlWhere;
		sqlWhere = string ("where mi.workspaceKey = ? ");
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
        if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
			{
				sqlWhere += ("and (JSON_EXTRACT(userData, '$.mmsData.dataType') is NULL ");
				sqlWhere += ("OR JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.dataType')) not like 'liveRecordingChunk%') ");
			}
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
		}
        if (jsonCondition != "")
            sqlWhere += ("and " + jsonCondition);
        
		int64_t numFound;
        {
			lastSQLCommand = 
				string("select count(*) from MMS_MediaItem mi ")
				+ sqlWhere;

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
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            if (title != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet(preparedStatement->executeQuery());


			return make_pair(resultSet, numFound);
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

pair<shared_ptr<sql::ResultSet>, int64_t> MMSEngineDBFacade::getMediaItemsList_withTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, string temporaryTableName,
		int64_t mediaItemKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk, string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string ingestionDateOrder,   // "" or "asc" or "desc"
		string jsonOrderBy,
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
			if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
			{
				if (liveRecordingChunk == 0)
				{
					sqlWhere += ("and (JSON_EXTRACT(mi.userData, '$.mmsData.dataType') is NULL ");
					sqlWhere += ("OR JSON_UNQUOTE(JSON_EXTRACT(mi.userData, '$.mmsData.dataType')) not like 'liveRecordingChunk%') ");
				}
				else if (liveRecordingChunk == 1)
					sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(mi.userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
			}
			if (jsonCondition != "")
				sqlWhere += ("and " + jsonCondition);
        
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
			if (startAndEndIngestionDatePresent)
			{
				preparedStatement->setString(queryParameterIndex++, startIngestionDate);
				preparedStatement->setString(queryParameterIndex++, endIngestionDate);
			}
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			preparedStatement->executeUpdate();

			createdTemporaryTable = true;
		}

		int64_t numFound;
		{
			lastSQLCommand = 
				string("select count(*) from ") + temporaryTableName + " f where "
				+ tagsGroupCondition;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

          	lastSQLCommand = 
           		string("select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
           			"DATE_FORMAT(convert_tz(mi.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
           			"DATE_FORMAT(convert_tz(mi.startPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startPublishing, "
           			"DATE_FORMAT(convert_tz(mi.endPublishing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endPublishing, "
           			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi, " + temporaryTableName + " f ")
           			+ sqlWhere
           			+ orderByCondition
           			+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());


			return make_pair(resultSet, numFound);
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

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
    int64_t referenceMediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having 'ts' file format and bigger size in case there are more than one)
	int64_t encodingProfileKey,
	bool warningIfMissing)
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
			tuple<int64_t, int, string, string, int64_t, bool> sourcePhysicalPathDetails =
				getSourcePhysicalPath(referenceMediaItemKey, warningIfMissing);
			tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore) = sourcePhysicalPathDetails;
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

    return physicalPathKey;
}


tuple<int64_t, int, string, string, int64_t, bool> MMSEngineDBFacade::getSourcePhysicalPath(
    int64_t mediaItemKey, bool warningIfMissing)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t physicalPathKey = -1;
        int mmsPartitionNumber;
        bool externalReadOnlyStorage;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
        {
			lastSQLCommand = string("") +
				"select physicalPathKey, sizeInBytes, fileName, relativePath, partitionNumber, externalReadOnlyStorage "
				"from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			int64_t maxSizeInBytes = -1;
			string selectedFileFormat;

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                int64_t localSizeInBytes = resultSet->getInt64("sizeInBytes");

                string localFileName = resultSet->getString("fileName");
				string localFileFormat;
				size_t extensionIndex = fileName.find_last_of(".");
				if (extensionIndex != string::npos)
					localFileFormat = fileName.substr(extensionIndex + 1);

				if (maxSizeInBytes != -1)
				{
					// this is the second or third... physicalPath
					// we are fore sure in the scenario encodingProfileKey == -1
					// So, in case we have more than one "source" physicalPath, we will select the 'ts' one
					// We prefer 'ts' because is easy and safe do activities like cut or concat
					if (selectedFileFormat == "ts")
					{
						if (localFileFormat == "ts")
						{
							if (localSizeInBytes <= maxSizeInBytes)
								continue;
						}
						else
						{
							continue;
						}
					}
					else
					{
						if (localSizeInBytes <= maxSizeInBytes)
							continue;
					}
				}

                physicalPathKey = resultSet->getInt64("physicalPathKey");
                externalReadOnlyStorage = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getInt64("sizeInBytes");

				maxSizeInBytes = localSizeInBytes;
				selectedFileFormat = localFileFormat;
            }

			if (maxSizeInBytes == -1)
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
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(physicalPathKey, mmsPartitionNumber,
				relativePath, fileName, sizeInBytes, externalReadOnlyStorage);
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

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
        int64_t workspaceKey, 
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel,
		bool warningIfMissing)
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
    
    return physicalPathKey;
}

tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
	MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
		contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

    try
    {
        conn = _connectionPool->borrow();	
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
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
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

tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string>
	MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(
	int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t, string>
		mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, mi.ingestionJobKey, p.fileName, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
                "where mi.workspaceKey = ? and mi.mediaItemKey = p.mediaItemKey and p.physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
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

                string fileName = resultSet->getString("fileName");

                string ingestionDate;
                if (!resultSet->isNull("ingestionDate"))
                    ingestionDate = resultSet->getString("ingestionDate");

                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

                mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName =
					make_tuple(mediaItemKey, contentType, title, userData, ingestionDate,
						ingestionJobKey, fileName);                
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
		conn = nullptr;
        
        return mediaItemKeyContentTypeTitleUserDataIngestionDateIngestionJobKeyAndFileName;
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

void MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey(
	int64_t workspaceKey, int64_t referenceIngestionJobKey, 
	int maxLastMediaItemsToBeReturned,
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

		IngestionType ingestionType;
        {
			lastSQLCommand =
				"select ingestionType "
				"from MMS_IngestionJob "
				"where ingestionJobKey = ? ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceIngestionJobKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
				orderBy = "order by JSON_EXTRACT(mi.userData, '$.mmsData.utcChunkStartTime') desc ";
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

pair<int64_t,MMSEngineDBFacade::ContentType>
	MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
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
                "select mi.mediaItemKey, mi.contentType "
				"from MMS_MediaItem mi, MMS_ExternalUniqueName eun "
                "where mi.mediaItemKey = eun.mediaItemKey "
                "and eun.workspaceKey = ? and eun.uniqueName = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, referenceUniqueName);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
    catch(MediaItemKeyNotFound e)
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
    
    return mediaItemKeyAndContentType;
}

int64_t MMSEngineDBFacade::getMediaDurationInMilliseconds(
	int64_t mediaItemKey, int64_t physicalPathKey)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
	int64_t durationInMilliSeconds;

    try
    {
        conn = _connectionPool->borrow();	
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(MediaItemKeyNotFound mnf)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw mnf;
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

	return durationInMilliSeconds;
}

void MMSEngineDBFacade::getVideoDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>>& videoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(MediaItemKeyNotFound mnf)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw mnf;
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

void MMSEngineDBFacade::getAudioDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
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
        
		audioTracks.clear();

        {
            lastSQLCommand = 
                "select audioTrackKey, trackIndex, durationInMilliSeconds, "
				"codecName, bitRate, sampleRate, channels, language "
                "from MMS_AudioTrack where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
		conn = nullptr;
        
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

pair<int64_t,int64_t> MMSEngineDBFacade::saveSourceContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,        
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
		bool externalReadOnlyStorage,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,

        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
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

	string title = "";
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
            
            if (JSONUtils::isMetadataPresent(parametersRoot, "ContentProviderName"))
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
            string ingester = "";
            string userData = "";
            string deliveryFileName = "";
            string sContentType;
            int retentionInMinutes = _contentRetentionInMinutesDefaultValue;
            // string encodingProfilesSet;

            string field = "Title";
            title = parametersRoot.get(field, "").asString();
            
            field = "Ingester";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
                ingester = parametersRoot.get(field, "").asString();

            field = "UserData";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
				// 2020-03-15: when it is set by the GUI it arrive here as a string
				if ((parametersRoot[field]).type() == Json::stringValue)
				{
					userData = parametersRoot.get(field, "").asString();

					// _logger->error(__FILEREF__ + "STRING AAAAAAAAAAA"
					// 	+ ", userData: " + userData
					// );
				}
				else
				{
					Json::StreamWriterBuilder wbuilder;

					userData = Json::writeString(wbuilder, parametersRoot[field]);                        

					// _logger->error(__FILEREF__ + "NO STRING AAAAAAAAAAA"
					// 	+ ", userData: " + userData
					// );
				}
            }

            field = "DeliveryFileName";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
                deliveryFileName = parametersRoot.get(field, "").asString();

            field = "Retention";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value publishingRoot = parametersRoot[field];

                    field = "startPublishing";
                    if (JSONUtils::isMetadataPresent(publishingRoot, field))
                        startPublishing = publishingRoot.get(field, "").asString();

                    field = "endPublishing";
                    if (JSONUtils::isMetadataPresent(publishingRoot, field))
                        endPublishing = publishingRoot.get(field, "").asString();
                }
                
                if (startPublishing == "NOW")
                {
                    tm          tmDateTime;
                    char        strDateTime [64];

                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    time_t utcTime = chrono::system_clock::to_time_t(now);

					// 2019-03-31: in case startPublishing is wrong, check how gmtime is used in MMSEngineDBFacade_Lock.cpp
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

					// 2019-03-31: in case startPublishing is wrong, check how gmtime is used in MMSEngineDBFacade_Lock.cpp
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
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
				stringstream ssTagsCommaSeparated (parametersRoot.get(field, "").asString());
				while (ssTagsCommaSeparated.good())
				{
					string tag;
					getline(ssTagsCommaSeparated, tag, ',');

					tag = StringUtils::trim(tag);

					if (tag == "")
						continue;

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
            if (JSONUtils::isMetadataPresent(parametersRoot, "UniqueName"))
                uniqueName = parametersRoot.get("UniqueName", "").asString();

            if (uniqueName != "")
            {
				bool allowUniqueNameOverride = false;
				if (JSONUtils::isMetadataPresent(parametersRoot, "AllowUniqueNameOverride"))
					allowUniqueNameOverride =
						JSONUtils::asBool(parametersRoot, "AllowUniqueNameOverride", false);

				addExternalUniqueName(conn, workspace->_workspaceKey, mediaItemKey,
					allowUniqueNameOverride, uniqueName);
            }
        }

		// cross references
		{
			string field = "CrossReference";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
                Json::Value crossReferenceRoot = parametersRoot[field];

				field = "Type";
				MMSEngineDBFacade::CrossReferenceType crossReferenceType =
					MMSEngineDBFacade::toCrossReferenceType(crossReferenceRoot.get(field, "").asString());

				int64_t sourceMediaItemKey;
				int64_t targetMediaItemKey;

				if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;

					targetMediaItemKey = mediaItemKey;

					field = "MediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;

					targetMediaItemKey = mediaItemKey;

					field = "MediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else
				{
					sourceMediaItemKey = mediaItemKey;

					field = "MediaItemKey";
					targetMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}

                Json::Value crossReferenceParametersRoot;
				field = "Parameters";
				if (JSONUtils::isMetadataPresent(crossReferenceRoot, field))
				{
					crossReferenceParametersRoot = crossReferenceRoot[field];
				}

				addCrossReference (conn, sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
						crossReferenceParametersRoot);
			}
		}

        /*
        // territories
        {
            string field = "Territories";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
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
                    if (JSONUtils::isMetadataPresent(territories, territoryName))
                    {
                        Json::Value territory = territories[territoryName];
                        
                        field = "startPublishing";
                        if (JSONUtils::isMetadataPresent(territory, field))
                            startPublishing = territory.get(field, "XXX").asString();

                        field = "endPublishing";
                        if (JSONUtils::isMetadataPresent(territory, field))
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

		string externalDeliveryTechnology;
		string externalDeliveryURL;
		{
            string field = "ExternalDeliveryTechnology";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				externalDeliveryTechnology = parametersRoot.get(field, "").asString();

            field = "ExternalDeliveryURL";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
				externalDeliveryURL = parametersRoot.get(field, "").asString();
		}

		int64_t physicalPathKey = -1;
		{
			int64_t liveRecordingIngestionJobKey = -1;

			// in case of a content generated by a live recording, we have to insert into MMS_IngestionJobOutput
			// of the live recording ingestion job
			{
                string field = "UserData";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value userDataRoot = parametersRoot[field];

                    field = "mmsData";
                    if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						Json::Value mmsDataRoot = userDataRoot[field];

						field = "dataType";
						if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
						{
							string dataType = mmsDataRoot.get(field, "").asString();
							if (dataType == "liveRecordingChunk")
							{
								field = "ingestionJobKey";
								if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
								{
									liveRecordingIngestionJobKey = JSONUtils::asInt64(mmsDataRoot, field, 0);

									/*
									addIngestionJobOutput(conn, liveRecordingIngestionJobKey, mediaItemKey,
											physicalPathKey);
									{
										lastSQLCommand = 
											"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
											"?, ?, ?)";

										shared_ptr<sql::PreparedStatement> preparedStatement (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
										int queryParameterIndex = 1;
										preparedStatement->setInt64(queryParameterIndex++, liveRecordingIngestionJobKey);
										preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
										preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

										int rowsUpdated = preparedStatement->executeUpdate();

										_logger->info(__FILEREF__ + "insert into MMS_IngestionJobOutput"
											+ ", liveRecordingIngestionJobKey: " + to_string(liveRecordingIngestionJobKey)
											+ ", mediaItemKey: " + to_string(mediaItemKey)
											+ ", physicalPathKey: " + to_string(physicalPathKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
										);
									}
									*/
								}
							}
						}
					}
				}
			}

			int64_t encodingProfileKey = -1;
			physicalPathKey = saveVariantContentMetadata(
				conn,
                
				workspace->_workspaceKey,
				ingestionJobKey,
				liveRecordingIngestionJobKey,
				mediaItemKey,
				externalReadOnlyStorage,
				externalDeliveryTechnology,
				externalDeliveryURL,
				mediaSourceFileName,
				relativePath,
				mmsPartitionIndexUsed,
				sizeInBytes,
				encodingProfileKey,

				// video-audio
				mediaInfoDetails,
				videoTracks,
				audioTracks,
				/*
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
				*/

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
			/*
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

			// in case of a content generated by a live recording, we have to insert into MMS_IngestionJobOutput
			// of the live recording ingestion job
			{
                string field = "UserData";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value userDataRoot = parametersRoot[field];

                    field = "mmsData";
                    if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						Json::Value mmsDataRoot = userDataRoot[field];

						field = "dataType";
						if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
						{
							string dataType = mmsDataRoot.get(field, "").asString();
							if (dataType == "liveRecordingChunk")
							{
								field = "ingestionJobKey";
								if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
								{
									int64_t liveRecordingIngestionJobKey = mmsDataRoot.get(field, "").asInt64();

									{
										lastSQLCommand = 
											"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
											"?, ?, ?)";

										shared_ptr<sql::PreparedStatement> preparedStatement (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
										int queryParameterIndex = 1;
										preparedStatement->setInt64(queryParameterIndex++, liveRecordingIngestionJobKey);
										preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
										preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

										int rowsUpdated = preparedStatement->executeUpdate();

										_logger->info(__FILEREF__ + "insert into MMS_IngestionJobOutput"
											+ ", liveRecordingIngestionJobKey: " + to_string(liveRecordingIngestionJobKey)
											+ ", mediaItemKey: " + to_string(mediaItemKey)
											+ ", physicalPathKey: " + to_string(physicalPathKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
										);
									}
								}
							}
						}
					}
				}
			}
			*/

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
		conn = nullptr;
        
        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
    }
    catch(sql::SQLException se)
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

    return mediaItemKeyAndPhysicalPathKey;
}

void MMSEngineDBFacade::addExternalUniqueName(
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
			string errorMessage = __FILEREF__ + "UniqueName is empty"
				+ ", uniqueName: " + uniqueName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		if (allowUniqueNameOverride)
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

			int rowsUpdated = preparedStatement->executeUpdate();
		}

		lastSQLCommand = 
			"insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) "
			"values (?, ?, ?)";

		shared_ptr<sql::PreparedStatement> preparedStatement (
			conn->_sqlConnection->prepareStatement(lastSQLCommand));
		int queryParameterIndex = 1;
		preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
		preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
		preparedStatement->setString(queryParameterIndex++, uniqueName);

		preparedStatement->executeUpdate();
	}
    catch(sql::SQLException se)
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
    catch(runtime_error e)
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
    catch(exception e)
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

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t liveRecordingIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,
		/*
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
		*/

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
        
        physicalPathKey = saveVariantContentMetadata(
            conn,

            workspaceKey,
			ingestionJobKey,
			liveRecordingIngestionJobKey,
            mediaItemKey,
			externalReadOnlyStorage,
			externalDeliveryTechnology,
			externalDeliveryURL,
            encodedFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,

            // video-audio
			mediaInfoDetails,
			videoTracks,
			audioTracks,
			/*
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
			*/

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
    
    return physicalPathKey;
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        shared_ptr<MySQLConnection> conn,

        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t liveRecordingIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,
		/*
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
		*/

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

		string deliveryInfo;
		{
			if (externalDeliveryTechnology != "" || externalDeliveryURL != "")
			{
				Json::Value deliveryInfoRoot;

				string field = "externalDeliveryTechnology";
				deliveryInfoRoot[field] = externalDeliveryTechnology;

				field = "externalDeliveryURL";
				deliveryInfoRoot[field] = externalDeliveryURL;

                Json::StreamWriterBuilder wbuilder;
                deliveryInfo = Json::writeString(wbuilder, deliveryInfoRoot);                        
			}
		}

        int64_t durationInMilliSeconds;
        long bitRate;

		tie(durationInMilliSeconds, bitRate) = mediaInfoDetails;

        {
            int drm = 0;

			_logger->info(__FILEREF__ + "insert into MMS_PhysicalPath"
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", relativePath: " + relativePath
					+ ", encodedFileName: " + encodedFileName
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", deliveryInfo: " + deliveryInfo
					);
            lastSQLCommand = 
                "insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, externalReadOnlyStorage, "
				"fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, "
				"durationInMilliSeconds, bitRate, deliveryInfo, creationDate) values ("
                "NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW())";

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

            preparedStatement->executeUpdate();
        }

        physicalPathKey = getLastInsertId(conn);

        {
			/*
            if (contentType == ContentType::Video && videoTracks.size() > 0 && audioTracks.size() > 0)
            {
				int videoTrackIndex;
				int64_t videoDurationInMilliSeconds;
				string videoCodecName;
				string videoProfile;
				int videoWidth;
				int videoHeight;
				string videoAvgFrameRate;
				long videoBitRate;

				int audioTrackIndex;
				int64_t audioDurationInMilliSeconds;
				string audioCodecName;
				long audioSampleRate;
				int audioChannels;
				long audioBitRate;


				tuple<int, int64_t, string, string, int, int, string, long> videoTrack = videoTracks[0];
				tie(videoTrackIndex, videoDurationInMilliSeconds, videoCodecName, videoProfile,
					videoWidth, videoHeight, videoAvgFrameRate, videoBitRate) = videoTrack;

				tuple<int, int64_t, string, long, int, long, string> audioTrack = audioTracks[0];
				tie(audioTrackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate,
					audioChannels, audioBitRate, ignore) = audioTrack;

                lastSQLCommand = 
                    "insert into MMS_VideoItemProfile (physicalPathKey, durationInMilliSeconds, bitRate, "
					"width, height, avgFrameRate, videoCodecName, videoProfile, videoBitRate, "
                    "audioCodecName, audioSampleRate, audioChannels, audioBitRate) values ("
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
            else if (contentType == ContentType::Audio && audioTracks.size() > 0)
            {
				int audioTrackIndex;
				int64_t audioDurationInMilliSeconds;
				string audioCodecName;
				long audioSampleRate;
				int audioChannels;
				long audioBitRate;


				tuple<int, int64_t, string, long, int, long, string> audioTrack = audioTracks[0];
				tie(audioTrackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate,
					audioChannels, audioBitRate, ignore) = audioTrack;

                lastSQLCommand = 
                    "insert into MMS_AudioItemProfile (physicalPathKey, durationInMilliSeconds, "
					"codecName, bitRate, sampleRate, channels) values ("
                    "?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
			*/
			/*
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
			*/
        }

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

                preparedStatement->executeUpdate();
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

                preparedStatement->executeUpdate();
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

		addIngestionJobOutput(conn, ingestionJobKey, mediaItemKey, physicalPathKey,
			liveRecordingIngestionJobKey);
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

void MMSEngineDBFacade::updateLiveRecorderVirtualVOD (
	int64_t workspaceKey,
	string liveRecorderVirtualVODUniqueName,
	int64_t mediaItemKey,
	int64_t physicalPathKey,

	int newRetentionInMinutes,

	int64_t firstUtcChunkStartTime,
	string sFirstUtcChunkStartTime,
	int64_t lastUtcChunkEndTime,
	string sLastUtcChunkEndTime,
	string title,
	int64_t durationInMilliSeconds,
	long bitRate,
	unsigned long long sizeInBytes,

	vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks
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
            lastSQLCommand = 
				"update MMS_PhysicalPath "
				"set sizeInBytes = ?, "
				"durationInMilliSeconds = ?, "
				"bitRate = ? "
				"where physicalPathKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
			preparedStatement->setInt64(queryParameterIndex++, durationInMilliSeconds);
			preparedStatement->setInt64(queryParameterIndex++, bitRate);
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "updateLiveRecorderVirtualVOD (sizeInBytes, durationInMilliSeconds, bitRate)"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", sizeInBytes: " + to_string(sizeInBytes)
				+ ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
				+ ", bitRate: " + to_string(bitRate)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }

        {
			// before it was liveRecordingChunk to avoid to be seen in the MediaItems view
			string newDataType = "liveRecordingVOD";
            lastSQLCommand = 
				"update MMS_MediaItem "
				"set title = ?, retentionInMinutes = ?, "
				"userData = JSON_SET(userData, '$.mmsData.dataType', ?), "
				"userData = JSON_SET(userData, '$.mmsData.firstUtcChunkStartTime', ?), "
				"userData = JSON_SET(userData, '$.mmsData.firstUtcChunkStartTime_str', ?), "
				"userData = JSON_SET(userData, '$.mmsData.lastUtcChunkEndTime', ?), "
				"userData = JSON_SET(userData, '$.mmsData.lastUtcChunkEndTime_str', ?) "
				"where mediaItemKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, title);
            preparedStatement->setInt(queryParameterIndex++, newRetentionInMinutes);
            preparedStatement->setString(queryParameterIndex++, newDataType);
            preparedStatement->setInt64(queryParameterIndex++, firstUtcChunkStartTime);
            preparedStatement->setString(queryParameterIndex++, sFirstUtcChunkStartTime);
            preparedStatement->setInt64(queryParameterIndex++, lastUtcChunkEndTime);
            preparedStatement->setString(queryParameterIndex++, sLastUtcChunkEndTime);
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "updateLiveRecorderVirtualVOD (title, retentionInMinutes, dataType)"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", title: " + title
				+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
				+ ", newDataType: " + newDataType
				+ ", rowsUpdated: " + to_string(rowsUpdated)
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }

		{
			// Previous liveRecordingVOD was set to liveRecordingChunk, so it will
			// not be visible into the MediaItems view
			{
				string previousDataType = "liveRecordingVOD";
				string newDataType = "liveRecordingChunk_VOD";
				lastSQLCommand = 
					"update MMS_MediaItem "
					"set userData = JSON_SET(userData, '$.mmsData.dataType', ?) "
					"where JSON_EXTRACT(userData, '$.mmsData.dataType') = ? and "
					"mediaItemKey in ( "
						"select mediaItemKey from MMS_ExternalUniqueName "
						"where workspaceKey = ? and "
						"uniqueName like '" + liveRecorderVirtualVODUniqueName + "-%' "
					")" ;

				shared_ptr<sql::PreparedStatement> preparedStatement(
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, newDataType);
				preparedStatement->setString(queryParameterIndex++, previousDataType);
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "updateLiveRecorderVirtualVOD (dataType)"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", liveRecorderVirtualVODUniqueName: " + liveRecorderVirtualVODUniqueName
					+ ", newDataType: " + newDataType
					+ ", rowsUpdated: " + to_string(rowsUpdated)
				);
			}

			// 2020-04-28: uniqueName is changed so that next check unique name is not found and
			// a new MediaItem is created.
			{
				lastSQLCommand = 
					"update MMS_ExternalUniqueName "
					"set uniqueName = concat(uniqueName, '-', CAST(UNIX_TIMESTAMP(CURTIME(3)) * 1000 as unsigned)) "
					"where workspaceKey = ? and uniqueName = ?";
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, liveRecorderVirtualVODUniqueName);

				int rowsUpdated = preparedStatement->executeUpdate();
			}
		}

		for (tuple<int, int64_t, string, string, int, int, string, long> videoTrack: videoTracks)
        {
			int videoTrackIndex;
			int64_t videoDurationInMilliSeconds;
			// string videoCodecName;
			// string videoProfile;
			// int videoWidth;
			// int videoHeight;
			string videoAvgFrameRate;
			long videoBitRate;

			tie(videoTrackIndex, videoDurationInMilliSeconds, ignore, ignore,
				ignore, ignore, videoAvgFrameRate, videoBitRate) = videoTrack;

            lastSQLCommand = 
				"update MMS_VideoTrack "
				"set durationInMilliSeconds = ?, "
				"avgFrameRate = ?, "
				"bitRate = ? "
				"where physicalPathKey = ? and trackIndex = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (videoDurationInMilliSeconds == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, videoDurationInMilliSeconds);
			if (videoAvgFrameRate == "")
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
			else
				preparedStatement->setString(queryParameterIndex++, videoAvgFrameRate);
			if (videoBitRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, videoBitRate);
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
			preparedStatement->setInt64(queryParameterIndex++, videoTrackIndex);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
                        + ", videoTrackIndex: " + to_string(videoTrackIndex)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }

		for(tuple<int, int64_t, string, long, int, long, string> audioTrack: audioTracks)
		{
			int audioTrackIndex;
			int64_t audioDurationInMilliSeconds;
			// string audioCodecName;
			long audioSampleRate;
			// int audioChannels;
			long audioBitRate;
			// string language;


			tie(audioTrackIndex, audioDurationInMilliSeconds, ignore, audioSampleRate,
				ignore, audioBitRate, ignore) = audioTrack;

            lastSQLCommand = 
				"update MMS_AudioTrack "
				"set durationInMilliSeconds = ?, "
				"bitRate = ?, "
				"sampleRate = ? "
				"where physicalPathKey = ? and trackIndex = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (audioDurationInMilliSeconds == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, audioDurationInMilliSeconds);
			if (audioBitRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, audioBitRate);
			if (audioSampleRate == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, audioSampleRate);
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
			preparedStatement->setInt64(queryParameterIndex++, audioTrackIndex);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
                        + ", audioTrackIndex: " + to_string(audioTrackIndex)
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

void MMSEngineDBFacade::addCrossReference (
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		addCrossReference (conn, sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
				crossReferenceParametersRoot);
        
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

void MMSEngineDBFacade::addCrossReference (
    shared_ptr<MySQLConnection> conn,
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
{
    
    string      lastSQLCommand;
    
    try
    {
		string crossReferenceParameters;
		{
			Json::StreamWriterBuilder wbuilder;
            crossReferenceParameters = Json::writeString(wbuilder, crossReferenceParametersRoot);
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

			preparedStatement->executeUpdate();
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

Json::Value MMSEngineDBFacade::getTagsList (
        int64_t workspaceKey, int start, int rows,
        bool contentTypePresent, ContentType contentType
)
{
    string      lastSQLCommand;
    Json::Value tagsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getTagsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
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
            
            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }
            
            field = "requestParameters";
            tagsListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere;
		sqlWhere = string ("where mi.mediaItemKey = t.mediaItemKey and mi.workspaceKey = ? ");
        if (contentTypePresent)
            sqlWhere += ("and mi.contentType = ? ");
        
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
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

    try
    {
        conn = _connectionPool->borrow();	
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

            int rowsUpdated = preparedStatement->executeUpdate();
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

