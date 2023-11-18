
#include "JSONUtils.h"
#include "FFMpeg.h"
#include "MMSEngineDBFacade.h"

Json::Value MMSEngineDBFacade::addYouTubeConf(
	int64_t workspaceKey,
	string label,
	string tokenType,
	string refreshToken,
	string accessToken
	)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		int64_t confKey;
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_YouTube(workspaceKey, label, tokenType, "
				"refreshToken, accessToken) values ("
				"{}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(tokenType),
				trans.quote(refreshToken), trans.quote(accessToken));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		Json::Value youTubeConfRoot;
		{
			string field = "confKey";
			youTubeConfRoot[field] = confKey;

			field = "label";
			youTubeConfRoot[field] = label;

			field = "tokenType";
			youTubeConfRoot[field] = tokenType;

			field = "refreshToken";
			if (tokenType == "RefreshToken")
			{
				youTubeConfRoot[field] = refreshToken;

				field = "accessToken";
				youTubeConfRoot[field] = Json::nullValue;
			}
			else
			{
				youTubeConfRoot[field] = Json::nullValue;

				field = "accessToken";
				youTubeConfRoot[field] = accessToken;
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return youTubeConfRoot;
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

Json::Value MMSEngineDBFacade::modifyYouTubeConf(
    int64_t confKey, int64_t workspaceKey,
    string label, bool labelModified,
    string tokenType, bool tokenTypeModified,
    string refreshToken, bool refreshTokenModified,
    string accessToken, bool accessTokenModified
	)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.quote(label));
				oneParameterPresent = true;
			}

			if (tokenTypeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tokenType = " + trans.quote(tokenType));
				oneParameterPresent = true;
			}

			if (refreshTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("refreshToken = " + (refreshToken == "" ? "null" : trans.quote(refreshToken)));
				oneParameterPresent = true;
			}

			if (accessTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("accessToken = " + (accessToken == "" ? "null" : trans.quote(accessToken)));
				oneParameterPresent = true;
			}

            string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_YouTube {} "
				"where confKey = {} and workspaceKey = {} returning 1) SELECT count(*) FROM rows",
				setSQL, confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }

		Json::Value youTubeConfRoot;
        {
			string sqlStatement = fmt::format(
                "select confKey, label, tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where confKey = {} and workspaceKey = {}",
				confKey, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
				string errorMessage = __FILEREF__ + "No YouTube conf found"
					+ ", confKey: " + to_string(confKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
            }

            {
                string field = "confKey";
				youTubeConfRoot[field] = res[0]["confKey"].as<int64_t>();

                field = "label";
                youTubeConfRoot[field] = res[0]["label"].as<string>();

				field = "tokenType";
				youTubeConfRoot[field] = res[0]["tokenType"].as<string>();

				field = "refreshToken";
				if (res[0]["refreshToken"].is_null())
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = res[0]["refreshToken"].as<string>();

				field = "accessToken";
				if (res[0]["accessToken"].is_null())
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = res[0]["accessToken"].as<string>();
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return youTubeConfRoot;
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

void MMSEngineDBFacade::removeYouTubeConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_YouTube where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getYouTubeConfList (
        int64_t workspaceKey
)
{
    Json::Value youTubeConfListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getYouTubeConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
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
        
        string sqlWhere = fmt::format("where workspaceKey = ? ", workspaceKey);
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_YouTube {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value youTubeRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select confKey, label, tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value youTubeConfRoot;

                field = "confKey";
                youTubeConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				youTubeConfRoot[field] = row["label"].as<string>();

				field = "tokenType";
				youTubeConfRoot[field] = row["tokenType"].as<string>();

				field = "refreshToken";
				if (row["refreshToken"].is_null())
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = row["refreshToken"].as<string>();

				field = "accessToken";
				if (row["accessToken"].is_null())
					youTubeConfRoot[field] = Json::nullValue;
				else
					youTubeConfRoot[field] = row["accessToken"].as<string>();

                youTubeRoot.append(youTubeConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "youTubeConf";
        responseRoot[field] = youTubeRoot;

        field = "response";
        youTubeConfListRoot[field] = responseRoot;

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
    
    return youTubeConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getYouTubeDetailsByConfigurationLabel(
	int64_t workspaceKey, string youTubeConfigurationLabel
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
		string		youTubeTokenType;
		string		youTubeRefreshToken;
		string		youTubeAccessToken;

        _logger->info(__FILEREF__ + "getYouTubeDetailsByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
        );

        {
			string sqlStatement = fmt::format(
                "select tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(youTubeConfigurationLabel));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_YouTube failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", youTubeConfigurationLabel: " + youTubeConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

			youTubeTokenType = res[0]["tokenType"].as<string>();
			if (!res[0]["refreshToken"].is_null())
				youTubeRefreshToken = res[0]["refreshToken"].as<string>();
			if (!res[0]["accessToken"].is_null())
				youTubeAccessToken = res[0]["accessToken"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(youTubeTokenType, youTubeRefreshToken, youTubeAccessToken);
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

int64_t MMSEngineDBFacade::addFacebookConf(
    int64_t workspaceKey,
    string label,
    string userAccessToken)
{
    int64_t     confKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_Facebook(workspaceKey, label, modificationDate, userAccessToken) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(userAccessToken));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyFacebookConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string userAccessToken)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_Facebook set label = {}, userAccessToken = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(userAccessToken), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeFacebookConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_Facebook where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getFacebookConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	Json::Value facebookConfListRoot;

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
        
        _logger->info(__FILEREF__ + "getFacebookConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            facebookConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and label = {} ", trans.quote(label));
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_Facebook {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value facebookRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select confKey, label, userAccessToken, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
                "from MMS_Conf_Facebook {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value facebookConfRoot;

                field = "confKey";
				facebookConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				facebookConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				facebookConfRoot[field] = row["modificationDate"].as<string>();

                field = "userAccessToken";
				facebookConfRoot[field] = row["userAccessToken"].as<string>();

                facebookRoot.append(facebookConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "facebookConf";
        responseRoot[field] = facebookRoot;

        field = "response";
        facebookConfListRoot[field] = responseRoot;

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
    
    return facebookConfListRoot;
}

string MMSEngineDBFacade::getFacebookUserAccessTokenByConfigurationLabel(
    int64_t workspaceKey, string facebookConfigurationLabel
)
{
    string      facebookUserAccessToken;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getFacebookUserAccessTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", facebookConfigurationLabel: " + facebookConfigurationLabel
        );

        {
			string sqlStatement = fmt::format(
                "select userAccessToken from MMS_Conf_Facebook where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(facebookConfigurationLabel));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Facebook failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", facebookConfigurationLabel: " + facebookConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
			facebookUserAccessToken = res[0]["userAccessToken"].as<string>();
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
    
    return facebookUserAccessToken;
}

int64_t MMSEngineDBFacade::addTwitchConf(
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_Twitch(workspaceKey, label, modificationDate, refreshToken) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(refreshToken));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyTwitchConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string refreshToken)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_Twitch set label = {}, refreshToken = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(refreshToken), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeTwitchConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_Twitch where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getTwitchConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	Json::Value twitchConfListRoot;

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
        
        _logger->info(__FILEREF__ + "getTwitchConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            twitchConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and label = {} ", trans.quote(label));
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_Twitch {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value twitchRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select confKey, label, refreshToken, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
                "from MMS_Conf_Twitch {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value twitchConfRoot;

                field = "confKey";
				twitchConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				twitchConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				twitchConfRoot[field] = row["modificationDate"].as<string>();

                field = "refreshToken";
				twitchConfRoot[field] = row["refreshToken"].as<string>();

                twitchRoot.append(twitchConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "twitchConf";
        responseRoot[field] = twitchRoot;

        field = "response";
        twitchConfListRoot[field] = responseRoot;

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
    
    return twitchConfListRoot;
}

string MMSEngineDBFacade::getTwitchUserAccessTokenByConfigurationLabel(
    int64_t workspaceKey, string twitchConfigurationLabel
)
{
    string      twitchRefreshToken;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getTwitchUserAccessTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", twitchConfigurationLabel: " + twitchConfigurationLabel
        );

        {
			string sqlStatement = fmt::format(
                "select refreshToken from MMS_Conf_Twitch where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(twitchConfigurationLabel));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Twitch failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", twitchConfigurationLabel: " + twitchConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
			twitchRefreshToken = res[0]["refreshToken"].as<string>();
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
    
    return twitchRefreshToken;
}

int64_t MMSEngineDBFacade::addTiktokConf(
    int64_t workspaceKey,
    string label,
    string token)
{
    int64_t     confKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_Tiktok(workspaceKey, label, modificationDate, token) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(token));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyTiktokConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string token)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_Tiktok set label = {}, token = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(token), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeTiktokConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_Tiktok where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getTiktokConfList (
	int64_t workspaceKey, int64_t confKey, string label
)
{
	Json::Value tiktokConfListRoot;

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
        
        _logger->info(__FILEREF__ + "getTiktokConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
            + ", label: " + label
        );
        
        {
            Json::Value requestParametersRoot;
            
			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "label";
				requestParametersRoot[field] = label;
			}

            field = "requestParameters";
            tiktokConfListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and label = {} ", trans.quote(label));
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_Tiktok {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value tiktokRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select confKey, label, token, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
                "from MMS_Conf_Tiktok {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value tiktokConfRoot;

				field = "confKey";
				tiktokConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				tiktokConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				tiktokConfRoot[field] = row["modificationDate"].as<string>();

                field = "token";
				tiktokConfRoot[field] = row["token"].as<string>();

                tiktokRoot.append(tiktokConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "tiktokConf";
        responseRoot[field] = tiktokRoot;

        field = "response";
        tiktokConfListRoot[field] = responseRoot;

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
    
    return tiktokConfListRoot;
}

string MMSEngineDBFacade::getTiktokTokenByConfigurationLabel(
    int64_t workspaceKey, string tiktokConfigurationLabel
)
{
    string      tiktokToken;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getTiktokTokenByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", tiktokConfigurationLabel: " + tiktokConfigurationLabel
        );

        {
			string sqlStatement = fmt::format(
                "select token from MMS_Conf_Tiktok where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(tiktokConfigurationLabel));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Tiktok failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", tiktokConfigurationLabel: " + tiktokConfigurationLabel
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
			tiktokToken = res[0]["token"].as<string>();
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
    
    return tiktokToken;
}

Json::Value MMSEngineDBFacade::addStream(
    int64_t workspaceKey,
    string label,
	string sourceType,
	int64_t encodersPoolKey,
	string url,
	string pushProtocol,
	int64_t pushEncoderKey,
	string pushServerName,		// indica il nome del server (public or internal)
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
	int64_t tvSourceTVConfKey,
	string type,
	string description,
	string name,
	string region,
	string country,
	int64_t imageMediaItemKey,
	string imageUniqueName,
	int position,
	Json::Value userData)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_Stream(workspaceKey, label, sourceType, "
				"encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"tvSourceTVConfKey, "
				"type, description, name, "
				"region, country, imageMediaItemKey, imageUniqueName, "
				"position, userData) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(sourceType),
				encodersPoolKey == -1 ? "null" : to_string(encodersPoolKey),
				url == "" ? "null" : trans.quote(url),
				pushProtocol == "" ? "null" : trans.quote(pushProtocol),
				pushEncoderKey == -1 ? "null" : to_string(pushEncoderKey),
				pushServerName == "" ? "null" : trans.quote(pushServerName),
				pushServerPort == -1 ? "null" : to_string(pushServerPort),
				pushUri == "" ? "null" : trans.quote(pushUri),
				pushListenTimeout == -1 ? "null" : to_string(pushListenTimeout),
				captureLiveVideoDeviceNumber == -1 ? "null" : to_string(captureLiveVideoDeviceNumber),
				captureLiveVideoInputFormat == "" ? "null" : trans.quote(captureLiveVideoInputFormat),
				captureLiveFrameRate == -1 ? "null" : to_string(captureLiveFrameRate),
				captureLiveWidth == -1 ? "null" : to_string(captureLiveWidth),
				captureLiveHeight == -1 ? "null" : to_string(captureLiveHeight),
				captureLiveAudioDeviceNumber == -1 ? "null" : to_string(captureLiveAudioDeviceNumber),
				captureLiveChannelsNumber == -1 ? "null" : to_string(captureLiveChannelsNumber),
				tvSourceTVConfKey == -1 ? "null" : to_string(tvSourceTVConfKey),
				type == "" ? "null" : trans.quote(type),
				description == "" ? "null" : trans.quote(description),
				name == "" ? "null" : trans.quote(name),
				region == "" ? "null" : trans.quote(region),
				country == "" ? "null" : trans.quote(country),
				imageMediaItemKey == -1 ? "null" : to_string(imageMediaItemKey),
				imageUniqueName == "" ? "null" : trans.quote(imageUniqueName),
				position == -1 ? "null" : to_string(position),
				userData == Json::nullValue ? "null" : trans.quote(JSONUtils::toString(userData))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		Json::Value streamsRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			bool labelLike = true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value streamListRoot = getStreamList (
				trans, conn, workspaceKey, confKey,
				start, rows, label, labelLike, url, sourceType, type, name,
				region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return streamsRoot[0];
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

Json::Value MMSEngineDBFacade::modifyStream(
    int64_t confKey, string labelKey,
    int64_t workspaceKey,
    bool labelToBeModified, string label,

	bool sourceTypeToBeModified, string sourceType,
	bool encodersPoolKeyToBeModified, int64_t encodersPoolKey,
	bool urlToBeModified, string url,
	bool pushProtocolToBeModified, string pushProtocol,
	bool pushEncoderKeyToBeModified, int64_t pushEncoderKey,
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
	bool tvSourceTVConfKeyToBeModified, int64_t tvSourceTVConfKey,

	bool typeToBeModified, string type,
	bool descriptionToBeModified, string description,
	bool nameToBeModified, string name,
	bool regionToBeModified, string region,
	bool countryToBeModified, string country,
	bool imageToBeModified, int64_t imageMediaItemKey, string imageUniqueName,
	bool positionToBeModified, int position,
	bool userDataToBeModified, Json::Value userData)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.quote(label));
				oneParameterPresent = true;
			}

			if (sourceTypeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("sourceType = " + trans.quote(sourceType));
				oneParameterPresent = true;
			}

			if (encodersPoolKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("encodersPoolKey = " + (encodersPoolKey == -1 ? "null" : to_string(encodersPoolKey)));
				oneParameterPresent = true;
			}

			if (urlToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("url = " + (url == "" ? "null" : trans.quote(url)));
				oneParameterPresent = true;
			}

			if (pushProtocolToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushProtocol = " + (pushProtocol == "" ? "null" : trans.quote(pushProtocol)));
				oneParameterPresent = true;
			}

			if (pushEncoderKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushEncoderKey = " + (pushEncoderKey == -1 ? "null" : to_string(pushEncoderKey)));
				oneParameterPresent = true;
			}

			if (pushServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushServerName = " + (pushServerName == "" ? "null" : trans.quote(pushServerName)));
				oneParameterPresent = true;
			}

			if (pushServerPortToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushServerPort = " + (pushServerPort == -1 ? "null" : to_string(pushServerPort)));
				oneParameterPresent = true;
			}

			if (pushUriToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushUri = " + (pushUri == "" ? "null" : trans.quote(pushUri)));
				oneParameterPresent = true;
			}

			if (pushListenTimeoutToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushListenTimeout = " + (pushListenTimeout == -1 ? "null" : to_string(pushListenTimeout)));
				oneParameterPresent = true;
			}

			if (captureLiveVideoDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveVideoDeviceNumber = " + (captureLiveVideoDeviceNumber == -1 ? "null" : to_string(captureLiveVideoDeviceNumber)));
				oneParameterPresent = true;
			}

			if (captureLiveVideoInputFormatToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveVideoInputFormat = " + (captureLiveVideoInputFormat == "" ? "null" : trans.quote(captureLiveVideoInputFormat)));
				oneParameterPresent = true;
			}

			if (captureLiveFrameRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveFrameRate = " + (captureLiveFrameRate == -1 ? "null" : to_string(captureLiveFrameRate)));
				oneParameterPresent = true;
			}

			if (captureLiveWidthToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveWidth = " + (captureLiveWidth == -1 ? "null" : to_string(captureLiveWidth)));
				oneParameterPresent = true;
			}

			if (captureLiveHeightToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveHeight = " + (captureLiveHeight == -1 ? "null" : to_string(captureLiveHeight)));
				oneParameterPresent = true;
			}

			if (captureLiveAudioDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveAudioDeviceNumber = " + (captureLiveAudioDeviceNumber == -1 ? "null" : to_string(captureLiveAudioDeviceNumber)));
				oneParameterPresent = true;
			}

			if (captureLiveChannelsNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveChannelsNumber = " + (captureLiveChannelsNumber == -1 ? "null" : to_string(captureLiveChannelsNumber)));
				oneParameterPresent = true;
			}

			if (tvSourceTVConfKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tvSourceTVConfKey = " + (tvSourceTVConfKey == -1 ? "null" : to_string(tvSourceTVConfKey)));
				oneParameterPresent = true;
			}

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = " + (type == "" ? "null" : trans.quote(type)));
				oneParameterPresent = true;
			}

			if (descriptionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("description = " + (description == "" ? "null" : trans.quote(description)));
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = " + (name == "" ? "null" : trans.quote(name)));
				oneParameterPresent = true;
			}

			if (regionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("region = " + (region == "" ? "null" : trans.quote(region)));
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = " + (country == "" ? "null" : trans.quote(country)));
				oneParameterPresent = true;
			}

			if (imageToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				if (imageMediaItemKey == -1)
					setSQL += ("imageMediaItemKey = null, imageUniqueName = " + (imageUniqueName == "" ? "null" : trans.quote(imageUniqueName)));
				else
					setSQL += ("imageMediaItemKey = " + to_string(imageMediaItemKey) + ", imageUniqueName = null");
				oneParameterPresent = true;
			}

			if (positionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("position = " + (position == -1 ? "null" : to_string(position)));
				oneParameterPresent = true;
			}

			if (userDataToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("userData = " + (userData == Json::nullValue ? "null" : trans.quote(JSONUtils::toString(userData))));
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

            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_Stream {} "
				"where {} = {} and workspaceKey = {} returning 1) select count(*) from rows",
				setSQL,
				confKey != -1 ? "confKey" : "label",
				confKey != -1 ? to_string(confKey) : trans.quote(label),
				workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
		}

		Json::Value streamsRoot;
		{
			int start = 0;
			int rows = 1;
			// string label;
			// se viene usata la labelKey come chiave per la modifica, labelLike sarà false
			bool labelLike = confKey == -1 ? false : true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			Json::Value streamListRoot = getStreamList (
				trans, conn, workspaceKey, confKey,
				start, rows, labelKey, labelLike, url, sourceType, type, name, region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return streamsRoot[0];
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

void MMSEngineDBFacade::removeStream(
    int64_t workspaceKey,
    int64_t confKey,
	string label)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		if (confKey == -1 && label == "")
		{
			string errorMessage = __FILEREF__ + "Wrong input"
				+ ", confKey: " + to_string(confKey)
				+ ", label: " + label
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);                    
		}

		{
			string sqlStatement;
			if (confKey != -1)
				sqlStatement = fmt::format(
					"WITH rows AS (delete from MMS_Conf_Stream where confKey = {} and workspaceKey = {} "
					"returning 1) select count(*) from rows",
					confKey, workspaceKey);
			else
				sqlStatement = fmt::format(
					"WITH rows AS (delete from MMS_Conf_Stream where label = {} and workspaceKey = {} "
					"returning 1) select count(*) from rows",
					trans.quote(label), workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                    + ", confKey: " + to_string(confKey)
					+ ", label: " + label
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getStreamList (
	int64_t workspaceKey, int64_t liveURLKey,
	int start, int rows,
	string label, bool labelLike, string url, string sourceType, string type,
	string name, string region, string country,
	string labelOrder	// "" or "asc" or "desc"
)
{
    Json::Value streamListRoot;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		streamListRoot = MMSEngineDBFacade::getStreamList (
			trans, conn, workspaceKey, liveURLKey,
			start, rows,
			label, labelLike, url, sourceType, type,
			name, region, country,
			labelOrder);

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
    
    return streamListRoot;
}

Json::Value MMSEngineDBFacade::getStreamList (
	nontransaction& trans, shared_ptr<PostgresConnection> conn,
	int64_t workspaceKey, int64_t liveURLKey,
	int start, int rows,
	string label, bool labelLike, string url, string sourceType, string type,
	string name, string region, string country,
	string labelOrder	// "" or "asc" or "desc"
)
{
    Json::Value streamListRoot;
    
    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getStreamList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", liveURLKey: " + to_string(liveURLKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", label: " + label
            + ", labelLike: " + to_string(labelLike)
            + ", url: " + url
            + ", sourceType: " + sourceType
            + ", type: " + type
            + ", name: " + name
            + ", region: " + region
            + ", country: " + country
            + ", labelOrder: " + labelOrder
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
            
			{
				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}
            
            if (url != "")
			{
				field = "url";
				requestParametersRoot[field] = url;
			}

            if (sourceType != "")
			{
				field = "sourceType";
				requestParametersRoot[field] = sourceType;
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
            streamListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
        if (liveURLKey != -1)
			sqlWhere += fmt::format("and confKey = {} ", liveURLKey);
        if (label != "")
		{
			if (labelLike)
				sqlWhere += fmt::format("and LOWER(label) like LOWER({}) ", trans.quote("%" + label + "%"));
			else
				sqlWhere += fmt::format("and LOWER(label) = LOWER({}) ", trans.quote(label));
		}
        if (url != "")
            sqlWhere += fmt::format("and url like {} ", trans.quote("%" + url + "%"));
        if (sourceType != "")
            sqlWhere += fmt::format("and sourceType = {} ", trans.quote(sourceType));
        if (type != "")
            sqlWhere += fmt::format("and type = {} ", trans.quote(type));
        if (name != "")
            sqlWhere += fmt::format("and LOWER(name) like LOWER({}) ", trans.quote("%" + name + "%"));
        if (region != "")
            sqlWhere += fmt::format("and region like {} ", trans.quote("%" + region + "%"));
        if (country != "")
            sqlWhere += fmt::format("and country like {} ", trans.quote("%" + country + "%"));

        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_Stream {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value streamsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder != "")
				orderByCondition = fmt::format("order by label {}", labelOrder);

			string sqlStatement = fmt::format(
                "select confKey, label, sourceType, encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, "
				"captureLiveVideoInputFormat, captureLiveFrameRate, captureLiveWidth, "
				"captureLiveHeight, captureLiveAudioDeviceNumber, "
				"captureLiveChannelsNumber, tvSourceTVConfKey, "
				"type, description, name, "
				"region, country, "
				"imageMediaItemKey, imageUniqueName, position, userData "
				"from MMS_Conf_Stream {} {} "
				"limit {} offset {}",
                sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value streamRoot;

				int64_t confKey = row["confKey"].as<int64_t>();
                field = "confKey";
                streamRoot[field] = confKey;

                field = "label";
				streamRoot[field] = row["label"].as<string>();

				string sourceType = row["sourceType"].as<string>();
				field = "sourceType";
				streamRoot[field] = sourceType;

                field = "encodersPoolKey";
				if (row["encodersPoolKey"].is_null())
					streamRoot[field] = Json::nullValue;
				else
				{
					int64_t encodersPoolKey = row["encodersPoolKey"].as<int64_t>();
					streamRoot[field] = encodersPoolKey;

					if (encodersPoolKey >= 0)
					{
						try
						{
							string encodersPoolLabel =
								getEncodersPoolDetails (encodersPoolKey);

							field = "encodersPoolLabel";
							streamRoot[field] = encodersPoolLabel;
						}
						catch(exception& e)
						{
							_logger->error(__FILEREF__ + "getEncodersPoolDetails failed"
								+ ", confKey: " + to_string(confKey)
								+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							);
						}
					}
				}

				// if (sourceType == "IP_PULL")
				{
					field = "url";
					if (row["url"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["url"].as<string>();
				}
				// else if (sourceType == "IP_PUSH")
				{
					field = "pushProtocol";
					if (row["pushProtocol"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["pushProtocol"].as<string>();

					field = "pushEncoderKey";
					if (row["pushEncoderKey"].is_null())
						streamRoot[field] = Json::nullValue;
					else
					{
						int64_t pushEncoderKey = row["pushEncoderKey"].as<int64_t>();
						streamRoot[field] = pushEncoderKey;

						if (pushEncoderKey >= 0)
						{
							try
							{
								tuple<string, string, string> encoderDetails
									= getEncoderDetails (pushEncoderKey);

								string pushEncoderLabel;
								tie(pushEncoderLabel, ignore, ignore) = encoderDetails;
	
								field = "pushEncoderLabel";
								streamRoot[field] = pushEncoderLabel;
							}
							catch(exception& e)
							{
								_logger->error(__FILEREF__ + "getEncoderDetails failed"
									+ ", pushEncoderKey: " + to_string(pushEncoderKey)
								);
							}
						}
					}

					field = "pushServerName";
					if (row["pushServerName"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["pushServerName"].as<string>();

					field = "pushServerPort";
					if (row["pushServerPort"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["pushServerPort"].as<int>();

					field = "pushUri";
					if (row["pushUri"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["pushUri"].as<string>();

					field = "pushListenTimeout";
					if (row["pushListenTimeout"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["pushListenTimeout"].as<int>();
				}
				// else if (sourceType == "CaptureLive")
				{
					field = "captureLiveVideoDeviceNumber";
					if (row["captureLiveVideoDeviceNumber"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveVideoDeviceNumber"].as<int>();

					field = "captureLiveVideoInputFormat";
					if (row["captureLiveVideoInputFormat"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveVideoInputFormat"].as<string>();

					field = "captureLiveFrameRate";
					if (row["captureLiveFrameRate"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveFrameRate"].as<int>();

					field = "captureLiveWidth";
					if (row["captureLiveWidth"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveWidth"].as<int>();

					field = "captureLiveHeight";
					if (row["captureLiveHeight"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveHeight"].as<int>();

					field = "captureLiveAudioDeviceNumber";
					if (row["captureLiveAudioDeviceNumber"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveAudioDeviceNumber"].as<int>();

					field = "captureLiveChannelsNumber";
					if (row["captureLiveChannelsNumber"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["captureLiveChannelsNumber"].as<int>();
				}
				// else if (sourceType == "TV")
				{
					field = "tvSourceTVConfKey";
					if (row["tvSourceTVConfKey"].is_null())
						streamRoot[field] = Json::nullValue;
					else
						streamRoot[field] = row["tvSourceTVConfKey"].as<int64_t>();
				}

				field = "type";
				if (row["type"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["type"].as<string>();

                field = "description";
				if (row["description"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["description"].as<string>();

                field = "name";
				if (row["name"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["name"].as<string>();

                field = "region";
				if (row["region"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["region"].as<string>();

                field = "country";
				if (row["country"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["country"].as<string>();

                field = "imageMediaItemKey";
				if (row["imageMediaItemKey"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["imageMediaItemKey"].as<int64_t>();

                field = "imageUniqueName";
				if (row["imageUniqueName"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["imageUniqueName"].as<string>();

                field = "position";
				if (row["position"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["position"].as<int>();

                field = "userData";
				if (row["userData"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["userData"].as<string>();

                streamsRoot.append(streamRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "streams";
        responseRoot[field] = streamsRoot;

        field = "response";
        streamListRoot[field] = responseRoot;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return streamListRoot;
}

tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
	int, string, int, int, int, int, int, int64_t>
	MMSEngineDBFacade::getStreamDetails(
    int64_t workspaceKey, string label,
	bool warningIfMissing
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        _logger->info(__FILEREF__ + "getStreamDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

		int64_t confKey;
		string sourceType;
		string encodersPoolLabel;
		string url;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		string pushServerName;		// indica il nome del server (public or internal)
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
		int64_t tvSourceTVConfKey = -1;
		{
			string sqlStatement = fmt::format(
				"select confKey, sourceType, "
				"encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, "
				"captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"tvSourceTVConfKey "
				"from MMS_Conf_Stream "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
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

			confKey = res[0]["confKey"].as<int64_t>();
			sourceType = res[0]["sourceType"].as<string>();
			if (!res[0]["encodersPoolKey"].is_null())
			{
				int64_t encodersPoolKey = res[0]["encodersPoolKey"].as<int64_t>();

				if (encodersPoolKey >= 0)
				{
					try
					{
						encodersPoolLabel = getEncodersPoolDetails (encodersPoolKey);
					}
					catch(exception& e)
					{
						_logger->error(__FILEREF__ + "getEncodersPoolDetails failed"
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
						);
					}
				}
			}
			if (!res[0]["url"].is_null())
				url = res[0]["url"].as<string>();
			if (!res[0]["pushProtocol"].is_null())
				pushProtocol = res[0]["pushProtocol"].as<string>();
			if (!res[0]["pushEncoderKey"].is_null())
				pushEncoderKey = res[0]["pushEncoderKey"].as<int64_t>();
			if (!res[0]["pushServerName"].is_null())
				pushServerName = res[0]["pushServerName"].as<string>();
			if (!res[0]["pushServerPort"].is_null())
				pushServerPort = res[0]["pushServerPort"].as<int>();
			if (!res[0]["pushUri"].is_null())
				pushUri = res[0]["pushUri"].as<string>();
			if (!res[0]["pushListenTimeout"].is_null())
				pushListenTimeout = res[0]["pushListenTimeout"].as<int>();
			if (!res[0]["captureLiveVideoDeviceNumber"].is_null())
				captureLiveVideoDeviceNumber = res[0]["captureLiveVideoDeviceNumber"].as<int>();
			if (!res[0]["captureLiveVideoInputFormat"].is_null())
				captureLiveVideoInputFormat = res[0]["captureLiveVideoInputFormat"].as<string>();
			if (!res[0]["captureLiveFrameRate"].is_null())
				captureLiveFrameRate = res[0]["captureLiveFrameRate"].as<int>();
			if (!res[0]["captureLiveWidth"].is_null())
				captureLiveWidth = res[0]["captureLiveWidth"].as<int>();
			if (!res[0]["captureLiveHeight"].is_null())
				captureLiveHeight = res[0]["captureLiveHeight"].as<int>();
			if (!res[0]["captureLiveAudioDeviceNumber"].is_null())
				captureLiveAudioDeviceNumber = res[0]["captureLiveAudioDeviceNumber"].as<int>();
			if (!res[0]["captureLiveChannelsNumber"].is_null())
				captureLiveChannelsNumber = res[0]["captureLiveChannelsNumber"].as<int>();
			if (!res[0]["tvSourceTVConfKey"].is_null())
				tvSourceTVConfKey = res[0]["tvSourceTVConfKey"].as<int64_t>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, sourceType, encodersPoolLabel, url,
			pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri, pushListenTimeout,
			captureLiveVideoDeviceNumber, captureLiveVideoInputFormat,
            captureLiveFrameRate, captureLiveWidth, captureLiveHeight,
			captureLiveAudioDeviceNumber, captureLiveChannelsNumber,
			tvSourceTVConfKey);
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
    catch(ConfKeyNotFound& e)
    {
		if (warningIfMissing)
			SPDLOG_WARN("ConfKeyNotFound SQL exception"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		else
			SPDLOG_ERROR("ConfKeyNotFound SQL exception"
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

tuple<string, string, string> MMSEngineDBFacade::getStreamDetails(
    int64_t workspaceKey, int64_t confKey
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getStreamDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", confKey: " + to_string(confKey)
        );

		string		url;
		string		channelName;
		string		userData;
        {
			string sqlStatement = fmt::format(
				"select url, name, userData from MMS_Conf_Stream "
				"where workspaceKey = {} and confKey = {}",
				workspaceKey, confKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_Stream failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", confKey: " + to_string(confKey)
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

			url = res[0]["url"].as<string>();
			channelName = res[0]["name"].as<string>();
			userData = res[0]["userData"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(url, channelName, userData);
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

Json::Value MMSEngineDBFacade::addSourceTVStream(
	string type,
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
	int64_t bandwidthInHz,
	string country,
	string deliverySystem
)
{
	int64_t		confKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_SourceTVStream( "
				"type, serviceId, networkId, transportStreamId, "
				"name, satellite, frequency, lnb, "
				"videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid, "
				"modulation, polarization, symbolRate, bandwidthInHz, "
				"country, deliverySystem "
				") values ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, {}, {}, {}) returning confKey returning confKey",
				trans.quote(type),
				serviceId == -1 ? "null" : to_string(serviceId),
				networkId == -1 ? "null" : to_string(networkId),
				transportStreamId == -1 ? "null" : to_string(transportStreamId),
				trans.quote(name),
				satellite == "" ? "null" : trans.quote(satellite),
				frequency,
				lnb == "" ? "null" : trans.quote(lnb),
				videoPid == -1 ? "null" : to_string(videoPid),
				audioPids == "" ? "null" : trans.quote(audioPids),
				audioItalianPid == -1 ? "null" : to_string(audioItalianPid),
				audioEnglishPid == -1 ? "null" : to_string(audioEnglishPid),
				teletextPid == -1 ? "null" : to_string(teletextPid),
				modulation == "" ? "null" : trans.quote(modulation),
				polarization == "" ? "null" : trans.quote(polarization),
				symbolRate == -1 ? "null" : to_string(symbolRate),
				bandwidthInHz == -1 ? "null" : to_string(bandwidthInHz),
				country == "" ? "null" : trans.quote(country),
				deliverySystem == "" ? "null" : trans.quote(deliverySystem)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		Json::Value sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceTVStreamRoot = getSourceTVStreamList (
				confKey, start, rows, type, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong channelConf";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceTVStreamsRoot[0];
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

Json::Value MMSEngineDBFacade::modifySourceTVStream(
	int64_t confKey,

	bool typeToBeModified, string type,
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
	bool bandwidthInHzToBeModified, int64_t bandwidthInHz,
	bool countryToBeModified, string country,
	bool deliverySystemToBeModified, string deliverySystem
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = " + (type == "" ? "null" : trans.quote(type)));
				oneParameterPresent = true;
			}

			if (serviceIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("serviceId = " + (serviceId == -1 ? "null" : to_string(serviceId)));
				oneParameterPresent = true;
			}

			if (networkIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("networkId = " + (networkId == -1 ? "null" : to_string(networkId)));
				oneParameterPresent = true;
			}

			if (transportStreamIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("transportStreamId = " + (transportStreamId == -1 ? "null" : to_string(transportStreamId)));
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = " + (name == "" ? "null" : trans.quote(name)));
				oneParameterPresent = true;
			}

			if (satelliteToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("satellite = " + (satellite == "" ? "null" : trans.quote(satellite)));
				oneParameterPresent = true;
			}

			if (frequencyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("frequency = " + (frequency == -1 ? "null" : to_string(frequency)));
				oneParameterPresent = true;
			}

			if (lnbToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("lnb = " + (lnb == "" ? "null" : trans.quote(lnb)));
				oneParameterPresent = true;
			}

			if (videoPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("videoPid = " + (videoPid == -1 ? "null" : to_string(videoPid)));
				oneParameterPresent = true;
			}

			if (audioPidsToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioPids = " + (audioPids == "" ? "null" : trans.quote(audioPids)));
				oneParameterPresent = true;
			}

			if (audioItalianPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioItalianPid = " + (audioItalianPid == -1 ? "null" : to_string(audioItalianPid)));
				oneParameterPresent = true;
			}

			if (audioEnglishPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioEnglishPid = " + (audioEnglishPid == -1 ? "null" : to_string(audioEnglishPid)));
				oneParameterPresent = true;
			}

			if (teletextPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("teletextPid = " + (teletextPid == -1 ? "null" : to_string(teletextPid)));
				oneParameterPresent = true;
			}

			if (modulationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("modulation = " + (modulation == "" ? "null" : trans.quote(modulation)));
				oneParameterPresent = true;
			}

			if (polarizationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("polarization = " + (polarization == "" ? "null" : trans.quote(polarization)));
				oneParameterPresent = true;
			}

			if (symbolRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("symbolRate = " + (symbolRate == -1 ? "null" : to_string(symbolRate)));
				oneParameterPresent = true;
			}

			if (bandwidthInHzToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("bandwidthInHz = " + (bandwidthInHz == -1 ? "null" : to_string(bandwidthInHz)));
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = " + (country == "" ? "null" : trans.quote(country)));
				oneParameterPresent = true;
			}

			if (deliverySystemToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("deliverySystem = " + (deliverySystem == "" ? "null" : trans.quote(deliverySystem)));
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

            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_SourceTVStream {} "
				"where confKey = {} returning 1) select count(*) from rows",
				setSQL, confKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }

		Json::Value sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId;
			string name;
			int64_t frequency;
			string lnb;
			int videoPid;
			string audioPids;
			string nameOrder;
			Json::Value sourceTVStreamRoot = getSourceTVStreamList (
				confKey, start, rows, type, serviceId, name, frequency, lnb,
				videoPid, audioPids, nameOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			Json::Value responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = __FILEREF__ + "Field is not present or it is null"
					+ ", Field: " + field;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = __FILEREF__ + "Wrong streams";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return sourceTVStreamsRoot[0];
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

void MMSEngineDBFacade::removeSourceTVStream(
	int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_SourceTVStream where confKey = {} "
				"returning 1) select count(*) from rows",
				confKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
					+ ", confKey: " + to_string(confKey)
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getSourceTVStreamList (
	int64_t confKey,
	int start, int rows,
	string type, int64_t serviceId, string name, int64_t frequency, string lnb,
	int videoPid, string audioPids,
	string nameOrder)
{
    Json::Value streamListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getSourceTVStreamList"
            + ", confKey: " + to_string(confKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", type: " + type
            + ", frequency: " + to_string(frequency)
            + ", lnb: " + lnb
            + ", serviceId: " + to_string(serviceId)
            + ", name: " + name
            + ", videoPid: " + to_string(videoPid)
            + ", audioPids: " + audioPids
            + ", nameOrder: " + nameOrder
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
            
            if (type != "")
			{
				field = "type";
				requestParametersRoot[field] = type;
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
            streamListRoot[field] = requestParametersRoot;
        }
        
		string sqlWhere;
        if (confKey != -1)
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.confKey = {} ", confKey);
			else
				sqlWhere += fmt::format("and sc.confKey = {} ", confKey);
		}
        if (type != "")
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.type = {} ", trans.quote(type));
			else
				sqlWhere += fmt::format("and sc.type = {} ", trans.quote(type));
		}
        if (serviceId != -1)
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.serviceId = {} ", serviceId);
			else
				sqlWhere += fmt::format("and sc.serviceId = {} ", serviceId);
		}
        if (name != "")
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("LOWER(sc.name) like LOWER({}) ", trans.quote("%" + name + "%"));
			else
				sqlWhere += fmt::format("and LOWER(sc.name) like LOWER({}) ", trans.quote("%" + name + "%"));
		}
        if (frequency != -1)
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.frequency = {} ", frequency);
			else
				sqlWhere += fmt::format("and sc.frequency = {} ", frequency);
		}
        if (lnb != "")
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("LOWER(sc.lnb) like LOWER({}) ", trans.quote("%" + lnb + "%"));
			else
				sqlWhere += fmt::format("and LOWER(sc.lnb) like LOWER({}) ", trans.quote("%" + lnb + "%"));
		}
        if (videoPid != -1)
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.videoPid = {} ", videoPid);
			else
				sqlWhere += fmt::format("and sc.videoPid = {} ", videoPid);
		}
        if (audioPids != "")
		{
			if (sqlWhere == "")
				sqlWhere += fmt::format("sc.audioPids = {} ", trans.quote(audioPids));
			else
				sqlWhere += fmt::format("and sc.audioPids = {} ", trans.quote(audioPids));
		}
		if (sqlWhere != "")
			sqlWhere = fmt::format("where {}", sqlWhere);

        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
				"select count(*) from MMS_Conf_SourceTVStream sc {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value streamsRoot(Json::arrayValue);
        {
			string orderByCondition;
				orderByCondition = fmt::format("order by sc.name {}", nameOrder);

			string sqlStatement = fmt::format(
				"select sc.confKey, sc.type, sc.serviceId, "
				"sc.networkId, sc.transportStreamId, sc.name, sc.satellite, "
				"sc.frequency, sc.lnb, sc.videoPid, sc.audioPids, "
				"sc.audioItalianPid, sc.audioEnglishPid, sc.teletextPid, "
				"sc.modulation, sc.polarization, sc.symbolRate, sc.bandwidthInHz, "
				"sc.country, sc.deliverySystem "
				"from MMS_Conf_SourceTVStream sc {} {} "
				"limit {} offset {}",
				sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value streamRoot;

                field = "confKey";
				streamRoot[field] = row["confKey"].as<int64_t>();

                field = "type";
				streamRoot[field] = row["type"].as<string>();

                field = "serviceId";
				if (row["serviceId"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["serviceId"].as<int>();

                field = "networkId";
				if (row["networkId"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["networkId"].as<int>();

                field = "transportStreamId";
				if (row["transportStreamId"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["transportStreamId"].as<int>();

                field = "name";
				streamRoot[field] = row["name"].as<string>();

                field = "satellite";
				if (row["satellite"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["satellite"].as<string>();

                field = "frequency";
				streamRoot[field] = row["frequency"].as<int64_t>();

                field = "lnb";
				if (row["lnb"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["lnb"].as<string>();

                field = "videoPid";
				if (row["videoPid"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["videoPid"].as<int>();

                field = "audioPids";
				if (row["audioPids"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["audioPids"].as<string>();

                field = "audioItalianPid";
				if (row["audioItalianPid"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["audioItalianPid"].as<int>();

                field = "audioEnglishPid";
				if (row["audioEnglishPid"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["audioEnglishPid"].as<int>();

                field = "teletextPid";
				if (row["teletextPid"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["teletextPid"].as<int>();

                field = "modulation";
				if (row["modulation"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["modulation"].as<string>();

                field = "polarization";
				if (row["polarization"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["polarization"].as<string>();

                field = "symbolRate";
				if (row["symbolRate"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["symbolRate"].as<int>();

                field = "bandwidthInHz";
				if (row["bandwidthInHz"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["bandwidthInHz"].as<int>();

				field = "country";
				if (row["country"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["country"].as<string>();

                field = "deliverySystem";
				if (row["deliverySystem"].is_null())
					streamRoot[field] = Json::nullValue;
				else
					streamRoot[field] = row["deliverySystem"].as<string>();

                streamsRoot.append(streamRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "sourceTVStreams";
        responseRoot[field] = streamsRoot;

        field = "response";
        streamListRoot[field] = responseRoot;

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
    
    return streamListRoot;
}

tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
	MMSEngineDBFacade::getSourceTVStreamDetails(
	int64_t confKey, bool warningIfMissing
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        _logger->info(__FILEREF__ + "getTVStreamDetails"
            + ", confKey: " + to_string(confKey)
        );

		string type;
		int64_t serviceId;
		int64_t frequency;
		int64_t symbolRate;
		int64_t bandwidthInHz;
		string modulation;
		int videoPid;
		int audioItalianPid;
        {
			string sqlStatement = fmt::format(
				"select type, serviceId, frequency, symbolRate, "
				"bandwidthInHz, modulation, videoPid, audioItalianPid "
				"from MMS_Conf_SourceTVStream "
				"where confKey = {}",
				confKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
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

			type = res[0]["type"].as<string>();
			serviceId = res[0]["serviceId"].as<int>();
			frequency = res[0]["frequency"].as<int64_t>();
			if (res[0]["symbolRate"].is_null())
				symbolRate = -1;
			else
				symbolRate = res[0]["symbolRate"].as<int>();
			if (res[0]["bandwidthInHz"].is_null())
				bandwidthInHz = -1;
			else
				bandwidthInHz = res[0]["bandwidthInHz"].as<int>();
			if (!res[0]["modulation"].is_null())
				modulation = res[0]["modulation"].as<string>();
			videoPid = res[0]["videoPid"].as<int>();
			audioItalianPid = res[0]["audioItalianPid"].as<int>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(type, serviceId, frequency, symbolRate, bandwidthInHz,
			modulation, videoPid, audioItalianPid);
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
    catch(ConfKeyNotFound& e)
	{
		if (warningIfMissing)
			SPDLOG_WARN("ConfKeyNotFound SQL exception"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);
		else
			SPDLOG_ERROR("ConfKeyNotFound SQL exception"
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

int64_t MMSEngineDBFacade::addAWSChannelConf(
	int64_t workspaceKey,
	string label,
	string channelId, string rtmpURL, string playURL,
	string type)
{
    int64_t     confKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
                "insert into MMS_Conf_AWSChannel(workspaceKey, label, channelId, "
				"rtmpURL, playURL, type) values ("
                "{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(channelId),
				trans.quote(rtmpURL), trans.quote(playURL), trans.quote(type));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyAWSChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
	string channelId, string rtmpURL, string playURL,
	string type)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_AWSChannel set label = {}, channelId = {}, rtmpURL = {}, "
				"playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(channelId), trans.quote(rtmpURL),
				trans.quote(playURL), trans.quote(type),
				confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeAWSChannelConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_AWSChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getAWSChannelConfList (
	int64_t workspaceKey, int64_t confKey, string label,
	int type	// 0: all, 1: SHARED, 2: DEDICATED
)
{
    Json::Value awsChannelConfListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getAWSChannelConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;

                field = "confKey";
                requestParametersRoot[field] = confKey;

                field = "label";
                requestParametersRoot[field] = label;
            }
            
            field = "requestParameters";
            awsChannelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = fmt::format("where ac.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)                                                                                    
			sqlWhere += fmt::format("and ac.confKey = {} ", confKey);
		else if (label != "")                                                                                 
			sqlWhere += fmt::format("and ac.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and ac.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and ac.type = 'DEDICATED' ";
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_AWSChannel ac {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value awsChannelRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
				"select ac.confKey, ac.label, ac.channelId, ac.rtmpURL, ac.playURL, "
				"ac.type, ac.outputIndex, ac.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_AWSChannel ac left join MMS_IngestionJob ij "
				"on ac.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by ac.label ",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value awsChannelConfRoot;

                field = "confKey";
				awsChannelConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				awsChannelConfRoot[field] = row["label"].as<string>();

                field = "channelId";
				awsChannelConfRoot[field] = row["channelId"].as<string>();

                field = "rtmpURL";
				awsChannelConfRoot[field] = row["rtmpURL"].as<string>();

                field = "playURL";
				awsChannelConfRoot[field] = row["playURL"].as<string>();

                field = "type";
				awsChannelConfRoot[field] = row["type"].as<string>();

                field = "outputIndex";
				if (row["outputIndex"].is_null())
					awsChannelConfRoot[field] = Json::nullValue;
				else
					awsChannelConfRoot[field] = row["outputIndex"].as<int>();

                field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					awsChannelConfRoot[field] = Json::nullValue;
				else
					awsChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

                field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					awsChannelConfRoot[field] = Json::nullValue;
				else
					awsChannelConfRoot[field] = row["configurationLabel"].as<string>();

                awsChannelRoot.append(awsChannelConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "awsChannelConf";
        responseRoot[field] = awsChannelRoot;

        field = "response";
        awsChannelConfListRoot[field] = responseRoot;

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
    
    return awsChannelConfListRoot;
}

tuple<string, string, string, bool>
	MMSEngineDBFacade::reserveAWSChannel(
	int64_t workspaceKey, string label,
	int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "reserveAWSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", label: " + label
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_AWSChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
        {
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
					"select distinct reservedByIngestionJobKey from MMS_Conf_AWSChannel where "
					"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row: res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveAWSChannel. "
						+ "The following AWS channels are reserved but the relative ingestionJobKey is finished,"
						+ "so they will be reset"
						+ ", ingestionJobKeyList: " + ingestionJobKeyList
					;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated == 0)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);                    
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedChannelId;
		string reservedRtmpURL;
		string reservedPlayURL;
		int64_t reservedByIngestionJobKey = -1;

		{

			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				sqlStatement = fmt::format(
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel " 
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {})) "
					"order by reservedByIngestionJobKey desc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel " 
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = __FILEREF__ + "No AWS Channel found"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedChannelId = res[0]["channelId"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			reservedPlayURL = res[0]["playURL"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedChannelId, reservedRtmpURL,
			reservedPlayURL, channelAlreadyReserved);
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

string MMSEngineDBFacade::releaseAWSChannel(
	int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "releaseAWSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

        {
			string sqlStatement = fmt::format(
				"select confKey, channelId from MMS_Conf_AWSChannel " 
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = string("No AWS Channel found")
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedChannelId = res[0]["channelId"].as<string>();
		}

        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return reservedChannelId;
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

int64_t MMSEngineDBFacade::addCDN77ChannelConf(
	int64_t workspaceKey,
	string label, string rtmpURL, string resourceURL, string filePath,
	string secureToken, string type)
{
    int64_t     confKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
                "insert into MMS_Conf_CDN77Channel(workspaceKey, label, rtmpURL, "
				"resourceURL, filePath, secureToken, type) values ("
                "{}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(rtmpURL),
				trans.quote(resourceURL), trans.quote(filePath),
				secureToken == "" ? "null" : trans.quote(secureToken),
				trans.quote(type));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyCDN77ChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
	string label, string rtmpURL, string resourceURL, string filePath,
	string secureToken, string type)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_CDN77Channel set label = {}, rtmpURL = {}, resourceURL = {}, "
				"filePath = {}, secureToken = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(rtmpURL), trans.quote(resourceURL), trans.quote(filePath),
				secureToken == "" ? "null" : trans.quote(secureToken),
				trans.quote(type), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeCDN77ChannelConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_CDN77Channel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getCDN77ChannelConfList (
	int64_t workspaceKey, int64_t confKey, string label,
	int type	// 0: all, 1: SHARED, 2: DEDICATED
)
{
    Json::Value cdn77ChannelConfListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getCDN77ChannelConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;

                field = "confKey";
                requestParametersRoot[field] = confKey;

                field = "label";
                requestParametersRoot[field] = label;
            }
            
            field = "requestParameters";
            cdn77ChannelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = fmt::format("where cc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and cc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and cc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and cc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and cc.type = 'DEDICATED' ";
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_CDN77Channel cc {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value cdn77ChannelRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
				"select cc.confKey, cc.label, cc.rtmpURL, cc.resourceURL, cc.filePath, "
				"cc.secureToken, cc.type, cc.outputIndex, cc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_CDN77Channel cc left join MMS_IngestionJob ij "
				"on cc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by cc.label ",
                sqlWhere);
			;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value cdn77ChannelConfRoot;

                field = "confKey";
				cdn77ChannelConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				cdn77ChannelConfRoot[field] = row["label"].as<string>();

                field = "rtmpURL";
				cdn77ChannelConfRoot[field] = row["rtmpURL"].as<string>();

                field = "resourceURL";
				cdn77ChannelConfRoot[field] = row["resourceURL"].as<string>();

                field = "filePath";
				cdn77ChannelConfRoot[field] = row["filePath"].as<string>();

                field = "secureToken";
				if (row["secureToken"].is_null())
					cdn77ChannelConfRoot[field] = Json::nullValue;
				else
					cdn77ChannelConfRoot[field] = row["secureToken"].as<string>();

                field = "type";
				cdn77ChannelConfRoot[field] = row["type"].as<string>();

                field = "outputIndex";
				if (row["outputIndex"].is_null())
					cdn77ChannelConfRoot[field] = Json::nullValue;
				else
					cdn77ChannelConfRoot[field] = row["outputIndex"].as<int>();

                field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					cdn77ChannelConfRoot[field] = Json::nullValue;
				else
					cdn77ChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

                field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					cdn77ChannelConfRoot[field] = Json::nullValue;
				else
					cdn77ChannelConfRoot[field] = row["configurationLabel"].as<string>();

                cdn77ChannelRoot.append(cdn77ChannelConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "cdn77ChannelConf";
        responseRoot[field] = cdn77ChannelRoot;

        field = "response";
        cdn77ChannelConfListRoot[field] = responseRoot;

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
    
    return cdn77ChannelConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getCDN77ChannelDetails (
	int64_t workspaceKey, string label)
{
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
        
        _logger->info(__FILEREF__ + "getCDN77ChannelDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
		string resourceURL;
		string filePath;
		string secureToken;
        {
			string sqlStatement = fmt::format(
				"select resourceURL, filePath, secureToken "
				"from MMS_Conf_CDN77Channel "
                "where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = __FILEREF__ + "Configuration label is not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);                    
            }

			resourceURL = res[0]["resourceURL"].as<string>();
			filePath = res[0]["filePath"].as<string>();
			if (!res[0]["secureToken"].is_null())
				secureToken = res[0]["secureToken"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(resourceURL, filePath, secureToken);
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
    catch(ConfKeyNotFound& e)
	{
		SPDLOG_ERROR("ConfKeyNotFound SQL exception"
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

tuple<string, string, string, string, string, bool>
	MMSEngineDBFacade::reserveCDN77Channel(
	int64_t workspaceKey, string label,
	int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "reserveCDN77Channel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", label: " + label
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_CDN77Channel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 CDN77)
        {
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
					"select distinct reservedByIngestionJobKey from MMS_Conf_CDN77Channel where "
					"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row: res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveCDN77Channel. "
						+ "The following CDN77 channels are reserved but the relative ingestionJobKey is finished,"
						+ "so they will be reset"
						+ ", ingestionJobKeyList: " + ingestionJobKeyList
					;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated == 0)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);                    
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		string reservedRtmpURL;
		string reservedResourceURL;
		string reservedFilePath;
		string reservedSecureToken;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, resourceURL, filePath, secureToken, "
					"reservedByIngestionJobKey from MMS_Conf_CDN77Channel " 
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {}))"
					"order by reservedByIngestionJobKey desc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, resourceURL, filePath, secureToken, "
					"reservedByIngestionJobKey from MMS_Conf_CDN77Channel " 
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = __FILEREF__ + "No CDN77 Channel found"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			reservedResourceURL = res[0]["resourceURL"].as<string>();
			reservedFilePath = res[0]["filePath"].as<string>();
			reservedSecureToken = res[0]["secureToken"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedRtmpURL, reservedResourceURL, reservedFilePath,
			reservedSecureToken, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseCDN77Channel(
	int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "releaseCDN77Channel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

        {
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_CDN77Channel " 
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = string("No CDN77 Channel found")
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
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

int64_t MMSEngineDBFacade::addRTMPChannelConf(
	int64_t workspaceKey,
	string label, string rtmpURL, string streamName, string userName,
	string password, string playURL, string type)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_RTMPChannel(workspaceKey, label, rtmpURL, "
				"streamName, userName, password, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(rtmpURL),
				streamName == "" ? "null" : trans.quote(streamName),
				userName == "" ? "null" : trans.quote(userName),
				password == "" ? "null" : trans.quote(password),
				playURL == "" ? "null" : trans.quote(playURL),
				trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyRTMPChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
	string label, string rtmpURL, string streamName, string userName,
	string password, string playURL, string type)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_RTMPChannel set label = {}, rtmpURL = {}, streamName = {}, "
				"userName = {}, password = {}, playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(rtmpURL),
				streamName == "" ? "null" : trans.quote(streamName),
				userName == "" ? "null" : trans.quote(userName),
				password == "" ? "null" : trans.quote(password),
				playURL == "" ? "null" : trans.quote(playURL),
				trans.quote(type), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeRTMPChannelConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_RTMPChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getRTMPChannelConfList (
	int64_t workspaceKey, int64_t confKey, string label,
	int type	// 0: all, 1: SHARED, 2: DEDICATED
)
{
    Json::Value rtmpChannelConfListRoot;

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
        
        _logger->info(__FILEREF__ + "getRTMPChannelConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;

                field = "confKey";
                requestParametersRoot[field] = confKey;

                field = "label";
                requestParametersRoot[field] = label;
            }
            
            field = "requestParameters";
            rtmpChannelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = fmt::format("where rc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and rc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and rc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and rc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and rc.type = 'DEDICATED' ";

        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_RTMPChannel rc {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value rtmpChannelRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
				"select rc.confKey, rc.label, rc.rtmpURL, rc.streamName, rc.userName, rc.password, "
				"rc.playURL, rc.type, rc.outputIndex, rc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_RTMPChannel rc left join MMS_IngestionJob ij "
				"on rc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by rc.label ",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value rtmpChannelConfRoot;

                field = "confKey";
				rtmpChannelConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				rtmpChannelConfRoot[field] = row["label"].as<string>();

                field = "rtmpURL";
				rtmpChannelConfRoot[field] = row["rtmpURL"].as<string>();

                field = "streamName";
				if (row["streamName"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["streamName"].as<string>();

                field = "userName";
				if (row["userName"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["userName"].as<string>();

                field = "password";
				if (row["password"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["password"].as<string>();

                field = "playURL";
				if (row["playURL"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["playURL"].as<string>();

                field = "type";
				rtmpChannelConfRoot[field] = row["type"].as<string>();

                field = "outputIndex";
				if (row["outputIndex"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["outputIndex"].as<int>();

                field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

                field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					rtmpChannelConfRoot[field] = Json::nullValue;
				else
					rtmpChannelConfRoot[field] = row["configurationLabel"].as<string>();

                rtmpChannelRoot.append(rtmpChannelConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "rtmpChannelConf";
        responseRoot[field] = rtmpChannelRoot;

        field = "response";
        rtmpChannelConfListRoot[field] = responseRoot;

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
    
    return rtmpChannelConfListRoot;
}

tuple<int64_t, string, string, string, string, string> MMSEngineDBFacade::getRTMPChannelDetails (
	int64_t workspaceKey, string label, bool warningIfMissing)
{
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
        
        _logger->info(__FILEREF__ + "getRTMPChannelDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
		int64_t confKey;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
        {
			string sqlStatement = fmt::format(
				"select confKey, rtmpURL, streamName, userName, password, playURL "
				"from MMS_Conf_RTMPChannel "
                "where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
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

			confKey = res[0]["confKey"].as<int64_t>();
			rtmpURL = res[0]["rtmpURL"].as<string>();
			if (!res[0]["streamName"].is_null())
				streamName = res[0]["streamName"].as<string>();
			if (!res[0]["userName"].is_null())
				userName = res[0]["userName"].as<string>();
			if (!res[0]["password"].is_null())
				password = res[0]["password"].as<string>();
			if (!res[0]["playURL"].is_null())
				playURL = res[0]["playURL"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, rtmpURL, streamName, userName, password, playURL);
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
    catch(ConfKeyNotFound& e)
	{
		SPDLOG_ERROR("ConfKeyNotFound SQL exception"
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

tuple<string, string, string, string, string, string, bool>
	MMSEngineDBFacade::reserveRTMPChannel(
	int64_t workspaceKey, string label,
	int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "reserveRTMPChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", label: " + label
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 RTMP)
        {
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
					"select distinct reservedByIngestionJobKey from MMS_Conf_RTMPChannel where "
					"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row: res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveRTMPChannel. "
						+ "The following RTMP channels are reserved but the relative ingestionJobKey is finished,"
						+ "so they will be reset"
						+ ", ingestionJobKeyList: " + ingestionJobKeyList
					;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated == 0)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", sqlStatement " + sqlStatement
						;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);                    
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		string reservedRtmpURL;
		string reservedStreamName;
		string reservedUserName;
		string reservedPassword;
		string reservedPlayURL;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, streamName, userName, password, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_RTMPChannel " 
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {}))"
					"order by reservedByIngestionJobKey desc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, streamName, userName, password, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_RTMPChannel " 
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = __FILEREF__ + "No RTMP Channel found"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			if (!res[0]["streamName"].is_null())
				reservedStreamName = res[0]["streamName"].as<string>();
			if (!res[0]["userName"].is_null())
				reservedUserName = res[0]["userName"].as<string>();
			if (!res[0]["password"].is_null())
				reservedPassword = res[0]["password"].as<string>();
			if (!res[0]["playURL"].is_null())
				reservedPlayURL = res[0]["playURL"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedRtmpURL, reservedStreamName, reservedUserName,
			reservedPassword, reservedPlayURL, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseRTMPChannel(
	int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "releaseRTMPChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

        {
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_RTMPChannel " 
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = string("No RTMP Channel found")
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
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

int64_t MMSEngineDBFacade::addHLSChannelConf(
	int64_t workspaceKey,
	string label, int64_t deliveryCode, int segmentDuration,
	int playlistEntriesNumber, string type)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_HLSChannel(workspaceKey, label, deliveryCode, "
				"segmentDuration, playlistEntriesNumber, type) values ("
				"{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(deliveryCode),
				segmentDuration == -1 ? "null" : to_string(segmentDuration),
				playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber),
				trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyHLSChannelConf(
    int64_t confKey,
    int64_t workspaceKey,
	string label, int64_t deliveryCode, int segmentDuration,
	int playlistEntriesNumber, string type)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_HLSChannel set label = {}, deliveryCode = {}, segmentDuration = {}, "
				"playlistEntriesNumber = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), deliveryCode,
				segmentDuration == -1 ? "null" : to_string(segmentDuration),
				playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber),
				trans.quote(type), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeHLSChannelConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_HLSChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getHLSChannelConfList (
	int64_t workspaceKey, int64_t confKey, string label,
	int type	// 0: all, 1: SHARED, 2: DEDICATED
)
{
    Json::Value hlsChannelConfListRoot;

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
        
        _logger->info(__FILEREF__ + "getHLSChannelConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
        {
            Json::Value requestParametersRoot;
            
            {
                field = "workspaceKey";
                requestParametersRoot[field] = workspaceKey;

                field = "confKey";
                requestParametersRoot[field] = confKey;

                field = "label";
                requestParametersRoot[field] = label;
            }
            
            field = "requestParameters";
            hlsChannelConfListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = fmt::format("where hc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and hc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and hc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and hc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and hc.type = 'DEDICATED' ";

        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_HLSChannel hc {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value hlsChannelRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
				"select hc.confKey, hc.label, hc.deliveryCode, hc.segmentDuration, hc.playlistEntriesNumber, "
				"hc.type, hc.outputIndex, hc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_HLSChannel hc left join MMS_IngestionJob ij "
				"on hc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by hc.label ",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value hlsChannelConfRoot;

                field = "confKey";
				hlsChannelConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				hlsChannelConfRoot[field] = row["label"].as<string>();

                field = "deliveryCode";
				hlsChannelConfRoot[field] = row["deliveryCode"].as<int64_t>();

                field = "segmentDuration";
				if (row["segmentDuration"].is_null())
					hlsChannelConfRoot[field] = Json::nullValue;
				else
					hlsChannelConfRoot[field] = row["segmentDuration"].as<int>();

                field = "playlistEntriesNumber";
				if (row["playlistEntriesNumber"].is_null())
					hlsChannelConfRoot[field] = Json::nullValue;
				else
					hlsChannelConfRoot[field] = row["playlistEntriesNumber"].as<int>();

                field = "type";
				hlsChannelConfRoot[field] = row["type"].as<string>();

                field = "outputIndex";
				if (row["outputIndex"].is_null())
					hlsChannelConfRoot[field] = Json::nullValue;
				else
					hlsChannelConfRoot[field] = row["outputIndex"].as<int>();

                field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					hlsChannelConfRoot[field] = Json::nullValue;
				else
					hlsChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

                field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					hlsChannelConfRoot[field] = Json::nullValue;
				else
					hlsChannelConfRoot[field] = row["configurationLabel"].as<string>();

                hlsChannelRoot.append(hlsChannelConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "hlsChannelConf";
        responseRoot[field] = hlsChannelRoot;

        field = "response";
        hlsChannelConfListRoot[field] = responseRoot;

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
    
    return hlsChannelConfListRoot;
}

tuple<int64_t, int64_t, int, int> MMSEngineDBFacade::getHLSChannelDetails (
	int64_t workspaceKey, string label, bool warningIfMissing)
{
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
        
        _logger->info(__FILEREF__ + "getHLSChannelDetails"
            + ", workspaceKey: " + to_string(workspaceKey)
        );
        
		int64_t confKey;
		int64_t deliveryCode;
		int segmentDuration;
		int playlistEntriesNumber;
        {
			string sqlStatement = fmt::format(
				"select confKey, deliveryCode, segmentDuration, playlistEntriesNumber "
				"from MMS_Conf_HLSChannel "
                "where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
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

			confKey = res[0]["confKey"].as<int64_t>();
			deliveryCode = res[0]["deliveryCode"].as<int64_t>();
			if (!res[0]["segmentDuration"].is_null())
				segmentDuration = res[0]["segmentDuration"].as<int>();
			if (!res[0]["playlistEntriesNumber"].is_null())
				playlistEntriesNumber = res[0]["playlistEntriesNumber"].as<int>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, deliveryCode, segmentDuration, playlistEntriesNumber);
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
    catch(ConfKeyNotFound& e)
	{
		SPDLOG_ERROR("ConfKeyNotFound SQL exception"
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

tuple<string, int64_t, int, int, bool>
	MMSEngineDBFacade::reserveHLSChannel(
	int64_t workspaceKey, string label,
	int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "reserveHLSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", label: " + label
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 HLS)
        {
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
					"select distinct reservedByIngestionJobKey from MMS_Conf_HLSChannel where "
					"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row: res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveHLSChannel. "
						+ "The following HLS channels are reserved but the relative ingestionJobKey is finished,"
						+ "so they will be reset"
						+ ", ingestionJobKeyList: " + ingestionJobKeyList
					;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated == 0)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", sqlStatement " + sqlStatement
						;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);                    
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		int64_t reservedDeliveryCode = -1;
		int reservedSegmentDuration = -1;
		int reservedPlaylistEntriesNumber = -1;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				sqlStatement = fmt::format(
					"select confKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, "
					"reservedByIngestionJobKey from MMS_Conf_HLSChannel " 
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {}))"
					"order by reservedByIngestionJobKey desc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, "
					"reservedByIngestionJobKey from MMS_Conf_HLSChannel " 
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = __FILEREF__ + "No HLS Channel found"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedDeliveryCode = res[0]["deliveryCode"].as<int64_t>();
			if (!res[0]["segmentDuration"].is_null())
				reservedSegmentDuration = res[0]["segmentDuration"].as<int>();
			if (!res[0]["playlistEntriesNumber"].is_null())
				reservedPlaylistEntriesNumber = res[0]["playlistEntriesNumber"].as<int>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedDeliveryCode, reservedSegmentDuration,
			reservedPlaylistEntriesNumber, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseHLSChannel(
	int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        string field;
        
		_logger->info(__FILEREF__ + "releaseHLSChannel"
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", outputIndex: " + to_string(outputIndex)
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

        {
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_HLSChannel " 
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
			{
				string errorMessage = string("No HLS Channel found")
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey);
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
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(reservedConfKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
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

int64_t MMSEngineDBFacade::addFTPConf(
    int64_t workspaceKey,
    string label,
    string server, int port, string userName, string password, string remoteDirectory)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_FTP(workspaceKey, label, server, port, userName, password, remoteDirectory) values ("
				"{}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(server),
				port, trans.quote(userName), trans.quote(password), trans.quote(remoteDirectory));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyFTPConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string server, int port, string userName, string password, string remoteDirectory)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_FTP set label = {}, server = {}, port = {}, "
				"userName = {}, password = {}, remoteDirectory = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(server), port,
				trans.quote(userName), trans.quote(password), trans.quote(remoteDirectory), confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeFTPConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_FTP where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getFTPConfList (
        int64_t workspaceKey
)
{
    Json::Value ftpConfListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getFTPConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
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
        
        string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_FTP {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value ftpRoot(Json::arrayValue);
        {                    
			string sqlStatement = fmt::format(
                "select confKey, label, server, port, userName, password, remoteDirectory from MMS_Conf_FTP {}", 
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value ftpConfRoot;

                field = "confKey";
				ftpConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				ftpConfRoot[field] = row["label"].as<string>();

                field = "server";
				ftpConfRoot[field] = row["server"].as<string>();

                field = "port";
				ftpConfRoot[field] = row["port"].as<int>();

                field = "userName";
				ftpConfRoot[field] = row["userName"].as<string>();

                field = "password";
				ftpConfRoot[field] = row["password"].as<string>();

                field = "remoteDirectory";
				ftpConfRoot[field] = row["remoteDirectory"].as<string>();

                ftpRoot.append(ftpConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "ftpConf";
        responseRoot[field] = ftpRoot;

        field = "response";
        ftpConfListRoot[field] = responseRoot;

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
    
    return ftpConfListRoot;
}

tuple<string, int, string, string, string> MMSEngineDBFacade::getFTPByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
	tuple<string, int, string, string, string> ftp;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getFTPByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        {
			string sqlStatement = fmt::format(
                "select server, port, userName, password, remoteDirectory from MMS_Conf_FTP "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_FTP failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

			string server = res[0]["server"].as<string>();
			int port = res[0]["port"].as<int>();
			string userName = res[0]["userName"].as<string>();
			string password = res[0]["password"].as<string>();
			string remoteDirectory = res[0]["remoteDirectory"].as<string>();

			ftp = make_tuple(server, port, userName, password, remoteDirectory);
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
    
    return ftp;
}

int64_t MMSEngineDBFacade::addEMailConf(
    int64_t workspaceKey,
    string label,
    string addresses, string subject, string message)
{
    int64_t     confKey;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		{
			string sqlStatement = fmt::format(
				"insert into MMS_Conf_EMail(workspaceKey, label, addresses, subject, message) values ("
				"{}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(addresses),
				trans.quote(subject), trans.quote(message));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
    
    return confKey;
}

void MMSEngineDBFacade::modifyEMailConf(
    int64_t confKey,
    int64_t workspaceKey,
    string label,
    string addresses, string subject, string message)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_Conf_EMail set label = {}, addresses = {}, subject = {}, "
				"message = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(addresses), trans.quote(subject),
				trans.quote(message),
				confKey, workspaceKey);
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
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
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

void MMSEngineDBFacade::removeEMailConf(
    int64_t workspaceKey,
    int64_t confKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Conf_EMail where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
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

Json::Value MMSEngineDBFacade::getEMailConfList (
        int64_t workspaceKey
)
{
    Json::Value emailConfListRoot;
    
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
        
        _logger->info(__FILEREF__ + "getEMailConfList"
            + ", workspaceKey: " + to_string(workspaceKey)
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
        
        string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
        
        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
                "select count(*) from MMS_Conf_EMail {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        Json::Value emailRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
                "select confKey, label, addresses, subject, message from MMS_Conf_EMail {}",
                sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value emailConfRoot;

                field = "confKey";
				emailConfRoot[field] = row["confKey"].as<int64_t>();

                field = "label";
				emailConfRoot[field] = row["label"].as<string>();

                field = "addresses";
				emailConfRoot[field] = row["addresses"].as<string>();

                field = "subject";
				emailConfRoot[field] = row["subject"].as<string>();

                field = "message";
				emailConfRoot[field] = row["message"].as<string>();

                emailRoot.append(emailConfRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "emailConf";
        responseRoot[field] = emailRoot;

        field = "response";
        emailConfListRoot[field] = responseRoot;

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
    
    return emailConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getEMailByConfigurationLabel(
    int64_t workspaceKey, string label
)
{
	tuple<string, string, string> email;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {        
        _logger->info(__FILEREF__ + "getEMailByConfigurationLabel"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        {
			string sqlStatement = fmt::format(
                "select addresses, subject, message from MMS_Conf_EMail "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
                string errorMessage = __FILEREF__ + "select from MMS_Conf_EMail failed"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", label: " + label
                ;

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

			string addresses = res[0]["addresses"].as<string>();
			string subject = res[0]["subject"].as<string>();
			string message = res[0]["message"].as<string>();

			email = make_tuple(addresses, subject, message);
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
    
    return email;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getStreamInputRoot(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	string configurationLabel,
	string useVideoTrackFromPhysicalPathName, string useVideoTrackFromPhysicalDeliveryURL,
	int maxWidth, string userAgent, string otherInputOptions,
	string taskEncodersPoolLabel, Json::Value drawTextDetailsRoot
)
{
	Json::Value streamInputRoot;

    try
    {
		int64_t confKey = -1;
		string streamSourceType;
		string encodersPoolLabel;
		string pullUrl;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		string pushServerName;	// indica il nome del server (public or internal)
		int pushServerPort = -1;
		string pushUri;
		int pushListenTimeout = -1;
		int captureVideoDeviceNumber = -1;
		string captureVideoInputFormat;
		int captureFrameRate = -1;
		int captureWidth = -1;
		int captureHeight = -1;
		int captureAudioDeviceNumber = -1;
		int captureChannelsNumber = -1;
		int64_t tvSourceTVConfKey = -1;

		{
			bool warningIfMissing = false;
			tuple<int64_t, string, string, string, string, int64_t, string, int, string, int,
				int, string, int, int, int, int, int, int64_t>
				channelConfDetails = getStreamDetails(
				workspace->_workspaceKey, configurationLabel, warningIfMissing);
			tie(confKey, streamSourceType,
				encodersPoolLabel,
				pullUrl,
				pushProtocol, pushEncoderKey, pushServerName, pushServerPort, pushUri,
				pushListenTimeout,
				captureVideoDeviceNumber,
				captureVideoInputFormat,
				captureFrameRate, captureWidth, captureHeight,
				captureAudioDeviceNumber, captureChannelsNumber,
				tvSourceTVConfKey) = channelConfDetails;

			// default is IP_PULL
			if (streamSourceType == "")
				streamSourceType = "IP_PULL";
		}

		string tvType;
		int64_t tvServiceId = -1;
		int64_t tvFrequency = -1;
		int64_t tvSymbolRate = -1;
		int64_t tvBandwidthInHz = -1;
		string tvModulation;
		int tvVideoPid = -1;
		int tvAudioItalianPid = -1;
		string liveURL;

		if (streamSourceType == "IP_PULL")
		{
			liveURL = pullUrl;

			string youTubePrefix1 ("https://www.youtube.com/");
			string youTubePrefix2 ("https://youtu.be/");
			if (
				(liveURL.size() >= youTubePrefix1.size()
					&& 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1))
				||
				(liveURL.size() >= youTubePrefix2.size()
					&& 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2))
				)
			{
				liveURL = getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, liveURL);
			}
		}
		else if (streamSourceType == "IP_PUSH")
		{
			liveURL = pushProtocol + "://" + pushServerName
				+ ":" + to_string(pushServerPort) + pushUri;
		}
		else if (streamSourceType == "TV")
		{
			bool warningIfMissing = false;
			tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
				tvChannelConfDetails = getSourceTVStreamDetails(
				tvSourceTVConfKey, warningIfMissing);

			tie(tvType, tvServiceId, tvFrequency,
				tvSymbolRate, tvBandwidthInHz, tvModulation,
				tvVideoPid, tvAudioItalianPid) = tvChannelConfDetails;
		}

		string field = "confKey";
		streamInputRoot[field] = confKey;

		field = "configurationLabel";
		streamInputRoot[field] = configurationLabel;

		field = "useVideoTrackFromPhysicalPathName";
		streamInputRoot[field] = useVideoTrackFromPhysicalPathName;

		field = "useVideoTrackFromPhysicalDeliveryURL";
		streamInputRoot[field] = useVideoTrackFromPhysicalDeliveryURL;

		field = "streamSourceType";
		streamInputRoot[field] = streamSourceType;

		field = "pushEncoderKey";
		streamInputRoot[field] = pushEncoderKey;

		field = "pushServerName";
		streamInputRoot[field] = pushServerName;

		// The taskEncodersPoolLabel (parameter of the Task/IngestionJob) overrides the one included
		// in ChannelConf if present
		field = "encodersPoolLabel";
		if (taskEncodersPoolLabel != "")
			streamInputRoot[field] = taskEncodersPoolLabel;
		else
			streamInputRoot[field] = encodersPoolLabel;

		field = "url";
		streamInputRoot[field] = liveURL;

		field = "drawTextDetails";
		streamInputRoot[field] = drawTextDetailsRoot;

		if (maxWidth != -1)
		{
			field = "maxWidth";
			streamInputRoot[field] = maxWidth;
		}

		if (userAgent != "")
		{
			field = "userAgent";
			streamInputRoot[field] = userAgent;
		}

		if (otherInputOptions != "")
		{
			field = "otherInputOptions";
			streamInputRoot[field] = otherInputOptions;
		}

		if (streamSourceType == "IP_PUSH")
		{
			field = "pushListenTimeout";
			streamInputRoot[field] = pushListenTimeout;
		}

		if (streamSourceType == "CaptureLive")
		{
			field = "captureVideoDeviceNumber";
			streamInputRoot[field] = captureVideoDeviceNumber;

			field = "captureVideoInputFormat";
			streamInputRoot[field] = captureVideoInputFormat;

			field = "captureFrameRate";
			streamInputRoot[field] = captureFrameRate;

			field = "captureWidth";
			streamInputRoot[field] = captureWidth;

			field = "captureHeight";
			streamInputRoot[field] = captureHeight;

			field = "captureAudioDeviceNumber";
			streamInputRoot[field] = captureAudioDeviceNumber;

			field = "captureChannelsNumber";
			streamInputRoot[field] = captureChannelsNumber;
		}

		if (streamSourceType == "TV")
		{
			field = "tvType";
			streamInputRoot[field] = tvType;

			field = "tvServiceId";
			streamInputRoot[field] = tvServiceId;

			field = "tvFrequency";
			streamInputRoot[field] = tvFrequency;

			field = "tvSymbolRate";
			streamInputRoot[field] = tvSymbolRate;

			field = "tvBandwidthInHz";
			streamInputRoot[field] = tvBandwidthInHz;

			field = "tvModulation";
			streamInputRoot[field] = tvModulation;

			field = "tvVideoPid";
			streamInputRoot[field] = tvVideoPid;

			field = "tvAudioItalianPid";
			streamInputRoot[field] = tvAudioItalianPid;
		}
	}
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "getStreamInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "getStreamInputRoot failed"
        );
        
        throw e;
    }

	return streamInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getVodInputRoot(
	MMSEngineDBFacade::ContentType vodContentType,
	vector<tuple<int64_t, string, string, string>>& sources,
	Json::Value drawTextDetailsRoot
)
{
	Json::Value vodInputRoot;

    try
    {
		string field = "vodContentType";
		vodInputRoot[field] = MMSEngineDBFacade::toString(vodContentType);

		Json::Value sourcesRoot(Json::arrayValue);

		for (tuple<int64_t, string, string, string> source: sources)
		{
			int64_t physicalPathKey;
			string mediaItemTitle;
			string sourcePhysicalPathName;
			string sourcePhysicalDeliveryURL;

			tie(physicalPathKey, mediaItemTitle, sourcePhysicalPathName,
				sourcePhysicalDeliveryURL) = source;


			Json::Value sourceRoot;

			field = "mediaItemTitle";
			sourceRoot[field] = mediaItemTitle;

			field = "sourcePhysicalPathName";
			sourceRoot[field] = sourcePhysicalPathName;

			field = "physicalPathKey";
			sourceRoot[field] = physicalPathKey;

			field = "sourcePhysicalDeliveryURL";
			sourceRoot[field] = sourcePhysicalDeliveryURL;

			sourcesRoot.append(sourceRoot);
		}

		field = "sources";
		vodInputRoot[field] = sourcesRoot;

		field = "drawTextDetails";
		vodInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
        );
        
        throw e;
    }

	return vodInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getCountdownInputRoot(
	string mmsSourceVideoAssetPathName,
	string mmsSourceVideoAssetDeliveryURL,
	int64_t physicalPathKey,
	int64_t videoDurationInMilliSeconds,
	Json::Value drawTextDetailsRoot
)
{
	Json::Value countdownInputRoot;

    try
    {
		string field = "mmsSourceVideoAssetPathName";
		countdownInputRoot[field] = mmsSourceVideoAssetPathName;

		field = "mmsSourceVideoAssetDeliveryURL";
		countdownInputRoot[field] = mmsSourceVideoAssetDeliveryURL;

		field = "physicalPathKey";
		countdownInputRoot[field] = physicalPathKey;

		field = "videoDurationInMilliSeconds";
		countdownInputRoot[field] = videoDurationInMilliSeconds;

		field = "drawTextDetails";
		countdownInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "getCountdownInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "getVodInputRoot failed"
        );
        
        throw e;
    }

	return countdownInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
Json::Value MMSEngineDBFacade::getDirectURLInputRoot(
	string url, Json::Value drawTextDetailsRoot
)
{
	Json::Value directURLInputRoot;

    try
    {
		string field = "url";
		directURLInputRoot[field] = url;

		field = "drawTextDetails";
		directURLInputRoot[field] = drawTextDetailsRoot;
	}
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "getDirectURLInputRoot failed"
            + ", e.what(): " + e.what()
        );
 
        throw e;
    }
    catch(exception& e)
    {
        _logger->error(__FILEREF__ + "getDirectURLInputRoot failed"
        );
        
        throw e;
    }

	return directURLInputRoot;
}

string MMSEngineDBFacade::getStreamingYouTubeLiveURL(
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t confKey,
	string liveURL
)
{

	string streamingYouTubeLiveURL;

	long hoursFromLastCalculatedURL;
	pair<long, string> lastYouTubeURLDetails;
	try
	{
		lastYouTubeURLDetails = getLastYouTubeURLDetails(workspace, ingestionJobKey, confKey);

		string lastCalculatedURL;

		tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

		long retrieveStreamingYouTubeURLPeriodInHours = 5;	// 5 hours

		_logger->info(__FILEREF__
			+ "check youTubeURLCalculate"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
			+ ", retrieveStreamingYouTubeURLPeriodInHours: " + to_string(retrieveStreamingYouTubeURLPeriodInHours)
		);
		if (hoursFromLastCalculatedURL < retrieveStreamingYouTubeURLPeriodInHours)
			streamingYouTubeLiveURL = lastCalculatedURL;
	}
	catch(runtime_error& e)
	{
		string errorMessage = __FILEREF__
			+ "youTubeURLCalculate. getLastYouTubeURLDetails failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", YouTube URL: " + streamingYouTubeLiveURL
		;
		_logger->error(errorMessage);
	}

	if (streamingYouTubeLiveURL == "")
	{
		try
		{
			FFMpeg ffmpeg (_configuration, _logger);
			pair<string, string> streamingLiveURLDetails =
				ffmpeg.retrieveStreamingYouTubeURL(ingestionJobKey, liveURL);

			tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

			_logger->info(__FILEREF__ + "youTubeURLCalculate. Retrieve streaming YouTube URL"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", confKey: " + to_string(confKey)
				+ ", initial YouTube URL: " + liveURL
				+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
				+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
			);
		}
		catch(runtime_error& e)
		{
			// in case ffmpeg.retrieveStreamingYouTubeURL fails
			// we will use the last saved URL
			tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

			string errorMessage = __FILEREF__
				+ "youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", confKey: " + to_string(confKey)
				+ ", YouTube URL: " + streamingYouTubeLiveURL
			;
			_logger->error(errorMessage);

			try
			{
				string firstLineOfErrorMessage;
				{
					string firstLine;
					stringstream ss(errorMessage);
					if (getline(ss, firstLine))
						firstLineOfErrorMessage = firstLine;
					else
						firstLineOfErrorMessage = errorMessage;
				}

				appendIngestionJobErrorMessage(ingestionJobKey, firstLineOfErrorMessage);
			}
			catch(runtime_error& e)
			{
				_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception& e)
			{
				_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				);
			}

			if (streamingYouTubeLiveURL == "")
			{
				// 2020-04-21: let's go ahead because it would be managed
				// the killing of the encodingJob
				// 2020-09-17: it does not have sense to continue
				//	if we do not have the right URL (m3u8)
				throw YouTubeURLNotRetrieved();
			}
		}

		if (streamingYouTubeLiveURL != "")
		{
			try
			{
				updateChannelDataWithNewYouTubeURL(workspace, ingestionJobKey,
					confKey, streamingYouTubeLiveURL);
			}
			catch(runtime_error& e)
			{
				string errorMessage = __FILEREF__
					+ "youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", confKey: " + to_string(confKey)
					+ ", YouTube URL: " + streamingYouTubeLiveURL
				;
				_logger->error(errorMessage);
			}
		}
	}
	else
	{
		_logger->info(__FILEREF__ + "youTubeURLCalculate. Reuse a previous streaming YouTube URL"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", confKey: " + to_string(confKey)
			+ ", initial YouTube URL: " + liveURL
			+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
			+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
		);
	}

	return streamingYouTubeLiveURL;
}

pair<long,string> MMSEngineDBFacade::getLastYouTubeURLDetails(
	shared_ptr<Workspace> workspace, int64_t ingestionKey, int64_t confKey
)
{
	long hoursFromLastCalculatedURL = -1;
	string lastCalculatedURL;

	try
	{
		tuple<string, string, string> channelDetails = getStreamDetails(
			workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot = JSONUtils::toJson(ingestionKey, -1, channelData);


		string field;

		Json::Value mmsDataRoot;
		{
			field = "mmsData";
			if (!JSONUtils::isMetadataPresent(channelDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no mmsData present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", confKey: " + to_string(confKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			mmsDataRoot = channelDataRoot[field];
		}

		Json::Value youTubeURLsRoot(Json::arrayValue);
		{
			field = "youTubeURLs";
			if (!JSONUtils::isMetadataPresent(mmsDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no youTubeURLs present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
					+ ", confKey: " + to_string(confKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			youTubeURLsRoot = mmsDataRoot[field];
		}

		if (youTubeURLsRoot.size() == 0)
		{
			_logger->info(__FILEREF__ + "no youTubeURL present"                
				+ ", ingestionKey: " + to_string(ingestionKey)
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", confKey: " + to_string(confKey)
			);

			return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
		}

		{
			Json::Value youTubeLiveURLRoot = youTubeURLsRoot[youTubeURLsRoot.size() - 1];

			time_t tNow;
			{
				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmNow;

				localtime_r (&utcNow, &tmNow);
				tNow = mktime(&tmNow);
			}

			time_t tLastCalculatedURLTime;
			{
				unsigned long       ulYear;
				unsigned long		ulMonth;
				unsigned long		ulDay;
				unsigned long		ulHour;
				unsigned long		ulMinutes;
				unsigned long		ulSeconds;
				int					sscanfReturn;

				field = "timestamp";
				string timestamp = JSONUtils::asString(youTubeLiveURLRoot, field, "");

				if ((sscanfReturn = sscanf (timestamp.c_str(),
					"%4lu-%2lu-%2lu %2lu:%2lu:%2lu",
					&ulYear,
					&ulMonth,
					&ulDay,
					&ulHour,
					&ulMinutes,
					&ulSeconds)) != 6)
				{
					string errorMessage = __FILEREF__ + "timestamp has a wrong format (sscanf failed)"                
						+ ", ingestionKey: " + to_string(ingestionKey)
						+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
						+ ", confKey: " + to_string(confKey)
						+ ", sscanfReturn: " + to_string(sscanfReturn)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmLastCalculatedURL;

				localtime_r (&utcNow, &tmLastCalculatedURL);

				tmLastCalculatedURL.tm_year	= ulYear - 1900;
				tmLastCalculatedURL.tm_mon	= ulMonth - 1;
				tmLastCalculatedURL.tm_mday	= ulDay;
				tmLastCalculatedURL.tm_hour	= ulHour;
				tmLastCalculatedURL.tm_min	= ulMinutes;
				tmLastCalculatedURL.tm_sec	= ulSeconds;

				tLastCalculatedURLTime = mktime(&tmLastCalculatedURL);
			}

			hoursFromLastCalculatedURL = (tNow - tLastCalculatedURLTime) / 3600;

			field = "youTubeURL";
			lastCalculatedURL = JSONUtils::asString(youTubeLiveURLRoot, field, "");
		}

		return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
	}
	catch(...)
	{
		string errorMessage = string("getLastYouTubeURLDetails failed")
			+ ", ingestionKey: " + to_string(ingestionKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", confKey: " + to_string(confKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineDBFacade::updateChannelDataWithNewYouTubeURL(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	int64_t confKey, string streamingYouTubeLiveURL
)
{
	try
	{
		tuple<string, string, string> channelDetails = getStreamDetails(
			workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot = JSONUtils::toJson(ingestionJobKey, -1, channelData);

		// add streamingYouTubeLiveURL info to the channelData
		{
			string field;

			Json::Value youTubeLiveURLRoot;
			{
				char strNow[64];
				{
					time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

					tm tmNow;

					localtime_r (&utcNow, &tmNow);
					sprintf (strNow, "%04d-%02d-%02d %02d:%02d:%02d",
						tmNow. tm_year + 1900,
						tmNow. tm_mon + 1,
						tmNow. tm_mday,
						tmNow. tm_hour,
						tmNow. tm_min,
						tmNow. tm_sec);
				}
				field = "timestamp";
				youTubeLiveURLRoot[field] = strNow;

				field = "youTubeURL";
				youTubeLiveURLRoot[field] = streamingYouTubeLiveURL;
			}

			Json::Value mmsDataRoot;
			{
				field = "mmsData";
				if (JSONUtils::isMetadataPresent(channelDataRoot, field))
					mmsDataRoot = channelDataRoot[field];
			}

			Json::Value previousYouTubeURLsRoot(Json::arrayValue);
			{
				field = "youTubeURLs";
				if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
					previousYouTubeURLsRoot = mmsDataRoot[field];
			}

			Json::Value youTubeURLsRoot(Json::arrayValue);

			// maintain the last 10 URLs
			int youTubeURLIndex;
			if (previousYouTubeURLsRoot.size() > 10)
				youTubeURLIndex = 10;
			else
				youTubeURLIndex = previousYouTubeURLsRoot.size();
			for (; youTubeURLIndex >= 0; youTubeURLIndex--)
				youTubeURLsRoot.append(previousYouTubeURLsRoot[youTubeURLIndex]);
			youTubeURLsRoot.append(youTubeLiveURLRoot);

			field = "youTubeURLs";
			mmsDataRoot[field] = youTubeURLsRoot;

			field = "mmsData";
			channelDataRoot[field] = mmsDataRoot;
		}

		bool labelToBeModified = false;
		string label;
		bool sourceTypeToBeModified = false;
		string sourceType;
		bool encodersPoolToBeModified = false;
		int64_t encodersPoolKey;
		bool urlToBeModified = false;
		string url;
		bool pushProtocolToBeModified = false;
		string pushProtocol;
		bool pushEncoderKeyToBeModified = false;
		int64_t pushEncoderKey = -1;
		bool pushServerNameToBeModified = false;
		string pushServerName;
		bool pushServerPortToBeModified = false;
		int pushServerPort = -1;
		bool pushUriToBeModified = false;
		string pushUri;
		bool pushListenTimeoutToBeModified = false;
		int pushListenTimeout = -1;
		bool captureVideoDeviceNumberToBeModified = false;
		int captureVideoDeviceNumber = -1;
		bool captureVideoInputFormatToBeModified = false;
		string captureVideoInputFormat;
		bool captureFrameRateToBeModified = false;
		int captureFrameRate = -1;
		bool captureWidthToBeModified = false;
		int captureWidth = -1;
		bool captureHeightToBeModified = false;
		int captureHeight = -1;
		bool captureAudioDeviceNumberToBeModified = false;
		int captureAudioDeviceNumber = -1;
		bool captureChannelsNumberToBeModified = false;
		int captureChannelsNumber = -1;
		bool tvSourceTVConfKeyToBeModified = false;
		int64_t tvSourceTVConfKey = -1;
		bool typeToBeModified = false;
		string type;
		bool descriptionToBeModified = false;
		string description;
		bool nameToBeModified = false;
		string name;
		bool regionToBeModified = false;
		string region;
		bool countryToBeModified = false;
		string country;
		bool imageToBeModified = false;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		bool positionToBeModified = false;
		int position = -1;
		bool channelDataToBeModified = true;

		modifyStream(
			confKey, "",
			workspace->_workspaceKey,
			labelToBeModified, label,
			sourceTypeToBeModified, sourceType,
			encodersPoolToBeModified, encodersPoolKey,
			urlToBeModified, url,
			pushProtocolToBeModified, pushProtocol,
			pushEncoderKeyToBeModified, pushEncoderKey,
			pushServerNameToBeModified, pushServerName,
			pushServerPortToBeModified, pushServerPort,
			pushUriToBeModified, pushUri,
			pushListenTimeoutToBeModified, pushListenTimeout,
			captureVideoDeviceNumberToBeModified, captureVideoDeviceNumber,
			captureVideoInputFormatToBeModified, captureVideoInputFormat,
			captureFrameRateToBeModified, captureFrameRate,
			captureWidthToBeModified, captureWidth,
			captureHeightToBeModified, captureHeight,
			captureAudioDeviceNumberToBeModified, captureAudioDeviceNumber,
			captureChannelsNumberToBeModified, captureChannelsNumber,
			tvSourceTVConfKeyToBeModified, tvSourceTVConfKey,
			typeToBeModified, type,
			descriptionToBeModified, description,
			nameToBeModified, name,
			regionToBeModified, region,
			countryToBeModified, country,
			imageToBeModified, imageMediaItemKey, imageUniqueName,
			positionToBeModified, position,
			channelDataToBeModified, channelDataRoot);
	}
	catch(...)
	{
		string errorMessage = string("updateChannelDataWithNewYouTubeURL failed")
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
			+ ", confKey: " + to_string(confKey)
			+ ", streamingYouTubeLiveURL: " + streamingYouTubeLiveURL
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

