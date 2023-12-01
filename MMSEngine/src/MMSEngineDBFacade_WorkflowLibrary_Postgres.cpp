
#include <random>
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	int64_t userKey,
	int64_t workspaceKey,
	string label,
	int64_t thumbnailMediaItemKey,
	string jsonWorkflow,
	bool admin
)
{
	int64_t		workflowLibraryKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		workflowLibraryKey = addUpdateWorkflowAsLibrary(
			conn,
			trans,
			userKey,
			workspaceKey,
			label,
			thumbnailMediaItemKey,
			jsonWorkflow,
			admin);

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
    
    return workflowLibraryKey;
}

int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	shared_ptr<PostgresConnection> conn,
	nontransaction& trans,
	int64_t userKey,
	int64_t workspaceKey,
	string label,
	int64_t thumbnailMediaItemKey,
	string jsonWorkflow,
	bool admin
)
{
	int64_t			workflowLibraryKey;

    try
    {
        {
			// in case workspaceKey == -1 (MMS library), only ADMIN can add/update it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = fmt::format( 
					"select workflowLibraryKey from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = {}",
					trans.quote(label));
			else
				sqlStatement = fmt::format( 
					"select workflowLibraryKey, creatorUserKey from MMS_WorkflowLibrary "
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
			if (!empty(res))
            {
				// two options:
				// 1. MMS library: it is an admin user (check already done, update can be done
				// 2. NO MMS Library: only creatorUserKey can do the update
				//		We should add a rights like 'Allow Update Workflow Library even if not the creator'
				//		but this is not present yet
                workflowLibraryKey     = res[0]["workflowLibraryKey"].as<int64_t>();
				if (workspaceKey != -1)
				{
					int64_t creatorUserKey     = res[0]["creatorUserKey"].as<int64_t>();
					if (creatorUserKey != userKey && !admin)
					{
						string errorMessage = string("User does not have the permission to add/update MMS WorkflowAsLibrary")
							+ ", creatorUserKey: " + to_string(creatorUserKey)
							+ ", userKey: " + to_string(userKey)
						;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

                string sqlStatement = fmt::format(
                    "WITH rows AS (update MMS_WorkflowLibrary set lastUpdateUserKey = {}, "
					"thumbnailMediaItemKey = {}, jsonWorkflow = {} "
					"where workflowLibraryKey = {} returning 1) select count(*) from rows",
					userKey == -1 ? "null" : to_string(userKey),
					thumbnailMediaItemKey == -1 ? "null" : to_string(thumbnailMediaItemKey),
					trans.quote(jsonWorkflow), workflowLibraryKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
            }
            else
            {
                string sqlStatement = fmt::format( 
					"insert into MMS_WorkflowLibrary ("
					"workspaceKey, creatorUserKey, lastUpdateUserKey, "
					"label, thumbnailMediaItemKey, jsonWorkflow) values ("
					"{}, {}, {}, {}, {}, {}) returning workflowLibraryKey",
					workspaceKey == -1 ? "null" : to_string(workspaceKey),
					workspaceKey == -1 ? "null" : to_string(userKey),
					userKey == -1 ? "null" : to_string(userKey),
					trans.quote(label),
					thumbnailMediaItemKey == -1 ? "null" : to_string(thumbnailMediaItemKey),
					trans.quote(jsonWorkflow));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				workflowLibraryKey = trans.exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
            }
        }
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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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
    
	return workflowLibraryKey;
}

void MMSEngineDBFacade::removeWorkflowAsLibrary(
    int64_t userKey, int64_t workspaceKey,
	int64_t workflowLibraryKey, bool admin)
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
			// in case workspaceKey == -1 (MMS library), only ADMIN can remove it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = fmt::format(
					"WITH rows AS (delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
					"and workspaceKey is null returning 1) select count(*) from rows",
					workflowLibraryKey);
			else
			{
				// admin is able to remove also WorkflowLibrarys created by others
				// for this reason the condition on creatorUserKey has to be removed
				if (admin)
					sqlStatement = fmt::format( 
						"WITH rows AS (delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
						"and workspaceKey = {} returning 1) select count(*) from rows",
						workflowLibraryKey, workspaceKey);
				else
					sqlStatement = fmt::format(
						"WITH rows AS (delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
						"and creatorUserKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
						workspaceKey, userKey, workspaceKey);
			}
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
                        + ", workflowLibraryKey: " + to_string(workflowLibraryKey)
                        + ", userKey: " + to_string(userKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
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

Json::Value MMSEngineDBFacade::getWorkflowsAsLibraryList (
	int64_t workspaceKey
)
{
    Json::Value workflowsLibraryRoot;
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
        
        _logger->info(__FILEREF__ + "getWorkflowsAsLibraryList"
            + ", workspaceKey: " + to_string(workspaceKey)
        );

        {
            Json::Value requestParametersRoot;
            
            field = "requestParameters";
            workflowsLibraryRoot[field] = requestParametersRoot;
        }

        string sqlWhere = fmt::format("where (workspaceKey = {} or workspaceKey is null) ", workspaceKey);

        Json::Value responseRoot;
        {
            string sqlStatement = fmt::format( 
                "select count(*) from MMS_WorkflowLibrary {}",
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

        Json::Value workflowsRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format( 
				"select workspaceKey, workflowLibraryKey, creatorUserKey, label, "
				"thumbnailMediaItemKey, jsonWorkflow from MMS_WorkflowLibrary {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value workflowLibraryRoot;

                field = "global";
				if (row["workspaceKey"].is_null())
					workflowLibraryRoot[field] = true;
				else
				{
					workflowLibraryRoot[field] = false;

					field = "creatorUserKey";
					workflowLibraryRoot[field] = row["creatorUserKey"].as<int64_t>();
				}

                field = "workflowLibraryKey";
                workflowLibraryRoot[field] = row["workflowLibraryKey"].as<int64_t>();

                field = "label";
                workflowLibraryRoot[field] = row["label"].as<string>();

				field = "thumbnailMediaItemKey";
				if (row["thumbnailMediaItemKey"].is_null())
					workflowLibraryRoot[field] = Json::nullValue;
				else
					workflowLibraryRoot[field] = row["thumbnailMediaItemKey"].as<int64_t>();

                {
                    string jsonWorkflow = row["jsonWorkflow"].as<string>();

                    Json::Value workflowRoot;
                    try
                    {
						workflowRoot = JSONUtils::toJson(-1, -1, jsonWorkflow);
                    }
                    catch(runtime_error& e)
                    {
                        _logger->error(__FILEREF__ + e.what());

                        continue;
                    }

                    field = "variables";
					if (!JSONUtils::isMetadataPresent(workflowRoot, field))
						workflowLibraryRoot["variables"] = Json::nullValue;
					else
						workflowLibraryRoot["variables"] = workflowRoot[field];
                }

                workflowsRoot.append(workflowLibraryRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "workflowsLibrary";
        responseRoot[field] = workflowsRoot;

        field = "response";
        workflowsLibraryRoot[field] = responseRoot;

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

    return workflowsLibraryRoot;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent (
	int64_t workspaceKey,
	int64_t workflowLibraryKey
)
{
    string		workflowLibraryContent;
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
        
        _logger->info(__FILEREF__ + "getWorkflowAsLibraryContent"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", workflowLibraryKey: " + to_string(workflowLibraryKey)
        );

        {                    
            string sqlStatement = fmt::format(
				"select jsonWorkflow from MMS_WorkflowLibrary "
				"where (workspaceKey = {} or workspaceKey is null) "
				"and workflowLibraryKey = {}",
				workspaceKey, workflowLibraryKey);
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
				string errorMessage = __FILEREF__ + "WorkflowLibrary was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", workflowLibraryKey: " + to_string(workflowLibraryKey)
				;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			workflowLibraryContent = res[0]["jsonWorkflow"].as<string>();
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

    return workflowLibraryContent;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent (
	int64_t workspaceKey,
	string label
)
{
    string		workflowLibraryContent;
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
        
        _logger->info(__FILEREF__ + "getWorkflowAsLibraryContent"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", label: " + label
        );

        {
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = fmt::format(
					"select jsonWorkflow from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = {}",
					trans.quote(label));
			else
				sqlStatement = fmt::format(
					"select jsonWorkflow from MMS_WorkflowLibrary "
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
				string errorMessage = __FILEREF__ + "WorkflowLibrary was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", label: " + label
				;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			workflowLibraryContent = res[0]["jsonWorkflow"].as<string>();
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

    return workflowLibraryContent;
}

