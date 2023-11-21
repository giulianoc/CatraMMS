
#include "JSONUtils.h"
#include <algorithm>
#include "MMSEngineDBFacade.h"
#include "MMSCURL.h"
#include "catralibraries/Convert.h"


int64_t MMSEngineDBFacade::addEncoder(
    string label,
	bool external,
	bool enabled,
    string protocol,
	string publicServerName,
	string internalServerName,
	int port
	)
{
    int64_t		encoderKey;
    
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
                "insert into MMS_Encoder(label, external, enabled, protocol, "
				"publicServerName, internalServerName, port "
				") values ("
                "{}, {}, {}, {}, {}, {}, {}) returning encoderKey",
				trans.quote(label), external, enabled,
				trans.quote(protocol), trans.quote(publicServerName),
				trans.quote(internalServerName), port);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encoderKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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
    
    return encoderKey;
}

void MMSEngineDBFacade::modifyEncoder(
    int64_t encoderKey,
    bool labelToBeModified, string label,
    bool externalToBeModified, bool external,
    bool enabledToBeModified, bool enabled,
    bool protocolToBeModified, string protocol,
	bool publicServerNameToBeModified, string publicServerName,
	bool internalServerNameToBeModified, string internalServerName,
	bool portToBeModified, int port
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

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.quote(label));
				oneParameterPresent = true;
			}

			if (externalToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += (external ? "external = true" : "external = false");
				oneParameterPresent = true;
			}

			if (enabledToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += (enabled ? "enabled = true" : "enabled = false");
				oneParameterPresent = true;
			}

			if (protocolToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("protocol = " + trans.quote(protocol));
				oneParameterPresent = true;
			}

			if (publicServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("publicServerName = " + trans.quote(publicServerName));
				oneParameterPresent = true;
			}

			if (internalServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("internalServerName = " + trans.quote(internalServerName));
				oneParameterPresent = true;
			}

			if (portToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("port = " + to_string(port));
				oneParameterPresent = true;
			}

			if (!oneParameterPresent)
            {
                string errorMessage = __FILEREF__ + "Wrong input, no parameters to be updated"
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", oneParameterPresent: " + to_string(oneParameterPresent)
                ;
                SPDLOG_ERROR(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_Encoder {} "
				"where encoderKey = ? returning 1) select count(*) from rows",
				setSQL, encoderKey);
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
                        + ", lastSQLCommand: " + lastSQLCommand
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

void MMSEngineDBFacade::removeEncoder(
    int64_t encoderKey)
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
                "WITH rows AS (delete from MMS_Encoder where encoderKey = {} "
				"returning 1) select count(*) from rows",
				encoderKey);
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
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                SPDLOG_WARN(errorMessage);

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

