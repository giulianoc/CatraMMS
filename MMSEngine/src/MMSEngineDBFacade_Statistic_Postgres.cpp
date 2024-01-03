
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


Json::Value MMSEngineDBFacade::addRequestStatistic(
	int64_t workspaceKey,
	string ipAddress,
	string userId,
	int64_t physicalPathKey,
	int64_t confStreamKey,
	string title
	)
{

	if (!_statisticsEnabled)
	{
		Json::Value statisticRoot;

		return statisticRoot;
	}

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		string sqlStatement = fmt::format(
			"insert into MMS_RequestStatistic(workspaceKey, ipAddress, userId, physicalPathKey, "
			"confStreamKey, title, requestTimestamp) values ("
			"{}, {}, {}, {}, {}, {}, now() at time zone 'utc') returning requestStatisticKey",
			workspaceKey,
			(ipAddress == "" ? "null" : trans.quote(ipAddress)),
			trans.quote(userId),
			(physicalPathKey == -1 ? "null" : to_string(physicalPathKey)),
			(confStreamKey == -1 ? "null" : to_string(confStreamKey)),
			trans.quote(title)
		);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		int64_t requestStatisticKey = trans.exec1(sqlStatement)[0].as<int64_t>();
		SPDLOG_INFO("SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, conn->getConnectionId(),
			chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
		);

		// update upToNextRequestInSeconds
		{
			sqlStatement = fmt::format(
				"select max(requestStatisticKey) as maxRequestStatisticKey from MMS_RequestStatistic "
				"where workspaceKey = {} and requestStatisticKey < {} and userId = {}",
				workspaceKey, requestStatisticKey, trans.quote(userId)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				if (!res[0][0].is_null())
				{
					int64_t previoudRequestStatisticKey = res[0][0].as<int64_t>();

					{
						sqlStatement = fmt::format(
							"WITH rows AS (update MMS_RequestStatistic "
							"set upToNextRequestInSeconds = EXTRACT(EPOCH FROM (now() at time zone 'utc' - requestTimestamp)) "
							"where requestStatisticKey = {} returning 1) SELECT count(*) FROM rows",
							previoudRequestStatisticKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
						SPDLOG_INFO("SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
						if (rowsUpdated != 1)
						{
							string errorMessage = fmt::format("no update was done"
								", previoudRequestStatisticKey: {}"
								", rowsUpdated: {}"
								", sqlStatement: {}",
								previoudRequestStatisticKey, rowsUpdated, sqlStatement);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
		}

		Json::Value statisticRoot;
		{
			string field = "requestStatisticKey";
			statisticRoot[field] = requestStatisticKey;

			field = "ipAddress";
			statisticRoot[field] = ipAddress;

			field = "userId";
			statisticRoot[field] = userId;

			field = "physicalPathKey";
			statisticRoot[field] = physicalPathKey;

			field = "confStreamKey";
			statisticRoot[field] = confStreamKey;

			field = "title";
			statisticRoot[field] = title;
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return statisticRoot;
	}
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticList"
		", workspaceKey: {}"
		", userId: {}"
		", title: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, userId, title, startStatisticDate, endStatisticDate, start, rows
	);

    Json::Value statisticsListRoot;
    
    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (userId != "")
			sqlWhere += fmt::format("and userId like {} ", trans.quote("%" + userId + "%"));
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));

        Json::Value responseRoot;
        {
            string sqlStatement = 
				fmt::format("select count(*) from MMS_RequestStatistic {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = count;
        }

        Json::Value statisticsRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select requestStatisticKey, ipAddress, userId, physicalPathKey, confStreamKey, title, "
				"to_char(requestTimestamp, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as requestTimestamp "
				"from MMS_RequestStatistic {}"
				"order by requestTimestamp asc "
				"limit {} offset {}",
				sqlWhere, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "requestStatisticKey";
				statisticRoot[field] = row["requestStatisticKey"].as<int64_t>();

                field = "ipAddress";
				if (row["ipAddress"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["ipAddress"].as<string>();

                field = "userId";
                statisticRoot[field] = row["userId"].as<string>();

                field = "physicalPathKey";
				if (row["physicalPathKey"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["physicalPathKey"].as<int64_t>();

                field = "confStreamKey";
				if (row["confStreamKey"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["confStreamKey"].as<int64_t>();

                field = "title";
                statisticRoot[field] = row["title"].as<string>();

                field = "requestTimestamp";
				statisticRoot[field] = row["requestTimestamp"].as<string>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
    {
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)                                              
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
    {
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)                                              
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticPerContentList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", totalNumFoundToBeCalculated: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate,
		minimalNextRequestDistanceInSeconds, totalNumFoundToBeCalculated, start, rows
	);

	Json::Value statisticsListRoot;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;

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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += fmt::format("and LOWER(userId) like LOWER({}) ", trans.quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += fmt::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			string sqlStatement = 
				fmt::format("select title, count(*) from MMS_RequestStatistic {}"
					"group by title order by count(*) desc ",
					sqlWhere
				)
			;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = res.size();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            string sqlStatement = 
				fmt::format("select title, count(*) as count from MMS_RequestStatistic {}"
				"group by title order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "title";
                statisticRoot[field] = row["title"].as<string>();

                field = "count";
                statisticRoot[field] = row["count"].as<int64_t>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticPerUserList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate,
		minimalNextRequestDistanceInSeconds, start, rows
	);

    Json::Value statisticsListRoot;

    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;

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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += fmt::format("and LOWER(userId) like LOWER({}) ", trans.quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += fmt::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			string sqlStatement = 
				fmt::format("select userId, count(*) from MMS_RequestStatistic {}"
					"group by userId order by count(*) desc ",
					sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = res.size();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            string sqlStatement = 
				fmt::format("select userId, count(*) as count from MMS_RequestStatistic {}"
					"group by userId order by count(*) desc "
					"limit {} offset {}",
					sqlWhere, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "userId";
                statisticRoot[field] = row["userId"].as<string>();

                field = "count";
                statisticRoot[field] = row["count"].as<int64_t>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticPerMonthList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate,
		minimalNextRequestDistanceInSeconds, start, rows
	);

    Json::Value statisticsListRoot;

    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;

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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += fmt::format("and LOWER(userId) like LOWER({}) ", trans.quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += fmt::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
        {
			string sqlStatement = fmt::format(
				"select to_char(requestTimestamp, 'YYYY-MM') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM') order by count(*) desc ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = res.size();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            string sqlStatement = fmt::format(
				"select to_char(requestTimestamp, 'YYYY-MM') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM') order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = row["date"].as<string>();

                field = "count";
                statisticRoot[field] = row["count"].as<int64_t>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticPerDayList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate,
		minimalNextRequestDistanceInSeconds, start, rows
	);

    Json::Value statisticsListRoot;

    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;

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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += fmt::format("and LOWER(userId) like LOWER({}) ", trans.quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += fmt::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = fmt::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM-DD') order by count(*) desc ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = res.size();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            string sqlStatement = fmt::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD') order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = row["date"].as<string>();

                field = "count";
                statisticRoot[field] = row["count"].as<int64_t>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
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
	SPDLOG_INFO("getRequestStatisticPerHourList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate,
		minimalNextRequestDistanceInSeconds, start, rows
	);

    Json::Value statisticsListRoot;

    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;

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
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += fmt::format("and LOWER(title) like LOWER({}) ", trans.quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += fmt::format("and LOWER(userId) like LOWER({}) ", trans.quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += fmt::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

        Json::Value responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = fmt::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD HH24') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD HH24') order by count(*) desc ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = res.size();
        }
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

        Json::Value statisticsRoot(Json::arrayValue);
        {
            string sqlStatement = fmt::format( 
				"select to_char(requestTimestamp, 'YYYY-MM-DD HH24') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD HH24') order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "date";
                statisticRoot[field] = row["date"].as<string>();

                field = "count";
                statisticRoot[field] = row["count"].as<int64_t>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "requestStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
    
    return statisticsListRoot;
}

void MMSEngineDBFacade::retentionOfStatisticData()
{
	SPDLOG_INFO("retentionOfStatisticData");

    shared_ptr<PostgresConnection> conn = nullptr;
	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		// check if next partition already exist and, if not, create it
		{
			string partition_start;
			string partition_end;
			string partitionName;
			{
				// 2022-05-01: changed from one to two months because, the first of each
				//	month, until this procedure do not run, it would not work
				chrono::duration<int, ratio<60*60*24*32>> one_month(1);

				char strDateTime [64];
				tm tmDateTime;
				time_t utcTime;
				chrono::system_clock::time_point today = chrono::system_clock::now();

				chrono::system_clock::time_point nextMonth = today + one_month;
				utcTime = chrono::system_clock::to_time_t(nextMonth);
				localtime_r (&utcTime, &tmDateTime);
				sprintf (strDateTime, "%04d-%02d-01",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1);
				partition_start = strDateTime;

				sprintf (strDateTime, "%04d_%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1);
				partitionName = fmt::format("requeststatistic_{}", strDateTime);


				chrono::system_clock::time_point nextNextMonth = nextMonth + one_month;
				utcTime = chrono::system_clock::to_time_t(nextNextMonth);
				localtime_r (&utcTime, &tmDateTime);
				sprintf (strDateTime, "%04d-%02d-01",
					tmDateTime. tm_year + 1900,      
					tmDateTime. tm_mon + 1);   
				partition_end = strDateTime;
			}

			/*
			SELECT
				nmsp_parent.nspname AS parent_schema,
				parent.relname      AS parent,
				nmsp_child.nspname  AS child_schema,
				child.relname       AS child
			FROM pg_inherits
				JOIN pg_class parent            ON pg_inherits.inhparent = parent.oid
				JOIN pg_class child             ON pg_inherits.inhrelid   = child.oid
				JOIN pg_namespace nmsp_parent   ON nmsp_parent.oid  = parent.relnamespace
				JOIN pg_namespace nmsp_child    ON nmsp_child.oid   = child.relnamespace
			WHERE parent.relname='mms_requeststatistic';
			*/
			string sqlStatement = fmt::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (count == 0)
			{
				string sqlStatement = fmt::format(
					"CREATE TABLE {} PARTITION OF MMS_RequestStatistic "
					"FOR VALUES FROM ({}) TO ({}) ",
					partitionName, trans.quote(partition_start), trans.quote(partition_end)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
		}

		// check if a partition has to be removed because "expired", if yes, remove it
		{
			string partitionName;
			{
				chrono::duration<int, ratio<60*60*24*31>> retentionMonths(
					_statisticRetentionInMonths);

				chrono::system_clock::time_point today = chrono::system_clock::now();
				chrono::system_clock::time_point retention = today - retentionMonths;
				time_t utcTime_retention = chrono::system_clock::to_time_t(retention);

				char strDateTime [64];
				tm tmDateTime;

				localtime_r (&utcTime_retention, &tmDateTime);

				sprintf (strDateTime, "%04d_%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1);
				partitionName = fmt::format("requeststatistic_{}", strDateTime);
			}

			string sqlStatement = fmt::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (count > 0)
			{
				string sqlStatement = fmt::format( 
					"DROP TABLE {}",
					partitionName
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
		}

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
	}
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
}

Json::Value MMSEngineDBFacade::getLoginStatisticList (
	string startStatisticDate, string endStatisticDate,
	int start, int rows
)
{
	SPDLOG_INFO("getLoginStatisticList"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", start: {}"
		", rows: {}",
		startStatisticDate, endStatisticDate, start, rows
	);

    Json::Value statisticsListRoot;
    
    shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
        {
            Json::Value requestParametersRoot;

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
        
		string sqlWhere = "where s.userKey = u.userKey ";
		if (startStatisticDate != "")
			sqlWhere += fmt::format("and s.successfulLogin >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startStatisticDate));
		if (endStatisticDate != "")
			sqlWhere += fmt::format("and s.successfulLogin <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endStatisticDate));

        Json::Value responseRoot;
        {
            string sqlStatement = 
				fmt::format("select count(*) from MMS_LoginStatistic s, MMS_User u {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

            field = "numFound";
            responseRoot[field] = count;
        }

        Json::Value statisticsRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select u.name as userName, u.eMailAddress as emailAddress, s.loginStatisticKey, "
				"s.userKey, s.ip, s.continent, s.continentCode, s.country, s.countryCode, "
				"s.region, s.city, s.org, s.isp, s.timezoneGMTOffset, "
				"to_char(s.successfulLogin, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as successfulLogin "
				"from MMS_LoginStatistic s, MMS_User u {}"
				"order by s.successfulLogin desc "
				"limit {} offset {}",
				sqlWhere, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value statisticRoot;

                field = "loginStatisticKey";
				statisticRoot[field] = row["loginStatisticKey"].as<int64_t>();

                field = "userKey";
                statisticRoot[field] = row["userKey"].as<int64_t>();

				field = "userName";
				statisticRoot[field] = row["userName"].as<string>();

				field = "emailAddress";
				statisticRoot[field] = row["emailAddress"].as<string>();

				field = "ip";
				if (row["ip"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["ip"].as<string>();

				field = "continent";
				if (row["continent"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["continent"].as<string>();

				field = "continentCode";
				if (row["continentCode"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["continentCode"].as<string>();

				field = "country";
				if (row["country"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["country"].as<string>();

				field = "countryCode";
				if (row["countryCode"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["countryCode"].as<string>();

				field = "region";
				if (row["region"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["region"].as<string>();

				field = "city";
				if (row["city"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["city"].as<string>();

				field = "org";
				if (row["org"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["org"].as<string>();

				field = "isp";
				if (row["isp"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["isp"].as<string>();

				field = "timezoneGMTOffset";
				if (row["isp"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["timezoneGMTOffset"].as<int>();

                field = "successfulLogin";
				if (row["successfulLogin"].is_null())
					statisticRoot[field] = Json::nullValue;
				else
					statisticRoot[field] = row["successfulLogin"].as<string>();

                statisticsRoot.append(statisticRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

        field = "loginStatistics";
        responseRoot[field] = statisticsRoot;

        field = "response";
        statisticsListRoot[field] = responseRoot;

		trans.commit();
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
    {
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)                                              
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}


		throw e;
	}
	catch(exception& e)
    {
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)                                              
			);
		}
		if (conn != nullptr)
		{
			connectionPool->unborrow(conn);
			conn = nullptr;
		}

		throw e;
	}

    return statisticsListRoot;
}

