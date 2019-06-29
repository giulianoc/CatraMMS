
#include "MMSEngineDBFacade.h"

string& ltrim(string& s)
{
    auto it = find_if(s.begin(), s.end(),
        [](char c)
        {
            return !isspace<char>(c, locale::classic());
        });
    s.erase(s.begin(), it);

    return s;
}

string& rtrim(string& s)                                                                                      
{                                                                                                             
    auto it = find_if(s.rbegin(), s.rend(),                                                                   
        [](char c)                                                                                            
        {                                                                                                     
            return !isspace<char>(c, locale::classic());                                                      
        });                                                                                                   
    s.erase(it.base(), s.end());                                                                              
                                                                                                              
    return s;                                                                                                 
}                                                                                                             
                                                                                                              
string& trim(string& s)                                                                                       
{                                                                                                             
    return ltrim(rtrim(s));                                                                                   
}

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
                "select workspaceKey, mediaItemKey, ingestionJobKey from MMS_MediaItem where "
                "DATE_ADD(ingestionDate, INTERVAL retentionInMinutes MINUTE) < NOW() "
                "and processorMMSForRetention is null "
                "limit ? offset ?";
                // "limit ? offset ? for update";
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
        int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey,
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
    Json::Value mediaItemsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getMediaItemsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", mediaItemKey: " + to_string(mediaItemKey)
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
        if (mediaItemKey == -1 && physicalPathKey != -1)
        {
            lastSQLCommand = 
                string("select mediaItemKey from MMS_PhysicalPath where physicalPathKey = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

		pair<shared_ptr<sql::ResultSet>, int64_t>	resultSetAndNumFound;
		if (tagsIn.size() > 0 || tagsNotIn.size() > 0)
		{
			resultSetAndNumFound = getMediaItemsList_withTagsCheck (
					conn, workspaceKey, newMediaItemKey, start, rows,
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
					conn, workspaceKey, newMediaItemKey, start, rows,
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
                        "select uniqueName from MMS_ExternalUniqueName where workspaceKey = ? and mediaItemKey = ?";

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
					if (contentType == ContentType::Video)
					{
						Json::Value mediaItemReferencesRoot(Json::arrayValue);
                    
						lastSQLCommand = 
							"select sourceMediaItemKey from MMS_CrossReference "
							"where type = 'imageOfVideo' and targetMediaItemKey = ?";

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
				}

                {
                    Json::Value mediaItemProfilesRoot(Json::arrayValue);
                    
                    lastSQLCommand = 
                        "select physicalPathKey, fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, "
                        "DATE_FORMAT(convert_tz(creationDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as creationDate "
                        "from MMS_PhysicalPath where mediaItemKey = ?";

                    shared_ptr<sql::PreparedStatement> preparedStatementProfiles (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementProfiles->setInt64(queryParameterIndex++, localMediaItemKey);
                    shared_ptr<sql::ResultSet> resultSetProfiles (preparedStatementProfiles->executeQuery());
                    while (resultSetProfiles->next())
                    {
                        Json::Value profileRoot;
                        
                        int64_t physicalPathKey = resultSetProfiles->getInt64("physicalPathKey");

                        field = "physicalPathKey";
                        profileRoot[field] = physicalPathKey;


                        field = "fileFormat";
                        string fileName = resultSetProfiles->getString("fileName");
                        size_t extensionIndex = fileName.find_last_of(".");
                        if (extensionIndex == string::npos)
                        {
                            profileRoot[field] = Json::nullValue;
                        }
                        else
                            profileRoot[field] = fileName.substr(extensionIndex + 1);
                        
						if (admin)
						{
							field = "partitionNumber";
							profileRoot[field] = resultSetProfiles->getInt("partitionNumber");

							field = "relativePath";
							profileRoot[field] = static_cast<string>(resultSetProfiles->getString("relativePath"));

							field = "fileName";
							profileRoot[field] = fileName;
						}

                        field = "encodingProfileKey";
                        if (resultSetProfiles->isNull("encodingProfileKey"))
                            profileRoot[field] = Json::nullValue;
                        else
                            profileRoot[field] = resultSetProfiles->getInt64("encodingProfileKey");

                        field = "sizeInBytes";
                        profileRoot[field] = resultSetProfiles->getInt64("sizeInBytes");

                        field = "creationDate";
                        profileRoot[field] = static_cast<string>(resultSetProfiles->getString("creationDate"));

                        if (contentType == ContentType::Video)
                        {
                            int64_t durationInMilliSeconds;
                            int videoWidth;
                            int videoHeight;
                            long bitRate;
                            string videoCodecName;
                            string videoProfile;
                            string videoAvgFrameRate;
                            long videoBitRate;
                            string audioCodecName;
                            long audioSampleRate;
                            int audioChannels;
                            long audioBitRate;

                            tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
                                videoDetails = getVideoDetails(localMediaItemKey, physicalPathKey);

                            tie(durationInMilliSeconds, bitRate,
                                videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
                                audioCodecName, audioSampleRate, audioChannels, audioBitRate) = videoDetails;

                            Json::Value videoDetailsRoot;

                            field = "durationInMilliSeconds";
                            videoDetailsRoot[field] = durationInMilliSeconds;

                            field = "videoWidth";
                            videoDetailsRoot[field] = videoWidth;

                            field = "videoHeight";
                            videoDetailsRoot[field] = videoHeight;

                            field = "bitRate";
                            videoDetailsRoot[field] = (int64_t) bitRate;

                            field = "videoCodecName";
                            videoDetailsRoot[field] = videoCodecName;

                            field = "videoProfile";
                            videoDetailsRoot[field] = videoProfile;

                            field = "videoAvgFrameRate";
                            videoDetailsRoot[field] = videoAvgFrameRate;

                            field = "videoBitRate";
                            videoDetailsRoot[field] = (int64_t) videoBitRate;

                            field = "audioCodecName";
                            videoDetailsRoot[field] = audioCodecName;

                            field = "audioSampleRate";
                            videoDetailsRoot[field] = (int64_t) audioSampleRate;

                            field = "audioChannels";
                            videoDetailsRoot[field] = audioChannels;

                            field = "audioBitRate";
                            videoDetailsRoot[field] = (int64_t) audioBitRate;


                            field = "videoDetails";
                            profileRoot[field] = videoDetailsRoot;
                        }
                        else if (contentType == ContentType::Audio)
                        {
                            int64_t durationInMilliSeconds;
                            string codecName;
                            long bitRate;
                            long sampleRate;
                            int channels;

                            tuple<int64_t,string,long,long,int>
                                audioDetails = getAudioDetails(localMediaItemKey, physicalPathKey);

                            tie(durationInMilliSeconds, codecName, bitRate, sampleRate, channels) 
                                    = audioDetails;

                            Json::Value audioDetailsRoot;

                            field = "durationInMilliSeconds";
                            audioDetailsRoot[field] = durationInMilliSeconds;

                            field = "codecName";
                            audioDetailsRoot[field] = codecName;

                            field = "bitRate";
                            audioDetailsRoot[field] = (int64_t) bitRate;

                            field = "sampleRate";
                            audioDetailsRoot[field] = (int64_t) sampleRate;

                            field = "channels";
                            audioDetailsRoot[field] = channels;


                            field = "audioDetails";
                            profileRoot[field] = audioDetailsRoot;
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
    
    return mediaItemsListRoot;
}

pair<shared_ptr<sql::ResultSet>, int64_t> MMSEngineDBFacade::getMediaItemsList_withoutTagsCheck (
		shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey, int64_t mediaItemKey,
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
            sqlWhere += ("and mi.mediaItemKey = ? ");
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
        if (liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
			{
				sqlWhere += ("and (JSON_EXTRACT(userData, '$.mmsData.dataType') is NULL ");
				sqlWhere += ("OR JSON_EXTRACT(userData, '$.mmsData.dataType') != 'liveRecordingChunk') ");
			}
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' ");
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
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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
        int64_t workspaceKey, int64_t mediaItemKey,
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
			if (tagsIn.size() > 0)
			{
				for (string tag: tagsIn)
				{
					tag = tagSeparator + tag + tagSeparator;

					if (tagsGroupCondition == "")
						tagsGroupCondition = "f.tagsGroup like '%" + tag + "%' ";
					else
						tagsGroupCondition += ("and f.tagsGroup like '%" + tag + "%' ");
				}
			}
			if (tagsNotIn.size() > 0)
			{
				for (string tag: tagsNotIn)
				{
					tag = tagSeparator + tag + tagSeparator;

					if (tagsGroupCondition == "")
						tagsGroupCondition = "f.tagsGroup not like '%" + tag + "%' ";
					else
						tagsGroupCondition += ("and f.tagsGroup not like '%" + tag + "%' ");
				}
			}
		}

		// create temporary table

		{
			string sqlWhere;
			sqlWhere = string ("where mi.mediaItemKey = t.mediaItemKey and mi.workspaceKey = ? ");
			if (mediaItemKey != -1)
				sqlWhere += ("and mi.mediaItemKey = ? ");
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
			if (liveRecordingChunk != -1)
			{
				if (liveRecordingChunk == 0)
				{
					sqlWhere += ("and (JSON_EXTRACT(mi.userData, '$.mmsData.dataType') is NULL ");
					sqlWhere += ("OR JSON_EXTRACT(mi.userData, '$.mmsData.dataType') != 'liveRecordingChunk') ");
				}
				else if (liveRecordingChunk == 1)
					sqlWhere += ("and JSON_EXTRACT(mi.userData, '$.mmsData.dataType') = 'liveRecordingChunk' ");
			}
			if (jsonCondition != "")
				sqlWhere += ("and " + jsonCondition);
        
			lastSQLCommand = 
				string("create temporary table MMS_MediaItemFilter select "
					"t.mediaItemKey, CONCAT('" + tagSeparator +
						"', GROUP_CONCAT(t.name SEPARATOR '" + tagSeparator + "'), '" +
						tagSeparator + "') tagsGroup "
					"from MMS_MediaItem mi, MMS_Tag t ")
				+ sqlWhere
				+ "group by t.mediaItemKey "
				;
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (mediaItemKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
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
		}

		int64_t numFound;
		{
			lastSQLCommand = 
				string("select count(*) from MediaItemFilter f where ")
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
           			"mi.contentType, mi.retentionInMinutes from MMS_MediaItem mi, MMS_MediaItemFilter f ")
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
    int64_t referenceMediaItemKey, int64_t encodingProfileKey,
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

        {
            lastSQLCommand = 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = ? and encodingProfileKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, referenceMediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                physicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
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

tuple<MMSEngineDBFacade::ContentType,string,string,string,int64_t> MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t mediaItemKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<MMSEngineDBFacade::ContentType,string,string,string,int64_t>
		contentTypeTitleUserDataIngestionDateAndIngestionJobKey;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select contentType, title, userData, ingestionJobKey, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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

                contentTypeTitleUserDataIngestionDateAndIngestionJobKey = make_tuple(contentType, title, userData, ingestionDate, ingestionJobKey);
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

        return contentTypeTitleUserDataIngestionDateAndIngestionJobKey;
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

tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t> MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(
    int64_t physicalPathKey, bool warningIfMissing)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn = nullptr;
    
    tuple<int64_t,MMSEngineDBFacade::ContentType,string,string,string,int64_t>
		mediaItemKeyContentTypeTitleUserDataIngestionDateAndIngestionJobKey;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, mi.ingestionJobKey, "
                "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
                "where mi.mediaItemKey = p.mediaItemKey and p.physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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

                string ingestionDate;
                if (!resultSet->isNull("ingestionDate"))
                    ingestionDate = resultSet->getString("ingestionDate");

                int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

                mediaItemKeyContentTypeTitleUserDataIngestionDateAndIngestionJobKey =
					make_tuple(mediaItemKey, contentType, title, userData, ingestionDate, ingestionJobKey);                
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
        
        return mediaItemKeyContentTypeTitleUserDataIngestionDateAndIngestionJobKey;
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
    int64_t referenceIngestionJobKey, 
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

        {
            // order by in the next select is important  to have the right order in case of dependency in a workflow
            lastSQLCommand = 
                "select mediaItemKey, physicalPathKey from MMS_IngestionJobOutput where ingestionJobKey = ? order by mediaItemKey";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
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
                            _logger->warn(errorMessage);
                        else
                            _logger->error(errorMessage);

                        throw MediaItemKeyNotFound(errorMessage);                    
                    }            
                }

                tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType 
                        = make_tuple(mediaItemKey, physicalPathKey, contentType);
                mediaItemsDetails.push_back(mediaItemKeyPhysicalPathKeyAndContentType);
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

pair<int64_t,MMSEngineDBFacade::ContentType> MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
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
                "select mi.mediaItemKey, mi.contentType from MMS_MediaItem mi, MMS_ExternalUniqueName eun "
                "where mi.mediaItemKey = eun.mediaItemKey "
                "and eun.workspaceKey = ? and eun.uniqueName = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
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
    
    return mediaItemKeyAndContentType;
}

tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> MMSEngineDBFacade::getVideoDetails(
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
                "from MMS_VideoItemProfile where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

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
        
        return make_tuple(durationInMilliSeconds, bitRate,
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate);
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

tuple<int64_t,string,long,long,int> MMSEngineDBFacade::getAudioDetails(
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
        
        int64_t durationInMilliSeconds;
        string codecName;
        long bitRate;
        long sampleRate;
        int channels;

        {
            lastSQLCommand = 
                "select durationInMilliSeconds, codecName, bitRate, sampleRate, channels "
                "from MMS_AudioItemProfile where physicalPathKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, localPhysicalPathKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                durationInMilliSeconds = resultSet->getInt64("durationInMilliSeconds");
                codecName = resultSet->getString("codecName");
                bitRate = resultSet->getInt("bitRate");
                sampleRate = resultSet->getInt("sampleRate");
                channels = resultSet->getInt("channels");
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
        
        return make_tuple(durationInMilliSeconds, codecName, bitRate, sampleRate, channels);
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

pair<int64_t,int64_t> MMSEngineDBFacade::saveIngestedContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,        
        MMSEngineDBFacade::ContentType contentType,
        Json::Value parametersRoot,
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

        _logger->info(__FILEREF__ + "Retrieving contentProviderKey");
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
            if (isMetadataPresent(parametersRoot, "ContentProviderName"))
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
            string title = "";
            string ingester = "";
            string userData = "";
            string deliveryFileName = "";
            string sContentType;
            int retentionInMinutes = _contentRetentionInMinutesDefaultValue;
            // string encodingProfilesSet;

            string field = "Title";
            title = parametersRoot.get(field, "").asString();
            
            field = "Ingester";
            if (isMetadataPresent(parametersRoot, field))
                ingester = parametersRoot.get(field, "").asString();

            field = "UserData";
            if (isMetadataPresent(parametersRoot, field))
            {
                Json::StreamWriterBuilder wbuilder;

                userData = Json::writeString(wbuilder, parametersRoot[field]);                        
            }

            field = "DeliveryFileName";
            if (isMetadataPresent(parametersRoot, field))
                deliveryFileName = parametersRoot.get(field, "").asString();

            field = "Retention";
            if (isMetadataPresent(parametersRoot, field))
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
                if (isMetadataPresent(parametersRoot, field))
                {
                    Json::Value publishingRoot = parametersRoot[field];

                    field = "startPublishing";
                    if (isMetadataPresent(publishingRoot, field))
                        startPublishing = publishingRoot.get(field, "").asString();

                    field = "endPublishing";
                    if (isMetadataPresent(publishingRoot, field))
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
			if (isMetadataPresent(parametersRoot, field))
			{
				for (int tagIndex = 0; tagIndex < parametersRoot[field].size(); tagIndex++)
				{
					string tag = parametersRoot[field][tagIndex].asString();
					trim(tag);

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
            if (isMetadataPresent(parametersRoot, "UniqueName"))
                uniqueName = parametersRoot.get("UniqueName", "").asString();

            if (uniqueName != "")
            {
                lastSQLCommand = 
                    "insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setString(queryParameterIndex++, uniqueName);

                preparedStatement->executeUpdate();
            }
        }
        
		// cross references
		{
			if (contentType == ContentType::Image)
			{
				string field = "ImageOfVideoMediaItemKey";
				if (isMetadataPresent(parametersRoot, field))
				{
					// string sImageOfVideoMediaItemKey = parametersRoot.get(field, "").asString();

					// if (sImageOfVideoMediaItemKey != "")
					{
						// int64_t imageOfVideoMediaItemKey = stoll(sImageOfVideoMediaItemKey);
						int64_t imageOfVideoMediaItemKey = parametersRoot.get(field, -1).asInt64();
						string type = "imageOfVideo";

						lastSQLCommand = 
							"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey) values ("
							"?, ?, ?)";

						shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
						preparedStatement->setString(queryParameterIndex++, type);
						preparedStatement->setInt64(queryParameterIndex++, imageOfVideoMediaItemKey);

						preparedStatement->executeUpdate();
					}
				}

				field = "ImageOfAudioMediaItemKey";
				if (isMetadataPresent(parametersRoot, field))
				{
					// string sImageOfAudioMediaItemKey = parametersRoot.get(field, "").asString();

					// if (sImageOfAudioMediaItemKey != "")
					{
						// int64_t imageOfAudioMediaItemKey = stoll(sImageOfAudioMediaItemKey);
						int64_t imageOfAudioMediaItemKey = parametersRoot.get(field, -1).asInt64();
						string type = "imageOfAudio";

						lastSQLCommand = 
							"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey) values ("
							"?, ?, ?)";

						shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
						preparedStatement->setString(queryParameterIndex++, type);
						preparedStatement->setInt64(queryParameterIndex++, imageOfAudioMediaItemKey);

						preparedStatement->executeUpdate();
					}
				}
			}
		}

        /*
        // territories
        {
            string field = "Territories";
            if (isMetadataPresent(parametersRoot, field))
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
        */

        int64_t encodingProfileKey = -1;
        int64_t physicalPathKey = saveEncodedContentMetadata(
            conn,
                
            workspace->_workspaceKey,
            mediaItemKey,
            mediaSourceFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,

            // video-audio
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

            // image
            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
        );

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
                if (isMetadataPresent(parametersRoot, field))
                {
                    Json::Value userDataRoot = parametersRoot[field];

                    field = "mmsData";
                    if (isMetadataPresent(userDataRoot, field))
					{
						Json::Value mmsDataRoot = userDataRoot[field];

						field = "dataType";
						if (isMetadataPresent(mmsDataRoot, field))
						{
							string dataType = mmsDataRoot.get(field, "").asString();
							if (dataType == "liveRecordingChunk")
							{
								field = "ingestionJobKey";
								if (isMetadataPresent(mmsDataRoot, field))
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
    
    return mediaItemKeyAndPhysicalPathKey;
}

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
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
        
        physicalPathKey = saveEncodedContentMetadata(
            conn,

            workspaceKey,
            mediaItemKey,
            encodedFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,

            // video-audio
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

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        shared_ptr<MySQLConnection> conn,
        
        int64_t workspaceKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
        
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
        
        {
            int drm = 0;

			_logger->info(__FILEREF__ + "insert into MMS_PhysicalPath"
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", relativePath: " + relativePath
					+ ", encodedFileName: " + encodedFileName
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					);
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
            if (encodingProfileKey == -1)
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
            else
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            preparedStatement->executeUpdate();
        }

        physicalPathKey = getLastInsertId(conn);

        {
            if (contentType == ContentType::Video)
            {
                lastSQLCommand = 
                    "insert into MMS_VideoItemProfile (physicalPathKey, durationInMilliSeconds, bitRate, width, height, avgFrameRate, "
                    "videoCodecName, videoProfile, videoBitRate, "
                    "audioCodecName, audioSampleRate, audioChannels, audioBitRate) values ("
                    "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
            else if (contentType == ContentType::Audio)
            {
                lastSQLCommand = 
                    "insert into MMS_AudioItemProfile (physicalPathKey, durationInMilliSeconds, codecName, bitRate, sampleRate, channels) values ("
                    "?, ?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
            else if (contentType == ContentType::Image)
            {
                lastSQLCommand = 
                    "insert into MMS_ImageItemProfile (physicalPathKey, width, height, format, quality) values ("
                    "?, ?, ?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
    
    return physicalPathKey;
}

void MMSEngineDBFacade::addCrossReference (
	int64_t sourceMediaItemKey, string type, int64_t targetMediaItemKey)
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
				"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey) values ("
				"?, ?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);
			preparedStatement->setString(queryParameterIndex++, type);
			preparedStatement->setInt64(queryParameterIndex++, targetMediaItemKey);

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