tuple<string, string, string> MMSEngineDBFacade::getEncoderDetails (
	int64_t encoderKey
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
        string field;

        SPDLOG_INFO("getEncoderDetails"
            ", encoderKey: {}", encoderKey
        );

		string label;
		string publicServerName;
		string internalServerName;

		{
			string sqlStatement = fmt::format(
				"select label, publicServerName, internalServerName "
				"from MMS_Encoder where encoderKey = {}",
				encoderKey);
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
				string errorMessage = string("Encoder was not found")
					+ ", encoderKey: " + to_string(encoderKey)
				;
				SPDLOG_ERROR(errorMessage);

				throw EncoderNotFound(errorMessage);
			}

			label = res[0]["label"].as<string>();
			publicServerName = res[0]["publicServerName"].as<string>();
			internalServerName = res[0]["internalServerName"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(label, publicServerName, internalServerName);
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
    catch(EncoderNotFound& e)
	{
		SPDLOG_ERROR("Encoder Not Found"
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

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey)
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
		addAssociationWorkspaceEncoder(workspaceKey, encoderKey, conn, trans);

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

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey,
	shared_ptr<PostgresConnection> conn, nontransaction& trans)
{
	SPDLOG_INFO("Received addAssociationWorkspaceEncoder"
		", workspaceKey: {}"
		", encoderKey: {}", workspaceKey, encoderKey
	);

    try
    {
        {
			string sqlStatement = fmt::format(
                "insert into MMS_EncoderWorkspaceMapping (workspaceKey, encoderKey) "
				"values ({}, {})", workspaceKey, encoderKey);
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
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey,
	string sharedEncodersPoolLabel, Json::Value sharedEncodersLabel)
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
		vector<int64_t> encoderKeys;
		for (int encoderIndex = 0; encoderIndex < sharedEncodersLabel.size();
			encoderIndex++)
        {
			string encoderLabel = JSONUtils::asString(sharedEncodersLabel[encoderIndex]);

			string sqlStatement = fmt::format(
				"select encoderKey from MMS_Encoder "
				"where label = {}", trans.quote(encoderLabel));
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
				encoderKeys.push_back(res[0]["encoderKey"].as<int64_t>());
			else
			{
				string errorMessage = string("No encoder label found")
					+ ", encoderLabel: " + encoderLabel
				;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
        }

		for (int64_t encoderKey: encoderKeys)
			addAssociationWorkspaceEncoder(workspaceKey, encoderKey, conn, trans);

		addEncodersPool(workspaceKey, sharedEncodersPoolLabel, encoderKeys);

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

void MMSEngineDBFacade::removeAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey)
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
		// se l'encoder che vogliamo rimuovere da un workspace è all'interno di qualche EncodersPool,
		// bisogna rimuoverlo
        {
            string sqlStatement = fmt::format( 
                "WITH rows AS (delete from MMS_EncoderEncodersPoolMapping "
				"where encodersPoolKey in (select encodersPoolKey from MMS_EncodersPool where workspaceKey = {}) "
				"and encoderKey = {} returning 1) select count(*) from rows",
				workspaceKey, encoderKey);
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

        {
            string sqlStatement = fmt::format( 
                "WITH rows AS (delete from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} returning 1) select count(*) from rows",
				workspaceKey, encoderKey);
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
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                SPDLOG_WARN(errorMessage);

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

Json::Value MMSEngineDBFacade::getEncoderWorkspacesAssociation(
    int64_t encoderKey)
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
		Json::Value encoderWorkspacesAssociatedRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format(
				"select w.workspaceKey, w.name "
				"from MMS_Workspace w, MMS_EncoderWorkspaceMapping ewm "
				"where w.workspaceKey = ewm.workspaceKey and ewm.encoderKey = {}",
				encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
				Json::Value encoderWorkspaceAssociatedRoot;
    
				string field = "workspaceKey";
				encoderWorkspaceAssociatedRoot[field] = row["workspaceKey"].as<int64_t>();

				field = "workspaceName";
				encoderWorkspaceAssociatedRoot[field] = row["name"].as<string>();

				encoderWorkspacesAssociatedRoot.append(encoderWorkspaceAssociatedRoot);
			}
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

		return encoderWorkspacesAssociatedRoot;
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


Json::Value MMSEngineDBFacade::getEncoderList (
	bool admin,
	int start, int rows,
	bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey,
	string label, string serverName, int port,
	string labelOrder	// "" or "asc" or "desc"
)
{
    Json::Value encoderListRoot;

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
        
        SPDLOG_INFO("getEncoderList"
            ", start: {}"
            ", rows: {}"
            ", allEncoders: {}"
            ", runningInfo: {}"
            ", workspaceKey: {}"
            ", encoderKey: {}"
            ", label: {}"
            ", serverName: {}"
            ", port: {}"
            ", labelOrder: {}",
			start, rows, allEncoders, runningInfo, workspaceKey, encoderKey,
			label, serverName, port, labelOrder
        );
        
        {
            Json::Value requestParametersRoot;

			/*
			{
				field = "allEncoders";
				requestParametersRoot[field] = allEncoders;
			}

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}
			*/

            if (encoderKey != -1)
			{
				field = "encoderKey";
				requestParametersRoot[field] = encoderKey;
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
            
            if (serverName != "")
			{
				field = "serverName";
				requestParametersRoot[field] = serverName;
			}

            if (port != -1)
			{
				field = "port";
				requestParametersRoot[field] = port;
			}

			{
				field = "runningInfo";
				requestParametersRoot[field] = runningInfo;
			}

            if (labelOrder != "")
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

            field = "requestParameters";
            encoderListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere;
        if (encoderKey != -1)
		{
			if (sqlWhere != "")
				sqlWhere += fmt::format("and e.encoderKey = {} ", encoderKey);
			else
				sqlWhere += fmt::format("e.encoderKey = {} ", encoderKey);
		}
        if (label != "")
		{
			if (sqlWhere != "")
				sqlWhere += fmt::format("and LOWER(e.label) like LOWER({}) ", trans.quote("%" + label + "%"));
			else
				sqlWhere += fmt::format("LOWER(e.label) like LOWER({}) ", trans.quote("%" + label + "%"));
		}
        if (serverName != "")
		{
			if (sqlWhere != "")
				sqlWhere += fmt::format("and (e.publicServerName like {} or e.internalServerName like {}) ",
					trans.quote("%" + serverName + "%"), trans.quote("%" + serverName + "%"));
			else
				sqlWhere += fmt::format("(e.publicServerName like {} or e.internalServerName like {}) ",
					trans.quote(serverName), trans.quote(serverName));
		}
        if (port != -1)
		{
			if (sqlWhere != "")
				sqlWhere += fmt::format("and e.port = {} ", port);
			else
				sqlWhere += fmt::format("e.port = {} ", port);
		}

		if (allEncoders)
		{
			// using just MMS_Encoder
			if (sqlWhere != "")
				sqlWhere = fmt::format("where {}", sqlWhere);
		}
		else
		{
			// join with MMS_EncoderWorkspaceMapping
			if (sqlWhere != "")
				sqlWhere = fmt::format("where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = {} and ", workspaceKey) + sqlWhere;
			else
				sqlWhere = fmt::format("where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = {} ", workspaceKey);
		}

        Json::Value responseRoot;
        {
			string sqlStatement;
			if (allEncoders)
			{
				sqlStatement = fmt::format(
					"select count(*) from MMS_Encoder e {}",
					sqlWhere);
			}
			else
			{
				sqlStatement = fmt::format(
					"select count(*) "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm {}",
					sqlWhere);
			}
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

        Json::Value encodersRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

			string sqlStatement;
			if (allEncoders)
				sqlStatement = fmt::format( 
					"select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start);
			else
				sqlStatement = fmt::format(
					"select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
				Json::Value encoderRoot = getEncoderRoot(admin, runningInfo, row);

				encodersRoot.append(encoderRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "encoders";
        responseRoot[field] = encodersRoot;

        field = "response";
        encoderListRoot[field] = responseRoot;

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
    
    return encoderListRoot;
}

Json::Value MMSEngineDBFacade::getEncoderRoot (
	bool admin,
	bool runningInfo,
	row& row
)
{
    Json::Value encoderRoot;
    
    try
    {
		int64_t encoderKey = row["encoderKey"].as<int64_t>();

        string field = "encoderKey";
		encoderRoot[field] = encoderKey;

		field = "label";
		encoderRoot[field] = row["label"].as<string>();

		field = "external";
		bool external = row["external"].as<bool>();
		encoderRoot[field] = external;

		field = "enabled";
		encoderRoot[field] = row["enabled"].as<bool>();

		field = "protocol";
		string protocol = row["protocol"].as<string>();
		encoderRoot[field] = protocol;

		field = "publicServerName";
		string publicServerName = row["publicServerName"].as<string>();
		encoderRoot[field] = publicServerName;

		field = "internalServerName";
		string internalServerName = row["internalServerName"].as<string>();
		encoderRoot[field] = internalServerName;

		field = "port";
		int port = row["port"].as<int>();
		encoderRoot[field] = port;

		// 2022-1-30: running and cpu usage takes a bit of time
		//		scenario: some MMS WEB pages loading encoder info, takes a bit of time
		//		to be loaded because of this check and, in these pages, we do not care about
		//		running info, so we made it optional
		if (runningInfo)
		{
			bool running;
			int cpuUsage = 0;
			pair<bool, int> encoderRunningDetails = getEncoderInfo(external, protocol,
				publicServerName, internalServerName, port);
			tie(running, cpuUsage) = encoderRunningDetails;

			field = "running";
			encoderRoot[field] = running;

			field = "cpuUsage";
			encoderRoot[field] = cpuUsage;
		}

		if (admin)
		{
			field = "workspacesAssociated";
			encoderRoot[field] = getEncoderWorkspacesAssociation(encoderKey);
		}
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}",
			e.query(), e.what()
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
		);

		throw e;
	}
    
    return encoderRoot;
}

bool MMSEngineDBFacade::isEncoderRunning(
	bool external, string protocol,
	string publicServerName, string internalServerName,
	int port)
{
	bool isRunning = true;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://"
			+ (external ? publicServerName : internalServerName)
			+ ":" + to_string(port)
            + _ffmpegEncoderStatusURI
		;

		vector<string> otherHeaders;
		Json::Value infoResponseRoot = MMSCURL::httpGetJson(
			_logger,
			-1,	// ingestionJobKey
			ffmpegEncoderURL,
			_ffmpegEncoderInfoTimeout,
			_ffmpegEncoderUser,
			_ffmpegEncoderPassword,
			otherHeaders
		);
	}
	catch (ServerNotReachable e)
	{
		SPDLOG_ERROR("Encoder is not reachable, is it down?"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
    }
	catch (runtime_error e)
	{
		SPDLOG_ERROR("Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
    }
	catch (exception e)
	{
		SPDLOG_ERROR("Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}

	return isRunning;
}


pair<bool, int> MMSEngineDBFacade::getEncoderInfo(
	bool external, string protocol,
	string publicServerName, string internalServerName,
	int port)
{
	bool isRunning = true;
	int cpuUsage = 0;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://"
			+ (external ? publicServerName : internalServerName)
			+ ":" + to_string(port)
            + _ffmpegEncoderInfoURI
		;

		vector<string> otherHeaders;
		Json::Value infoResponseRoot = MMSCURL::httpGetJson(
			_logger,
			-1,	// ingestionJobKey
			ffmpegEncoderURL,
			_ffmpegEncoderInfoTimeout,
			_ffmpegEncoderUser,
			_ffmpegEncoderPassword,
			otherHeaders
		);

		string field = "cpuUsage";
		cpuUsage = JSONUtils::asInt(infoResponseRoot, field, 0);
	}
	catch (ServerNotReachable e)
	{
		SPDLOG_ERROR("Encoder is not reachable, is it down?"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
    }
	catch (runtime_error e)
	{
		SPDLOG_ERROR("Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
    }
	catch (exception e)
	{
		SPDLOG_ERROR("Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}", ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}

	return make_pair(isRunning, cpuUsage);
}


string MMSEngineDBFacade::getEncodersPoolDetails (int64_t encodersPoolKey)
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

        SPDLOG_INFO("getEncodersPoolDetails"
            ", encodersPoolKey: {}", encodersPoolKey
        );

		string label;

		{
			string sqlStatement = fmt::format(
				"select label from MMS_EncodersPool "
				"where encodersPoolKey = {}",
				encodersPoolKey);
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
				string errorMessage = __FILEREF__ + "EncodersPool was not found"
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			label = res[0]["label"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return label;
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

Json::Value MMSEngineDBFacade::getEncodersPoolList (
	int start, int rows,
	int64_t workspaceKey, int64_t encodersPoolKey, string label,
	string labelOrder	// "" or "asc" or "desc"
)
{
    Json::Value encodersPoolListRoot;

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
        
        SPDLOG_INFO("getEncodersPoolList"
            ", start: {}"
            ", rows: {}"
            ", workspaceKey: {}"
            ", encodersPoolKey: {}"
            ", label: {}"
            ", labelOrder: {}",
			start, rows, workspaceKey, encodersPoolKey, label, labelOrder
        );
        
        {
            Json::Value requestParametersRoot;

            if (encodersPoolKey != -1)
			{
				field = "encodersPoolKey";
				requestParametersRoot[field] = encodersPoolKey;
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
            
            if (labelOrder != "")
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

            field = "requestParameters";
            encodersPoolListRoot[field] = requestParametersRoot;
        }
        
		// label == NULL is the "internal" EncodersPool representing the default encoders pool
		// for a workspace, the one using all the internal encoders associated to the workspace
		string sqlWhere = fmt::format("where workspaceKey = {} and label is not NULL ", workspaceKey);
        if (encodersPoolKey != -1)
			sqlWhere += fmt::format("and encodersPoolKey = {} ", encodersPoolKey);
        if (label != "")
			sqlWhere += fmt::format("and LOWER(label) like LOWER({}) ", trans.quote("%" + label + "%"));


        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
				"select count(*) from MMS_EncodersPool {}", sqlWhere);
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

        Json::Value encodersPoolsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder != "")
				orderByCondition = "order by label " + labelOrder + " ";

			string sqlStatement = fmt::format( 
				"select encodersPoolKey, label from MMS_EncodersPool {} {} limit {} offset {}",
				sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
				Json::Value encodersPoolRoot;

                int64_t encodersPoolKey = row["encodersPoolKey"].as<int64_t>();

				string field = "encodersPoolKey";
				encodersPoolRoot[field] = encodersPoolKey;

				field = "label";
                encodersPoolRoot[field] = row["label"].as<string>();

				Json::Value encodersRoot(Json::arrayValue);
				{
					string sqlStatement = fmt::format( 
						"select encoderKey from MMS_EncoderEncodersPoolMapping " 
						"where encodersPoolKey = {}", encodersPoolKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row: res)
					{
						int64_t encoderKey = row["encoderKey"].as<int64_t>();

						{
							string sqlStatement = fmt::format( 
								"select encoderKey, label, external, enabled, protocol, "
								"publicServerName, internalServerName, port "
								"from MMS_Encoder "
								"where encoderKey = {} ", encoderKey);
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
								bool admin = false;
								bool runningInfo = false;
								Json::Value encoderRoot = getEncoderRoot (
									admin, runningInfo, row);

								encodersRoot.append(encoderRoot);
							}
							else
							{
								string errorMessage = string("No encoderKey found")
									+ ", encoderKey: " + to_string(encoderKey)
								;
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}
				}

				field = "encoders";
				encodersPoolRoot[field] = encodersRoot;

                encodersPoolsRoot.append(encodersPoolRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "encodersPool";
        responseRoot[field] = encodersPoolsRoot;

        field = "response";
        encodersPoolListRoot[field] = responseRoot;

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
    
    return encodersPoolListRoot;
}

int64_t MMSEngineDBFacade::addEncodersPool(
	int64_t workspaceKey,
    string label,
	vector<int64_t>& encoderKeys
	)
{
    int64_t		encodersPoolKey;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();	
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		// check: every encoderKey shall be already associated to the workspace
		for(int64_t encoderKey: encoderKeys)
        {
            string sqlStatement = fmt::format( 
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} ",
				workspaceKey, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (trans.exec1(sqlStatement)[0].as<int>() == 0)
			{
				string errorMessage = __FILEREF__
					+ "Encoder is not already associated to the workspace"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encoderKey: " + to_string(encoderKey)
				;
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

        {
			string sqlStatement = fmt::format(
                "insert into MMS_EncodersPool(workspaceKey, label, "
				"lastEncoderIndexUsed) values ( "
                "{}, {}, 0) returning encodersPoolKey",
				workspaceKey, trans.quote(label));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encodersPoolKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		for(int64_t encoderKey: encoderKeys)
        {
			string sqlStatement = fmt::format(
                "insert into MMS_EncoderEncodersPoolMapping(encodersPoolKey, "
				"encoderKey) values ( "
				"{}, {})", encodersPoolKey, encoderKey);
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
    
    return encodersPoolKey;
}

int64_t MMSEngineDBFacade::modifyEncodersPool(
	int64_t encodersPoolKey,
	int64_t workspaceKey,
    string newLabel,
	vector<int64_t>& newEncoderKeys
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
		SPDLOG_INFO("Received modifyEncodersPool"
			", encodersPoolKey: {}"
			", workspaceKey: {}"
			", newLabel: {}"
			", newEncoderKeys.size: {}",
			encodersPoolKey, workspaceKey, newLabel, newEncoderKeys.size()
		);

		// check: every encoderKey shall be already associated to the workspace
		for(int64_t encoderKey: newEncoderKeys)
        {
            string sqlStatement = fmt::format( 
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} ",
				workspaceKey, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (trans.exec1(sqlStatement)[0].as<int>() == 0)
			{
				string errorMessage = __FILEREF__
					+ "Encoder is not already associated to the workspace"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encoderKey: " + to_string(encoderKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

        {
			string sqlStatement = fmt::format( 
				"select label from MMS_EncodersPool "
				"where encodersPoolKey = {} ", encodersPoolKey);
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
				string savedLabel = res[0]["label"].as<string>();
				if (savedLabel != newLabel)
				{
					string sqlStatement = fmt::format( 
						"WITH rows AS (update MMS_EncodersPool "
						"set label = {} "
						"where encodersPoolKey = {} returning 1) select count(*) from rows",
						trans.quote(newLabel), encodersPoolKey);
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
							+ ", newLabel: " + newLabel
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                    
					}
				}

				vector<int64_t> savedEncoderKeys;
				{
					string sqlStatement = fmt::format( 
						"select encoderKey from MMS_EncoderEncodersPoolMapping "
						"where encodersPoolKey = {}", encodersPoolKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row: res)
					{
						int64_t encoderKey = row["encoderKey"].as<int64_t>();

						savedEncoderKeys.push_back(encoderKey);
					}
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
				}

				// all the new encoderKey that are not present in savedEncoderKeys have to be added
				for(int64_t newEncoderKey: newEncoderKeys)
				{
					if (find(savedEncoderKeys.begin(), savedEncoderKeys.end(),
						newEncoderKey) == savedEncoderKeys.end())
					{
						string sqlStatement = fmt::format(
							"insert into MMS_EncoderEncodersPoolMapping("
							"encodersPoolKey, encoderKey) values ( "
							"{}, {})", encodersPoolKey, newEncoderKey);
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

				// all the saved encoderKey that are not present in encoderKeys have to be removed
				for(int64_t savedEncoderKey: savedEncoderKeys)
				{
					if (find(newEncoderKeys.begin(), newEncoderKeys.end(),
						savedEncoderKey) == newEncoderKeys.end())
					{
						string sqlStatement = fmt::format( 
							"WITH rows AS (delete from MMS_EncoderEncodersPoolMapping "
							"where encodersPoolKey = {} and encoderKey = {} "
							"returning 1) select count(*) from rows",
							encodersPoolKey, savedEncoderKey);
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
								+ ", encodersPoolKey: " + to_string(encodersPoolKey)
								+ ", savedEncoderKey: " + to_string(savedEncoderKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", sqlStatement: " + sqlStatement
							;
							_logger->warn(errorMessage);

							throw runtime_error(errorMessage);                    
						}
					}
				}
            }
			else
			{
                string errorMessage = __FILEREF__ + "No encodersPool found"
                        + ", encodersPoolKey: " + to_string(encodersPoolKey)
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
    
    return encodersPoolKey;
}

void MMSEngineDBFacade::removeEncodersPool(
    int64_t encodersPoolKey)
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
				"WITH rows AS (delete from MMS_EncodersPool where encodersPoolKey = {} "
				"returning 1) select count(*) from rows",
				encodersPoolKey);
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
                        + ", encodersPoolKey: " + to_string(encodersPoolKey)
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

tuple<int64_t, bool, string, string, string, int>
	MMSEngineDBFacade::getRunningEncoderByEncodersPool(
	int64_t workspaceKey, string encodersPoolLabel,
	int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed)
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
        
        _logger->info(__FILEREF__ + "getRunningEncoderByEncodersPool"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodersPoolLabel: " + encodersPoolLabel
            + ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
            + ", externalEncoderAllowed: " + to_string(externalEncoderAllowed)
        );
        
		int lastEncoderIndexUsed;
		int64_t encodersPoolKey;
        {
			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = fmt::format( 
					"select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool "
					"where workspaceKey = ? "
					"and label is null for update", workspaceKey);
			else
				sqlStatement = fmt::format(
					"select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool "
					"where workspaceKey = {} "
					"and label = {} for update", workspaceKey, trans.quote(encodersPoolLabel));
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
				string errorMessage = __FILEREF__ + "encodersPool was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encodersPoolLabel: " + encodersPoolLabel
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			lastEncoderIndexUsed = res[0]["lastEncoderIndexUsed"].as<int>();
			encodersPoolKey = res[0]["encodersPoolKey"].as<int64_t>();
        }

		int encodersNumber;
		{
			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = fmt::format(
					"select count(*) from MMS_EncoderWorkspaceMapping "
					"where workspaceKey = {} ", workspaceKey);
			else
				sqlStatement = fmt::format(
					"select count(*) from MMS_EncoderEncodersPoolMapping "
					"where encodersPoolKey = {} ", encodersPoolKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            encodersNumber = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		int newLastEncoderIndexUsed = lastEncoderIndexUsed;

		int64_t encoderKey;
		bool external = false;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port;
		bool encoderFound = false;
		int encoderIndex = 0;

		while(!encoderFound && encoderIndex < encodersNumber)
		{
			encoderIndex++;

			newLastEncoderIndexUsed = (newLastEncoderIndexUsed + 1) % encodersNumber;

			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = fmt::format( 
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm " 
					"where e.encoderKey = ewm.encoderKey and ewm.workspaceKey = {} "
					"order by e.publicServerName "
					"limit 1 offset {}", workspaceKey, newLastEncoderIndexUsed);
			else
				sqlStatement = fmt::format( 
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderEncodersPoolMapping eepm " 
					"where e.encoderKey = eepm.encoderKey and eepm.encodersPoolKey = {} "
					"order by e.publicServerName "
					"limit 1 offset {}", encodersPoolKey, newLastEncoderIndexUsed);
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
				encoderKey = res[0]["encoderKey"].as<int64_t>();

				bool enabled = res[0]["enabled"].as<bool>();
				external = res[0]["external"].as<bool>();
				protocol = res[0]["protocol"].as<string>();
				publicServerName = res[0]["publicServerName"].as<string>();
				internalServerName = res[0]["internalServerName"].as<string>();
				port = res[0]["port"].as<int>();

				if (external && !externalEncoderAllowed)
				{
					SPDLOG_INFO(
						"getEncoderByEncodersPool, skipped encoderKey because external encoder are not allowed"
						", workspaceKey: {}"
						", encodersPoolLabel: {}"
						", enabled: {}",
						workspaceKey, encodersPoolLabel, enabled
					);

					continue;
				}
				else if (!enabled)
				{
					SPDLOG_INFO("getEncoderByEncodersPool, skipped encoderKey because encoder not enabled"
						", workspaceKey: {}"
						", encodersPoolLabel: {}"
						", enabled: {}",
						workspaceKey, encodersPoolLabel, enabled
					);

					continue;
				}
				else if (encoderKeyToBeSkipped != -1 && encoderKeyToBeSkipped == encoderKey)
				{
					_logger->info(__FILEREF__ + "getEncoderByEncodersPool, skipped encoderKey"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
						+ ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
					);

					continue;
				}
				else
				{
					if (!isEncoderRunning(external, protocol,
						publicServerName, internalServerName, port))
					{
						_logger->info(__FILEREF__
							+ "getEncoderByEncodersPool, dicarded encoderKey because not running"
							+ ", workspaceKey: " + to_string(workspaceKey)
							+ ", encodersPoolLabel: " + encodersPoolLabel
						);

						continue;
					}
				}

				encoderFound = true;
			}
			else
			{
				string errorMessage = __FILEREF__ + "Encoder details not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!encoderFound)
		{
			string errorMessage = __FILEREF__ + "Encoder was not found"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
			;
			_logger->error(errorMessage);

			throw EncoderNotFound(errorMessage);
		}

        {
			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_EncodersPool set lastEncoderIndexUsed = {} "
				"where encodersPoolKey = {} returning 1) select count(*) from rows",
				newLastEncoderIndexUsed, encodersPoolKey);
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
					+ ", newLastEncoderIndexUsed: " + to_string(newLastEncoderIndexUsed)
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

				// in case of one encoder, no update is done
				// because newLastEncoderIndexUsed is always the same

                // throw runtime_error(errorMessage);                    
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(encoderKey, external, protocol,
			publicServerName, internalServerName, port);
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
    catch(EncoderNotFound& e)
	{
		SPDLOG_ERROR("Encoder not found"
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

int MMSEngineDBFacade::getEncodersNumberByEncodersPool(
	int64_t workspaceKey, string encodersPoolLabel)
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
        
        _logger->info(__FILEREF__ + "getEncodersNumberByEncodersPool"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodersPoolLabel: " + encodersPoolLabel
        );
        
		int encodersNumber;
		if (encodersPoolLabel != "")
        {
			int64_t encodersPoolKey;
			{
				string sqlStatement = fmt::format( 
					"select encodersPoolKey from MMS_EncodersPool "
					"where workspaceKey = {} "
					"and label = {} ", workspaceKey, trans.quote(encodersPoolLabel));
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
					string errorMessage = string("lastEncoderIndexUsed was not found")
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersPoolKey = res[0]["encodersPoolKey"].as<int64_t>();
			}

			{
				string sqlStatement = fmt::format(
					"select count(*) from MMS_EncoderEncodersPoolMapping "
					"where encodersPoolKey = {} ", encodersPoolKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodersNumber = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
		}
		else
		{
			string sqlStatement = fmt::format(
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} ", workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            encodersNumber = trans.exec1(sqlStatement)[0].as<int>();
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

		return encodersNumber;
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

pair<string, bool> MMSEngineDBFacade::getEncoderURL(int64_t encoderKey,
	string serverName)
{
    Json::Value encodersPoolListRoot;

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

        _logger->info(__FILEREF__ + "getEncoderURL"
			+ ", encoderKey: " + to_string(encoderKey)
			+ ", serverName: " + serverName
        );

		bool external;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port;
		{
			string sqlStatement = fmt::format( 
				"select external, protocol, publicServerName, internalServerName, port "
				"from MMS_Encoder where encoderKey = {} ", encoderKey)
				// 2022-05-22. Scenario: tried to kill a job of a just disabled encoder 
				//	The kill did not work becuase this method did not return the URL
				//	because of the below sql condition.
				//	It was commented because it is not in this method that has to be
				//	checked the enabled flag, this method is specific to return a url
				//	of an encoder and ddoes not have to check the enabled flag
				// "and enabled = 1 "
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
			if (!empty(res))
			{
				external = res[0]["external"].as<bool>();
				protocol = res[0]["protocol"].as<string>();
				publicServerName = res[0]["publicServerName"].as<string>();
				internalServerName = res[0]["internalServerName"].as<string>();
				port = res[0]["port"].as<int>();
			}
			else
			{
				string errorMessage = string("Encoder details not found or not enabled")
					+ ", encoderKey: " + to_string(encoderKey)
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


		string encoderURL;
		if (serverName != "")
			encoderURL = protocol + "://" + serverName + ":" + to_string(port);
		else
		{
			if (external)
				encoderURL = protocol + "://" + publicServerName + ":" + to_string(port);
			else
				encoderURL = protocol + "://" + internalServerName + ":"
					+ to_string(port);
		}

        _logger->info(__FILEREF__ + "getEncoderURL"
			+ ", encoderKey: " + to_string(encoderKey)
			+ ", serverName: " + serverName
			+ ", encoderURL: " + encoderURL
        );

		return make_pair(encoderURL, external);
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

