
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

Json::Value MMSEngineDBFacade::addRequestStatistic(
	int64_t workspaceKey,
	string userId,
	int64_t physicalPathKey,
	int64_t confStreamKey
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

		int64_t requestStatisticKey;
		{
			lastSQLCommand = 
				"insert into MMS_RequestStatistic(workspaceKey, userId, physicalPathKey, "
				"confStreamKey, requestTimestamp) values ("
				"?, ?, ?, ?, NOW())";

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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", userId: " + userId
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", confStreamKey: " + to_string(confStreamKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            requestStatisticKey = getLastInsertId(conn);
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
                "select requestStatisticKey, userId, "
				"physicalPathKey, confStreamKey, "
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

