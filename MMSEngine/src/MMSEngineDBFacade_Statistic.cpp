
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

Json::Value MMSEngineDBFacade::addRequestStatistic(
	int64_t workspaceKey,
	string userId,
	int64_t physicalPathKey,
	int64_t confStreamKey,
	string title
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
				"insert into MMS_RequestStatistic(workspaceKey, userId, physicalPathKey, "
				"confStreamKey, title, requestTimestamp) values ("
				"?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setString(queryParameterIndex++, userId);
			if (physicalPathKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
			if (confStreamKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, confStreamKey);
			preparedStatement->setString(queryParameterIndex++, title);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userId: " + userId
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", confStreamKey: " + to_string(confStreamKey)
				+ ", title: " + title
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

		int64_t requestStatisticKey = getLastInsertId(conn);

		// update upToNextRequestInSeconds
		{
			lastSQLCommand = 
				"select requestStatisticKey from MMS_RequestStatistic "
				"where workspaceKey = ? and requestStatisticKey < ? and userId = ? "
				"order by requestStatisticKey desc limit 1";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, requestStatisticKey);
			preparedStatement->setString(queryParameterIndex++, userId);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", requestStatisticKey: " + to_string(requestStatisticKey)
				+ ", userId: " + userId
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				int64_t previoudRequestStatisticKey = resultSet->getInt64("requestStatisticKey");

				{
					lastSQLCommand = 
						"update MMS_RequestStatistic "
						"set upToNextRequestInSeconds = TIMESTAMPDIFF(SECOND, requestTimestamp, NOW()) "
						"where requestStatisticKey = ?";
					shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, previoudRequestStatisticKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", previoudRequestStatisticKey: " + to_string(previoudRequestStatisticKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (millisecs): @" + to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", previoudRequestStatisticKey: " + to_string(previoudRequestStatisticKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", lastSQLCommand: " + lastSQLCommand
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
            }
        }

		Json::Value statisticRoot;
		{
			string field = "requestStatisticKey";
			statisticRoot[field] = requestStatisticKey;

			field = "userId";
			statisticRoot[field] = userId;

			field = "physicalPathKey";
			statisticRoot[field] = physicalPathKey;

			field = "confStreamKey";
			statisticRoot[field] = confStreamKey;

			field = "title";
			statisticRoot[field] = title;
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return statisticRoot;
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

Json::Value MMSEngineDBFacade::getRequestStatisticList (
	int64_t workspaceKey,
	string userId,
	string title,
	string startStatisticDate, string endStatisticDate,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getRequestStatisticList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", userId: " + userId
            + ", title: " + title
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (userId != "")
			sqlWhere += ("and userId like ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");

        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_RequestStatistic ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userId: " + userId
				+ ", title: " + title
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
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

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                "select requestStatisticKey, userId, physicalPathKey, confStreamKey, title, "
				"DATE_FORMAT(convert_tz(requestTimestamp, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as requestTimestamp "
				"from MMS_RequestStatistic "
                + sqlWhere
				+ "order by requestTimestamp asc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userId: " + userId
				+ ", title: " + title
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "requestStatisticKey";
				statisticRoot[field] = resultSet->getInt64("requestStatisticKey");

                field = "userId";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("userId"));

                field = "physicalPathKey";
				if (resultSet->isNull("physicalPathKey"))
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = resultSet->getInt64("physicalPathKey");

                field = "confStreamKey";
				if (resultSet->isNull("confStreamKey"))
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = resultSet->getInt64("confStreamKey");

                field = "title";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("title"));

                field = "requestTimestamp";
				statisticRoot[field] = static_cast<string>(
					resultSet->getString("requestTimestamp"));

                statisticsRoot.append(statisticRoot);
            }
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

Json::Value MMSEngineDBFacade::getRequestStatisticPerContentList (
	int64_t workspaceKey,
	string title, string userId,
	string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds,
	bool totalNumFoundToBeCalculated,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getRequestStatisticPerContentList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", title: " + title
            + ", userId: " + userId
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (userId != "")
			sqlWhere += ("and LOWER(userId) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += ("and upToNextRequestInSeconds >= ? ");

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			lastSQLCommand = 
				string("select title, count(*) from MMS_RequestStatistic ")
				+ sqlWhere
				+ "group by title order by count(*) desc "
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics Per content rowsCount@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            field = "numFound";
            responseRoot[field] = resultSet->rowsCount();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				string("select title, count(*) as count from MMS_RequestStatistic ")
				+ sqlWhere
				+ "group by title order by count(*) desc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "title";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("title"));

                field = "count";
                statisticRoot[field] = resultSet->getInt64("count");

                statisticsRoot.append(statisticRoot);
            }
			_logger->info(__FILEREF__ + "@SQL statistics@ Per content limit"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

Json::Value MMSEngineDBFacade::getRequestStatisticPerUserList (
	int64_t workspaceKey,
	string title, string userId,
	string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds,
	bool totalNumFoundToBeCalculated,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getRequestStatisticPerUserList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", title: " + title
            + ", userId: " + userId
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (userId != "")
			sqlWhere += ("and LOWER(userId) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += ("and upToNextRequestInSeconds >= ? ");

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			lastSQLCommand = 
				string("select userId, count(*) from MMS_RequestStatistic ")
				+ sqlWhere
				+ "group by userId order by count(*) desc "
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ Per user rowsCount"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            field = "numFound";
            responseRoot[field] = resultSet->rowsCount();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				string("select userId, count(*) as count from MMS_RequestStatistic ")
				+ sqlWhere
				+ "group by userId order by count(*) desc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "userId";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("userId"));

                field = "count";
                statisticRoot[field] = resultSet->getInt64("count");

                statisticsRoot.append(statisticRoot);
            }
			_logger->info(__FILEREF__ + "@SQL statistics@ Per user limit"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

Json::Value MMSEngineDBFacade::getRequestStatisticPerMonthList (
	int64_t workspaceKey,
	string title, string userId,
	string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds,
	bool totalNumFoundToBeCalculated,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getRequestStatisticPerMonthList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", title: " + title
            + ", userId: " + userId
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (userId != "")
			sqlWhere += ("and LOWER(userId) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += ("and upToNextRequestInSeconds >= ? ");

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m\") order by count(*) desc "
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ Per month rowsCount"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            field = "numFound";
            responseRoot[field] = resultSet->rowsCount();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m\") order by count(*) desc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("date"));

                field = "count";
                statisticRoot[field] = resultSet->getInt64("count");

                statisticsRoot.append(statisticRoot);
            }
			_logger->info(__FILEREF__ + "@SQL statistics@ Per month limit"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

Json::Value MMSEngineDBFacade::getRequestStatisticPerDayList (
	int64_t workspaceKey,
	string title, string userId,
	string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds,
	bool totalNumFoundToBeCalculated,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getRequestStatisticPerDayList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", title: " + title
            + ", userId: " + userId
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (userId != "")
			sqlWhere += ("and LOWER(userId) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += ("and upToNextRequestInSeconds >= ? ");

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m-%d\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m-%d\") order by count(*) desc "
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ Per day rowsCount"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            field = "numFound";
            responseRoot[field] = resultSet->rowsCount();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m-%d\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m-%d\") order by count(*) desc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("date"));

                field = "count";
                statisticRoot[field] = resultSet->getInt64("count");

                statisticsRoot.append(statisticRoot);
            }
			_logger->info(__FILEREF__ + "@SQL statistics@ Per day limit"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

Json::Value MMSEngineDBFacade::getRequestStatisticPerHourList (
	int64_t workspaceKey,
	string title, string userId,
	string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds,
	bool totalNumFoundToBeCalculated,
	int start, int rows
)
{
    string      lastSQLCommand;
    Json::Value statisticsListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getRequestStatisticPerHourList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", title: " + title
            + ", userId: " + userId
            + ", startStatisticDate: " + startStatisticDate
            + ", endStatisticDate: " + endStatisticDate
            + ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
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
            
			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
            {
                field = "startStatisticDate";
                requestParametersRoot[field] = startStatisticDate;
            }

			if (endStatisticDate != "")
            {
                field = "endStatisticDate";
                requestParametersRoot[field] = endStatisticDate;
            }

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

            field = "requestParameters";
            statisticsListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = string ("where workspaceKey = ? ");
		if (title != "")
			sqlWhere += ("and LOWER(title) like LOWER(?) ");
		if (userId != "")
			sqlWhere += ("and LOWER(userId) like LOWER(?) ");
		if (startStatisticDate != "")
			sqlWhere += ("and requestTimestamp >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (endStatisticDate != "")
			sqlWhere += ("and requestTimestamp <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += ("and upToNextRequestInSeconds >= ? ");

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m-%d %H\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m-%d %H\") order by count(*) desc "
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++, string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ Per hour rowsCount"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            field = "numFound";
            responseRoot[field] = resultSet->rowsCount();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
				"select DATE_FORMAT(requestTimestamp, \"%Y-%m-%d %H\") as date, count(*) as count "
				"from MMS_RequestStatistic "
				+ sqlWhere
				+ "group by DATE_FORMAT(requestTimestamp, \"%Y-%m-%d %H\") order by count(*) desc "
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			if (title != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + title + "%");
			if (userId != "")
				preparedStatement->setString(queryParameterIndex++,
					string("%") + userId + "%");
			if (startStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, startStatisticDate);
			if (endStatisticDate != "")
				preparedStatement->setString(queryParameterIndex++, endStatisticDate);
			if (minimalNextRequestDistanceInSeconds > 0)
				preparedStatement->setInt64(queryParameterIndex++, minimalNextRequestDistanceInSeconds);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = static_cast<string>(
					resultSet->getString("date"));

                field = "count";
                statisticRoot[field] = resultSet->getInt64("count");

                statisticsRoot.append(statisticRoot);
            }
			_logger->info(__FILEREF__ + "@SQL statistics@ Per hour limit"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", title: " + title
				+ ", userId: " + userId
				+ ", startStatisticDate: " + startStatisticDate
				+ ", endStatisticDate: " + endStatisticDate
				+ ", minimalNextRequestDistanceInSeconds: " + to_string(minimalNextRequestDistanceInSeconds)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

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
    
    return statisticsListRoot;
}

void MMSEngineDBFacade::retentionOfStatisticData()
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "retentionOfStatisticData"
			);

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int maxRetriesOnError = 2;
		int currentRetriesOnError = 0;
		bool toBeExecutedAgain = true;
		while (toBeExecutedAgain)
		{
			try
			{
				// check if next partition already exist and, if not, create it
				{
					string currentPartition_YYYYMM_1;
					string currentPartition_YYYYMM_2;
					{
						// 2022-05-01: changed from one to two months because, the first of each
						//	month, until this procedure do not run, it would not work
						chrono::duration<int, ratio<60*60*24*30>> two_month(2);

						chrono::system_clock::time_point today = chrono::system_clock::now();
						chrono::system_clock::time_point nextMonth = today + two_month;
						time_t utcTime_nextMonth = chrono::system_clock::to_time_t(nextMonth);

						char strDateTime [64];
						tm tmDateTime;

						localtime_r (&utcTime_nextMonth, &tmDateTime);

						sprintf (strDateTime, "%04d-%02d-01",
						tmDateTime. tm_year + 1900,
						tmDateTime. tm_mon + 1);
						currentPartition_YYYYMM_1 = strDateTime;

						sprintf (strDateTime, "%04d_%02d_01",
						tmDateTime. tm_year + 1900,      
						tmDateTime. tm_mon + 1);   
						currentPartition_YYYYMM_2 = strDateTime;
					}                                           

					lastSQLCommand = 
						"select partition_name "
						"from information_schema.partitions "
						"where table_name = 'MMS_RequestStatistic' "
						"and partition_name = 'p_" + currentPartition_YYYYMM_2 + "'"
					;

					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", elapsed (millisecs): @"
							+ to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (!resultSet->next())
					{
						lastSQLCommand = 
							"ALTER TABLE MMS_RequestStatistic ADD PARTITION (PARTITION p_"
							+ currentPartition_YYYYMM_2
							+ " VALUES LESS THAN (to_days('" + currentPartition_YYYYMM_1 + "')) "
							+ ") "
						;

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
					}
				}
	
				// check if a partition has to be removed because "expired", if yes, remove it
				{
					string currentPartition_YYYYMM_2;
					{
						chrono::duration<int, ratio<60*60*24*30>> retentionMonths(
							_statisticRetentionInMonths);

						chrono::system_clock::time_point today = chrono::system_clock::now();
						chrono::system_clock::time_point retention = today - retentionMonths;
						time_t utcTime_retention = chrono::system_clock::to_time_t(retention);

						char strDateTime [64];
						tm tmDateTime;

						localtime_r (&utcTime_retention, &tmDateTime);

						sprintf (strDateTime, "%04d_%02d_01",
							tmDateTime. tm_year + 1900,      
							tmDateTime. tm_mon + 1);   
						currentPartition_YYYYMM_2 = strDateTime;
					}                                           

					lastSQLCommand = 
						"select partition_name "
						"from information_schema.partitions "
						"where table_name = 'MMS_RequestStatistic' "
						"and partition_name = 'p_" + currentPartition_YYYYMM_2 + "'"
					;

					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", elapsed (millisecs): @"
							+ to_string(chrono::duration_cast<chrono::milliseconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (resultSet->next())
					{
						lastSQLCommand = 
							"ALTER TABLE MMS_RequestStatistic DROP PARTITION p_"
							+ currentPartition_YYYYMM_2
						;

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
					}
				}

				toBeExecutedAgain = false;
			}
			catch(sql::SQLException se)
			{
				currentRetriesOnError++;
				if (currentRetriesOnError >= maxRetriesOnError)
					throw se;

				// Deadlock!!!
				_logger->error(__FILEREF__ + "SQL exception"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", se.what(): " + se.what()
					+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
				);

				{
					int secondsBetweenRetries = 15;
					_logger->info(__FILEREF__ + "retentionOfStatisticData failed, "
						+ "waiting before to try again"
						+ ", currentRetriesOnError: " + to_string(currentRetriesOnError)
						+ ", maxRetriesOnError: " + to_string(maxRetriesOnError)
						+ ", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
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
        _logger->error(__FILEREF__ + "runtime_error exception"
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
        _logger->error(__FILEREF__ + "exception"
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

