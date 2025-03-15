
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "catralibraries/Convert.h"
#include "catralibraries/PostgresConnection.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <algorithm>

int64_t MMSEngineDBFacade::addEncoder(
	string label, bool external, bool enabled, string protocol, string publicServerName, string internalServerName, int port
)
{
	int64_t encoderKey;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Encoder(label, external, enabled, protocol, "
				"publicServerName, internalServerName, port "
				") values ("
				"{}, {}, {}, {}, {}, {}, {}) returning encoderKey",
				trans.transaction->quote(label), external, enabled, trans.transaction->quote(protocol), trans.transaction->quote(publicServerName),
				trans.transaction->quote(internalServerName), port
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encoderKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return encoderKey;
}

void MMSEngineDBFacade::modifyEncoder(
	int64_t encoderKey, bool labelToBeModified, string label, bool externalToBeModified, bool external, bool enabledToBeModified, bool enabled,
	bool protocolToBeModified, string protocol, bool publicServerNameToBeModified, string publicServerName, bool internalServerNameToBeModified,
	string internalServerName, bool portToBeModified, int port
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.transaction->quote(label));
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
				setSQL += ("protocol = " + trans.transaction->quote(protocol));
				oneParameterPresent = true;
			}

			if (publicServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("publicServerName = " + trans.transaction->quote(publicServerName));
				oneParameterPresent = true;
			}

			if (internalServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("internalServerName = " + trans.transaction->quote(internalServerName));
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
				string errorMessage = std::format(
					"Wrong input, no parameters to be updated"
					", encoderKey: {}"
					", oneParameterPresent: {}",
					encoderKey, oneParameterPresent
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sqlStatement = std::format(
				"update MMS_Encoder {} "
				"where encoderKey = {} ",
				setSQL, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			/*
		if (rowsUpdated != 1)
		{
			string errorMessage = __FILEREF__ + "no update was done"
					+ ", confKey: " + to_string(confKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", lastSQLCommand: " + lastSQLCommand
			;
			SPDLOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::removeEncoder(int64_t encoderKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"WITH rows AS (delete from MMS_Encoder where encoderKey = {} "
				"returning 1) select count(*) from rows",
				encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = std::format(
					"no delete was done"
					", encoderKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					encoderKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

tuple<string, string, string> MMSEngineDBFacade::encoder_LabelPublicServerNameInternalServerName(int64_t encoderKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encoder:.label", "mms_encoder:.publicServerName", "mms_encoder:.internalServerName"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encoderQuery(requestedColumns, encoderKey, fromMaster);

		string label = (*sqlResultSet)[0][0].as<string>("");
		string publicServerName = (*sqlResultSet)[0][1].as<string>("");
		string internalServerName = (*sqlResultSet)[0][2].as<string>("");

		return make_tuple(label, publicServerName, internalServerName);
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_ERROR(
			"NotFound"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);
		*/

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encoderKey: {}"
			", fromMaster: {}",
			encoderKey, fromMaster
		);

		throw e;
	}
}

string MMSEngineDBFacade::encoder_columnAsString(string columnName, int64_t encoderKey, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_encoder:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encoderQuery(requestedColumns, encoderKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>(string());
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_ERROR(
			"NotFound"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);
		*/

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encoderKey: {}"
			", fromMaster: {}",
			encoderKey, fromMaster
		);

		throw e;
	}
}

/*
string MMSEngineDBFacade::encoder_PublicServerName(int64_t encoderKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encoder:.publicServerName"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encoderQuery(requestedColumns, encoderKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encoderKey: {}"
			", fromMaster: {}",
			encoderKey, fromMaster
		);

		throw e;
	}
}

string MMSEngineDBFacade::encoder_InternalServerName(int64_t encoderKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encoder:.internalServerName"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encoderQuery(requestedColumns, encoderKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encoderKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encoderKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encoderKey: {}"
			", fromMaster: {}",
			encoderKey, fromMaster
		);

		throw e;
	}
}
*/

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::encoderQuery(
	vector<string> &requestedColumns, int64_t encoderKey, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/
	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = std::format(
				"Too many rows requested"
				", rows: {}"
				", maxRows: {}",
				rows, _maxRows
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else if ((startIndex != -1 || rows != -1) && orderBy == "")
		{
			// The query optimizer takes LIMIT into account when generating query plans, so you are very likely to get different plans (yielding
			// different row orders) depending on what you give for LIMIT and OFFSET. Thus, using different LIMIT/OFFSET values to select different
			// subsets of a query result will give inconsistent results unless you enforce a predictable result ordering with ORDER BY. This is not a
			// bug; it is an inherent consequence of the fact that SQL does not promise to deliver the results of a query in any particular order
			// unless ORDER BY is used to constrain the order. The rows skipped by an OFFSET clause still have to be computed inside the server;
			// therefore a large OFFSET might be inefficient.
			string errorMessage = std::format(
				"Using startIndex/row without orderBy will give inconsistent results"
				", startIndex: {}"
				", rows: {}"
				", orderBy: {}",
				startIndex, rows, orderBy
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (encoderKey != -1)
				where += std::format("{} encoderKey = {} ", where.size() > 0 ? "and" : "", encoderKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_Encoder "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			sqlResultSet = _postgresHelper.buildResult(res);
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && encoderKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"encoder not found"
					", encoderKey: {}",
					encoderKey
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		addAssociationWorkspaceEncoder(workspaceKey, encoderKey, trans);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey, PostgresConnTrans &trans)
{
	SPDLOG_INFO(
		"Received addAssociationWorkspaceEncoder"
		", workspaceKey: {}"
		", encoderKey: {}",
		workspaceKey, encoderKey
	);

	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_EncoderWorkspaceMapping (workspaceKey, encoderKey) "
				"values ({}, {})",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		throw;
	}
}

bool MMSEngineDBFacade::encoderWorkspaceMapping_isPresent(int64_t workspaceKey, int64_t encoderKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		bool isPresent;
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {}",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			isPresent = trans.transaction->exec1(sqlStatement)[0].as<int64_t>() == 1;
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		return isPresent;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(int64_t workspaceKey, string sharedEncodersPoolLabel, json sharedEncodersLabel)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		vector<int64_t> encoderKeys;
		for (int encoderIndex = 0; encoderIndex < sharedEncodersLabel.size(); encoderIndex++)
		{
			string encoderLabel = JSONUtils::asString(sharedEncodersLabel[encoderIndex]);

			string sqlStatement = std::format(
				"select encoderKey from MMS_Encoder "
				"where label = {}",
				trans.transaction->quote(encoderLabel)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (!empty(res))
				encoderKeys.push_back(res[0]["encoderKey"].as<int64_t>());
			else
			{
				string errorMessage = std::format(
					"No encoder label found"
					", encoderLabel: {}",
					encoderLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		for (int64_t encoderKey : encoderKeys)
			addAssociationWorkspaceEncoder(workspaceKey, encoderKey, trans);

		addEncodersPool(workspaceKey, sharedEncodersPoolLabel, encoderKeys);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::removeAssociationWorkspaceEncoder(int64_t workspaceKey, int64_t encoderKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// se l'encoder che vogliamo rimuovere da un workspace è all'interno di qualche EncodersPool,
		// bisogna rimuoverlo
		{
			string sqlStatement = std::format(
				"delete from MMS_EncoderEncodersPoolMapping "
				"where encodersPoolKey in (select encodersPoolKey from MMS_EncodersPool where workspaceKey = {}) "
				"and encoderKey = {} ",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		{
			string sqlStatement = std::format(
				"WITH rows AS (delete from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} returning 1) select count(*) from rows",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = std::format(
					"no delete was done"
					", encoderKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					encoderKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getEncoderWorkspacesAssociation(int64_t encoderKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		json encoderWorkspacesAssociatedRoot = json::array();
		{
			string sqlStatement = std::format(
				"select w.workspaceKey, w.name "
				"from MMS_Workspace w, MMS_EncoderWorkspaceMapping ewm "
				"where w.workspaceKey = ewm.workspaceKey and ewm.encoderKey = {}",
				encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json encoderWorkspaceAssociatedRoot;

				string field = "workspaceKey";
				encoderWorkspaceAssociatedRoot[field] = row["workspaceKey"].as<int64_t>();

				field = "workspaceName";
				encoderWorkspaceAssociatedRoot[field] = row["name"].as<string>();

				encoderWorkspacesAssociatedRoot.push_back(encoderWorkspaceAssociatedRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		return encoderWorkspacesAssociatedRoot;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getEncoderList(
	bool admin, int start, int rows, bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey, string label, string serverName,
	int port,
	string labelOrder // "" or "asc" or "desc"
)
{
	json encoderListRoot;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getEncoderList"
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
			start, rows, allEncoders, runningInfo, workspaceKey, encoderKey, label, serverName, port, labelOrder
		);

		{
			json requestParametersRoot;

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
				sqlWhere += std::format("and e.encoderKey = {} ", encoderKey);
			else
				sqlWhere += std::format("e.encoderKey = {} ", encoderKey);
		}
		if (label != "")
		{
			if (sqlWhere != "")
				sqlWhere += std::format("and LOWER(e.label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
			else
				sqlWhere += std::format("LOWER(e.label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
		}
		if (serverName != "")
		{
			if (sqlWhere != "")
				sqlWhere += std::format(
					"and (e.publicServerName like {} or e.internalServerName like {}) ", trans.transaction->quote("%" + serverName + "%"),
					trans.transaction->quote("%" + serverName + "%")
				);
			else
				sqlWhere += std::format(
					"(e.publicServerName like {} or e.internalServerName like {}) ", trans.transaction->quote(serverName),
					trans.transaction->quote(serverName)
				);
		}
		if (port != -1)
		{
			if (sqlWhere != "")
				sqlWhere += std::format("and e.port = {} ", port);
			else
				sqlWhere += std::format("e.port = {} ", port);
		}

		if (allEncoders)
		{
			// using just MMS_Encoder
			if (sqlWhere != "")
				sqlWhere = std::format("where {}", sqlWhere);
		}
		else
		{
			// join with MMS_EncoderWorkspaceMapping
			if (sqlWhere != "")
				sqlWhere = std::format(
							   "where e.encoderKey = ewm.encoderKey "
							   "and ewm.workspaceKey = {} and ",
							   workspaceKey
						   ) +
						   sqlWhere;
			else
				sqlWhere = std::format(
					"where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = {} ",
					workspaceKey
				);
		}

		json responseRoot;
		{
			string sqlStatement;
			if (allEncoders)
			{
				sqlStatement = std::format("select count(*) from MMS_Encoder e {}", sqlWhere);
			}
			else
			{
				sqlStatement = std::format(
					"select count(*) "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm {}",
					sqlWhere
				);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		json encodersRoot = json::array();
		{
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

			string sqlStatement;
			if (allEncoders)
				sqlStatement = std::format(
					"select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start
				);
			else
				sqlStatement = std::format(
					"select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json encoderRoot = getEncoderRoot(admin, runningInfo, row);

				encodersRoot.push_back(encoderRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		field = "encoders";
		responseRoot[field] = encodersRoot;

		field = "response";
		encoderListRoot[field] = responseRoot;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return encoderListRoot;
}

json MMSEngineDBFacade::getEncoderRoot(bool admin, bool runningInfo, row &row)
{
	json encoderRoot;

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
			pair<bool, int> encoderRunningDetails = getEncoderInfo(external, protocol, publicServerName, internalServerName, port);
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
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}",
				se->query(), se->what()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}",
				e.what()
			);

		throw;
	}

	return encoderRoot;
}

bool MMSEngineDBFacade::isEncoderRunning(bool external, string protocol, string publicServerName, string internalServerName, int port)
{
	bool isRunning = true;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://" + (external ? publicServerName : internalServerName) + ":" + to_string(port) + _ffmpegEncoderStatusURI;

		vector<string> otherHeaders;
		json infoResponseRoot = CurlWrapper::httpGetJson(
			ffmpegEncoderURL, _ffmpegEncoderInfoTimeout, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword), otherHeaders
		);
	}
	catch (ServerNotReachable e)
	{
		SPDLOG_ERROR(
			"Encoder is not reachable, is it down?"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}

	return isRunning;
}

pair<bool, int> MMSEngineDBFacade::getEncoderInfo(bool external, string protocol, string publicServerName, string internalServerName, int port)
{
	bool isRunning = true;
	int cpuUsage = 0;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://" + (external ? publicServerName : internalServerName) + ":" + to_string(port) + _ffmpegEncoderInfoURI;

		vector<string> otherHeaders;
		json infoResponseRoot = CurlWrapper::httpGetJson(
			ffmpegEncoderURL, _ffmpegEncoderInfoTimeout, CurlWrapper::basicAuthorization(_ffmpegEncoderUser, _ffmpegEncoderPassword), otherHeaders
		);

		string field = "cpuUsage";
		cpuUsage = JSONUtils::asInt(infoResponseRoot, field, 0);
	}
	catch (ServerNotReachable e)
	{
		SPDLOG_ERROR(
			"Encoder is not reachable, is it down?"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"Status URL failed (exception)"
			", ffmpegEncoderURL: {}"
			", exception: {}",
			ffmpegEncoderURL, e.what()
		);

		isRunning = false;
	}

	return make_pair(isRunning, cpuUsage);
}

string MMSEngineDBFacade::getEncodersPoolDetails(int64_t encodersPoolKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getEncodersPoolDetails"
			", encodersPoolKey: {}",
			encodersPoolKey
		);

		string label;

		{
			string sqlStatement = std::format(
				"select label from MMS_EncodersPool "
				"where encodersPoolKey = {}",
				encodersPoolKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (empty(res))
			{
				string errorMessage = std::format(
					"EncodersPool was not found"
					", encodersPoolKey: {}",
					encodersPoolKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (!res[0]["label"].is_null())
				label = res[0]["label"].as<string>();
		}

		return label;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getEncodersPoolList(
	int start, int rows, int64_t workspaceKey, int64_t encodersPoolKey, string label,
	string labelOrder // "" or "asc" or "desc"
)
{
	json encodersPoolListRoot;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getEncodersPoolList"
			", start: {}"
			", rows: {}"
			", workspaceKey: {}"
			", encodersPoolKey: {}"
			", label: {}"
			", labelOrder: {}",
			start, rows, workspaceKey, encodersPoolKey, label, labelOrder
		);

		{
			json requestParametersRoot;

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
		string sqlWhere = std::format("where workspaceKey = {} and label is not NULL ", workspaceKey);
		if (encodersPoolKey != -1)
			sqlWhere += std::format("and encodersPoolKey = {} ", encodersPoolKey);
		if (label != "")
			sqlWhere += std::format("and LOWER(label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_EncodersPool {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		json encodersPoolsRoot = json::array();
		{
			string orderByCondition;
			if (labelOrder != "")
				orderByCondition = "order by label " + labelOrder + " ";

			string sqlStatement =
				std::format("select encodersPoolKey, label from MMS_EncodersPool {} {} limit {} offset {}", sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json encodersPoolRoot;

				int64_t encodersPoolKey = row["encodersPoolKey"].as<int64_t>();

				string field = "encodersPoolKey";
				encodersPoolRoot[field] = encodersPoolKey;

				field = "label";
				encodersPoolRoot[field] = row["label"].as<string>();

				json encodersRoot = json::array();
				{
					string sqlStatement = std::format(
						"select encoderKey from MMS_EncoderEncodersPoolMapping "
						"where encodersPoolKey = {}",
						encodersPoolKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						int64_t encoderKey = row["encoderKey"].as<int64_t>();

						{
							string sqlStatement = std::format(
								"select encoderKey, label, external, enabled, protocol, "
								"publicServerName, internalServerName, port "
								"from MMS_Encoder "
								"where encoderKey = {} ",
								encoderKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							result res = trans.transaction->exec(sqlStatement);
							long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
							SQLQUERYLOG(
								"default", elapsed,
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, trans.connection->getConnectionId(), elapsed
							);
							if (!empty(res))
							{
								bool admin = false;
								bool runningInfo = false;
								auto row1 = res[0];
								json encoderRoot = getEncoderRoot(admin, runningInfo, row1);

								encodersRoot.push_back(encoderRoot);
							}
							else
							{
								string errorMessage = std::format(
									"No encoderKey found"
									", encoderKey: {}",
									encoderKey
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}
				}

				field = "encoders";
				encodersPoolRoot[field] = encodersRoot;

				encodersPoolsRoot.push_back(encodersPoolRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		field = "encodersPool";
		responseRoot[field] = encodersPoolsRoot;

		field = "response";
		encodersPoolListRoot[field] = responseRoot;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return encodersPoolListRoot;
}

int64_t MMSEngineDBFacade::addEncodersPool(int64_t workspaceKey, string label, vector<int64_t> &encoderKeys)
{
	int64_t encodersPoolKey;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// check: every encoderKey shall be already associated to the workspace
		for (int64_t encoderKey : encoderKeys)
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} ",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (count == 0)
			{
				string errorMessage = std::format(
					"Encoder is not already associated to the workspace"
					", workspaceKey: {}"
					", encoderKey: {}",
					workspaceKey, encoderKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"insert into MMS_EncodersPool(workspaceKey, label, "
				"lastEncoderIndexUsed) values ( "
				"{}, {}, 0) returning encodersPoolKey",
				workspaceKey, trans.transaction->quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encodersPoolKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		for (int64_t encoderKey : encoderKeys)
		{
			string sqlStatement = std::format(
				"insert into MMS_EncoderEncodersPoolMapping(encodersPoolKey, "
				"encoderKey) values ( "
				"{}, {})",
				encodersPoolKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return encodersPoolKey;
}

int64_t MMSEngineDBFacade::modifyEncodersPool(int64_t encodersPoolKey, int64_t workspaceKey, string newLabel, vector<int64_t> &newEncoderKeys)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		SPDLOG_INFO(
			"Received modifyEncodersPool"
			", encodersPoolKey: {}"
			", workspaceKey: {}"
			", newLabel: {}"
			", newEncoderKeys.size: {}",
			encodersPoolKey, workspaceKey, newLabel, newEncoderKeys.size()
		);

		// check: every encoderKey shall be already associated to the workspace
		for (int64_t encoderKey : newEncoderKeys)
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} and encoderKey = {} ",
				workspaceKey, encoderKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (count == 0)
			{
				string errorMessage = std::format(
					"Encoder is not already associated to the workspace"
					", workspaceKey: {}"
					", encoderKey: {}",
					workspaceKey, encoderKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"select label from MMS_EncodersPool "
				"where encodersPoolKey = {} ",
				encodersPoolKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (!empty(res))
			{
				string savedLabel = res[0]["label"].as<string>();
				if (savedLabel != newLabel)
				{
					string sqlStatement = std::format(
						"WITH rows AS (update MMS_EncodersPool "
						"set label = {} "
						"where encodersPoolKey = {} returning 1) select count(*) from rows",
						trans.transaction->quote(newLabel), encodersPoolKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, trans.connection->getConnectionId(), elapsed
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = std::format(
							"no update was done"
							", newLabel: {}"
							", encodersPoolKey: {}"
							", rowsUpdated: {}"
							", sqlStatement: {}",
							newLabel, encodersPoolKey, rowsUpdated, sqlStatement
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				vector<int64_t> savedEncoderKeys;
				{
					string sqlStatement = std::format(
						"select encoderKey from MMS_EncoderEncodersPoolMapping "
						"where encodersPoolKey = {}",
						encodersPoolKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						int64_t encoderKey = row["encoderKey"].as<int64_t>();

						savedEncoderKeys.push_back(encoderKey);
					}
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, trans.connection->getConnectionId(), elapsed
					);
				}

				// all the new encoderKey that are not present in savedEncoderKeys have to be added
				for (int64_t newEncoderKey : newEncoderKeys)
				{
					if (find(savedEncoderKeys.begin(), savedEncoderKeys.end(), newEncoderKey) == savedEncoderKeys.end())
					{
						string sqlStatement = std::format(
							"insert into MMS_EncoderEncodersPoolMapping("
							"encodersPoolKey, encoderKey) values ( "
							"{}, {})",
							encodersPoolKey, newEncoderKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						trans.transaction->exec0(sqlStatement);
						long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
						SQLQUERYLOG(
							"default", elapsed,
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, trans.connection->getConnectionId(), elapsed
						);
					}
				}

				// all the saved encoderKey that are not present in encoderKeys have to be removed
				for (int64_t savedEncoderKey : savedEncoderKeys)
				{
					if (find(newEncoderKeys.begin(), newEncoderKeys.end(), savedEncoderKey) == newEncoderKeys.end())
					{
						string sqlStatement = std::format(
							"WITH rows AS (delete from MMS_EncoderEncodersPoolMapping "
							"where encodersPoolKey = {} and encoderKey = {} "
							"returning 1) select count(*) from rows",
							encodersPoolKey, savedEncoderKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
						long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
						SQLQUERYLOG(
							"default", elapsed,
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, trans.connection->getConnectionId(), elapsed
						);
						if (rowsUpdated != 1)
						{
							string errorMessage = std::format(
								"no delete was done"
								", encodersPoolKey: {}"
								", savedEncoderKey: {}"
								", rowsUpdated: {}"
								", sqlStatement: {}",
								encodersPoolKey, savedEncoderKey, rowsUpdated, sqlStatement
							);
							SPDLOG_WARN(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
			else
			{
				string errorMessage = std::format(
					"No encodersPool found"
					", encodersPoolKey: {}",
					encodersPoolKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return encodersPoolKey;
}

void MMSEngineDBFacade::removeEncodersPool(int64_t encodersPoolKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"WITH rows AS (delete from MMS_EncodersPool where encodersPoolKey = {} "
				"returning 1) select count(*) from rows",
				encodersPoolKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = std::format(
					"no delete was done"
					", encodersPoolKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					encodersPoolKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

tuple<int64_t, bool, string, string, string, int> MMSEngineDBFacade::getRunningEncoderByEncodersPool(
	int64_t workspaceKey, string encodersPoolLabel, int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"Received getRunningEncoderByEncodersPool"
			", workspaceKey: {}"
			", encodersPoolLabel: {}"
			", encoderKeyToBeSkipped: {}"
			", externalEncoderAllowed: {}",
			workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped, externalEncoderAllowed
		);

		int lastEncoderIndexUsed;
		int64_t encodersPoolKey;
		{
			// 2025-01-02: ho trovato la select che non ritornava (bloccata) per cui elimino il 'for update'
			// 	tanto serve solamente per rispettare l'ordine di uso degli encoder (lastEncoderIndexUsed) e,
			// 	anche se si dovesse riutilizzare un encoder, non è un problema, meglio evitare blocchi/deadlock
			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = std::format(
					"select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool "
					"where workspaceKey = {} "
					"and label is null", // for update",
					workspaceKey
				);
			else
				sqlStatement = std::format(
					"select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool "
					"where workspaceKey = {} "
					"and label = {}", // for update",
					workspaceKey, trans.transaction->quote(encodersPoolLabel)
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (empty(res))
			{
				string errorMessage = std::format(
					"encodersPool was not found"
					", workspaceKey: {}"
					", encodersPoolLabel: {}",
					workspaceKey, encodersPoolLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			lastEncoderIndexUsed = res[0]["lastEncoderIndexUsed"].as<int>();
			encodersPoolKey = res[0]["encodersPoolKey"].as<int64_t>();
		}

		int encodersNumber;
		{
			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = std::format(
					"select count(*) from MMS_EncoderWorkspaceMapping "
					"where workspaceKey = {} ",
					workspaceKey
				);
			else
				sqlStatement = std::format(
					"select count(*) from MMS_EncoderEncodersPoolMapping "
					"where encodersPoolKey = {} ",
					encodersPoolKey
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encodersNumber = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
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

		// trovo l'encoder successivo da utilizzare. Ci sono encoder che potrebbero non essere utilizzati, ad esempio perchè gli external non sono
		// allowed oppure perchè non sono enabled oppure perchè sono encoderKeyToBeSkipped o perchè non sono running
		while (!encoderFound && encoderIndex < encodersNumber)
		{
			encoderIndex++;

			newLastEncoderIndexUsed = (newLastEncoderIndexUsed + 1) % encodersNumber;

			string sqlStatement;
			if (encodersPoolLabel == "")
				sqlStatement = std::format(
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm "
					"where e.encoderKey = ewm.encoderKey and ewm.workspaceKey = {} "
					"order by e.publicServerName "
					"limit 1 offset {}",
					workspaceKey, newLastEncoderIndexUsed
				);
			else
				sqlStatement = std::format(
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderEncodersPoolMapping eepm "
					"where e.encoderKey = eepm.encoderKey and eepm.encodersPoolKey = {} "
					"order by e.publicServerName "
					"limit 1 offset {}",
					encodersPoolKey, newLastEncoderIndexUsed
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (empty(res))
			{
				string errorMessage = std::format(
					"Encoder details not found"
					", workspaceKey: {}"
					", encodersPoolKey: {}",
					workspaceKey, encodersPoolKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

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
				SPDLOG_INFO(
					"getEncoderByEncodersPool, skipped encoderKey because encoder not enabled"
					", workspaceKey: {}"
					", encodersPoolLabel: {}"
					", enabled: {}",
					workspaceKey, encodersPoolLabel, enabled
				);

				continue;
			}
			else if (encoderKeyToBeSkipped != -1 && encoderKeyToBeSkipped == encoderKey)
			{
				SPDLOG_INFO(
					"getEncoderByEncodersPool, skipped encoderKey"
					", workspaceKey: {}"
					", encodersPoolLabel: {}"
					", encoderKeyToBeSkipped: {}",
					workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped
				);

				continue;
			}
			else
			{
				if (!isEncoderRunning(external, protocol, publicServerName, internalServerName, port))
				{
					SPDLOG_INFO(
						"getEncoderByEncodersPool, dicarded encoderKey because not running"
						", workspaceKey: {}"
						", encodersPoolLabel: {}",
						workspaceKey, encodersPoolLabel
					);

					continue;
				}
			}

			encoderFound = true;
		}

		if (!encoderFound)
		{
			string errorMessage = std::format(
				"Encoder was not found"
				", workspaceKey: {}"
				", encodersPoolLabel: {}"
				", encoderKeyToBeSkipped: {}",
				workspaceKey, encodersPoolLabel, encoderKeyToBeSkipped
			);
			SPDLOG_ERROR(errorMessage);

			throw EncoderNotFound(errorMessage);
		}

		{
			string sqlStatement = std::format(
				"update MMS_EncodersPool set lastEncoderIndexUsed = {} "
				"where encodersPoolKey = {} ",
				newLastEncoderIndexUsed, encodersPoolKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			/*
			if (rowsUpdated != 1)
			{
				SPDLOG_WARN(
					"no update was done"
					", newLastEncoderIndexUsed: {}"
					", encodersPoolKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					newLastEncoderIndexUsed, encodersPoolKey, rowsUpdated, sqlStatement
				);

				// in case of one encoder, no update is done
				// because newLastEncoderIndexUsed is always the same

				// throw runtime_error(errorMessage);
			}
			*/
		}

		return make_tuple(encoderKey, external, protocol, publicServerName, internalServerName, port);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

int MMSEngineDBFacade::getEncodersNumberByEncodersPool(int64_t workspaceKey, string encodersPoolLabel)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getEncodersNumberByEncodersPool"
			", workspaceKey: {}"
			", encodersPoolLabel: {}",
			workspaceKey, encodersPoolLabel
		);

		int encodersNumber;
		if (encodersPoolLabel != "")
		{
			int64_t encodersPoolKey;
			{
				string sqlStatement = std::format(
					"select encodersPoolKey from MMS_EncodersPool "
					"where workspaceKey = {} "
					"and label = {} ",
					workspaceKey, trans.transaction->quote(encodersPoolLabel)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				SQLQUERYLOG(
					"default", elapsed,
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, trans.connection->getConnectionId(), elapsed
				);
				if (empty(res))
				{
					string errorMessage = std::format(
						"lastEncoderIndexUsed was not found"
						", workspaceKey: {}"
						", encodersPoolLabel: {}",
						workspaceKey, encodersPoolLabel
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersPoolKey = res[0]["encodersPoolKey"].as<int64_t>();
			}

			{
				string sqlStatement = std::format(
					"select count(*) from MMS_EncoderEncodersPoolMapping "
					"where encodersPoolKey = {} ",
					encodersPoolKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodersNumber = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				SQLQUERYLOG(
					"default", elapsed,
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, trans.connection->getConnectionId(), elapsed
				);
			}
		}
		else
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = {} ",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			encodersNumber = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		return encodersNumber;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

pair<string, bool> MMSEngineDBFacade::getEncoderURL(int64_t encoderKey, string serverName)
{
	json encodersPoolListRoot;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getEncoderURL"
			", encoderKey: {}"
			", serverName: {}",
			encoderKey, serverName
		);

		bool external;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port;
		{
			string sqlStatement = std::format(
				"select external, protocol, publicServerName, internalServerName, port "
				"from MMS_Encoder where encoderKey = {} ",
				encoderKey
			)
				// 2022-05-22. Scenario: tried to kill a job of a just disabled encoder
				//	The kill did not work becuase this method did not return the URL
				//	because of the below sql condition.
				//	It was commented because it is not in this method that has to be
				//	checked the enabled flag, this method is specific to return a url
				//	of an encoder and ddoes not have to check the enabled flag
				// "and enabled = 1 "
				;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
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
				string errorMessage = std::format(
					"Encoder details not found or not enabled"
					", encoderKey: {}",
					encoderKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string encoderURL;
		if (serverName != "")
			encoderURL = protocol + "://" + serverName + ":" + to_string(port);
		else
		{
			if (external)
				encoderURL = protocol + "://" + publicServerName + ":" + to_string(port);
			else
				encoderURL = protocol + "://" + internalServerName + ":" + to_string(port);
		}

		SPDLOG_INFO(
			"getEncoderURL"
			", encoderKey: {}"
			", serverName: {}"
			", encoderURL: {}",
			encoderKey, serverName, encoderURL
		);

		return make_pair(encoderURL, external);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			SPDLOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}
