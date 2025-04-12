
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <cstdint>

int64_t
MMSEngineDBFacade::addFTPConf(int64_t workspaceKey, string label, string server, int port, string userName, string password, string remoteDirectory)
{
	int64_t confKey;

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
				"insert into MMS_Conf_FTP(workspaceKey, label, server, port, userName, password, "
				"remoteDirectory) values ("
				"{}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(server), port, trans.transaction->quote(userName),
				trans.transaction->quote(password), trans.transaction->quote(remoteDirectory)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyFTPConf(
	int64_t confKey, int64_t workspaceKey, string label, string server, int port, string userName, string password, string remoteDirectory
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
			string sqlStatement = std::format(
				"update MMS_Conf_FTP set label = {}, server = {}, port = {}, "
				"userName = {}, password = {}, remoteDirectory = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(server), port, trans.transaction->quote(userName),
				trans.transaction->quote(password), trans.transaction->quote(remoteDirectory), confKey, workspaceKey
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

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

void MMSEngineDBFacade::removeFTPConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_FTP where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			int rowsUpdated = res.affected_rows();
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
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

json MMSEngineDBFacade::getFTPConfList(int64_t workspaceKey)
{
	json ftpConfListRoot;

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
			"getFTPConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			field = "requestParameters";
			ftpConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_FTP {}", sqlWhere);
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

		json ftpRoot = json::array();
		{
			string sqlStatement =
				std::format("select confKey, label, server, port, userName, password, remoteDirectory from MMS_Conf_FTP {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json ftpConfRoot;

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

				ftpRoot.push_back(ftpConfRoot);
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

		field = "ftpConf";
		responseRoot[field] = ftpRoot;

		field = "response";
		ftpConfListRoot[field] = responseRoot;
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

	return ftpConfListRoot;
}

tuple<string, int, string, string, string> MMSEngineDBFacade::getFTPByConfigurationLabel(int64_t workspaceKey, string label)
{
	tuple<string, int, string, string, string> ftp;

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
		SPDLOG_INFO(
			"getFTPByConfigurationLabel"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		{
			string sqlStatement = std::format(
				"select server, port, userName, password, remoteDirectory from MMS_Conf_FTP "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.transaction->quote(label)
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
					"Configuration not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string server = res[0]["server"].as<string>();
			int port = res[0]["port"].as<int>();
			string userName = res[0]["userName"].as<string>();
			string password = res[0]["password"].as<string>();
			string remoteDirectory = res[0]["remoteDirectory"].as<string>();

			ftp = make_tuple(server, port, userName, password, remoteDirectory);
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

	return ftp;
}

int64_t MMSEngineDBFacade::addEMailConf(int64_t workspaceKey, string label, string addresses, string subject, string message)
{
	int64_t confKey;

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
				"insert into MMS_Conf_EMail(workspaceKey, label, addresses, subject, message) values ("
				"{}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(addresses), trans.transaction->quote(subject),
				trans.transaction->quote(message)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyEMailConf(int64_t confKey, int64_t workspaceKey, string label, string addresses, string subject, string message)
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
				"update MMS_Conf_EMail set label = {}, addresses = {}, subject = {}, "
				"message = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(addresses), trans.transaction->quote(subject),
				trans.transaction->quote(message), confKey, workspaceKey
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

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

void MMSEngineDBFacade::removeEMailConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_EMail where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			int rowsUpdated = res.affected_rows();
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
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

json MMSEngineDBFacade::getEMailConfList(int64_t workspaceKey)
{
	json emailConfListRoot;

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
			"getEMailConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			field = "requestParameters";
			emailConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_EMail {}", sqlWhere);
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

		json emailRoot = json::array();
		{
			string sqlStatement = std::format("select confKey, label, addresses, subject, message from MMS_Conf_EMail {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json emailConfRoot;

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

				emailRoot.push_back(emailConfRoot);
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

		field = "emailConf";
		responseRoot[field] = emailRoot;

		field = "response";
		emailConfListRoot[field] = responseRoot;
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

	return emailConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getEMailByConfigurationLabel(int64_t workspaceKey, string label)
{
	tuple<string, string, string> email;

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
		SPDLOG_INFO(
			"getEMailByConfigurationLabel"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		{
			string sqlStatement = std::format(
				"select addresses, subject, message from MMS_Conf_EMail "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.transaction->quote(label)
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
					"Configuration not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string addresses = res[0]["addresses"].as<string>();
			string subject = res[0]["subject"].as<string>();
			string message = res[0]["message"].as<string>();

			email = make_tuple(addresses, subject, message);
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

	return email;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
json MMSEngineDBFacade::getStreamInputRoot(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string configurationLabel, string useVideoTrackFromPhysicalPathName,
	string useVideoTrackFromPhysicalDeliveryURL, int maxWidth, string userAgent, string otherInputOptions, string taskEncodersPoolLabel,
	json filtersRoot
)
{
	json streamInputRoot;

	try
	{
		int64_t confKey = -1;
		string streamSourceType;
		string encodersPoolLabel;
		string pullUrl;
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
			tie(confKey, streamSourceType, encodersPoolLabel, pullUrl, ignore, ignore, pushListenTimeout, captureVideoDeviceNumber,
				captureVideoInputFormat, captureFrameRate, captureWidth, captureHeight, captureAudioDeviceNumber, captureChannelsNumber,
				tvSourceTVConfKey) = stream_aLot(workspace->_workspaceKey, configurationLabel);

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
		string url;

		int64_t pushEncoderKey = -1;

		if (streamSourceType == "IP_PULL")
		{
			url = pullUrl;

			string youTubePrefix1("https://www.youtube.com/");
			string youTubePrefix2("https://youtu.be/");
			if ((url.size() >= youTubePrefix1.size() && 0 == url.compare(0, youTubePrefix1.size(), youTubePrefix1)) ||
				(url.size() >= youTubePrefix2.size() && 0 == url.compare(0, youTubePrefix2.size(), youTubePrefix2)))
			{
				url = getStreamingYouTubeLiveURL(workspace, ingestionJobKey, confKey, url);
			}
		}
		else if (streamSourceType == "IP_PUSH")
		{
			tie(pushEncoderKey, url) = getStreamInputPushDetails(workspace->_workspaceKey, ingestionJobKey, configurationLabel);
		}
		else if (streamSourceType == "TV")
		{
			bool warningIfMissing = false;
			tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int> tvChannelConfDetails =
				getSourceTVStreamDetails(tvSourceTVConfKey, warningIfMissing);

			tie(tvType, tvServiceId, tvFrequency, tvSymbolRate, tvBandwidthInHz, tvModulation, tvVideoPid, tvAudioItalianPid) = tvChannelConfDetails;
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

		// field = "pushEncoderName";
		// streamInputRoot[field] = pushEncoderName;

		// The taskEncodersPoolLabel (parameter of the Task/IngestionJob) overrides the one included
		// in ChannelConf if present
		field = "encodersPoolLabel";
		if (taskEncodersPoolLabel != "")
			streamInputRoot[field] = taskEncodersPoolLabel;
		else
			streamInputRoot[field] = encodersPoolLabel;

		field = "url";
		streamInputRoot[field] = url;

		field = "filters";
		streamInputRoot[field] = filtersRoot;

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
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"getStreamInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getStreamInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR("getStreamInputRoot failed");

		throw e;
	}

	return streamInputRoot;
}

// this method is added here just because it is called by both MMSEngineDBFacade::getStreamInputRoot and EncoderProxy_LiveProxy.cpp
pair<int64_t, string> MMSEngineDBFacade::getStreamInputPushDetails(int64_t workspaceKey, int64_t ingestionJobKey, string configurationLabel)
{
	try
	{
		string streamSourceType;
		string pushProtocol;
		int64_t pushEncoderKey = -1;
		int pushServerPort = -1;
		string pushUri;

		bool pushPublicEncoderName = false;
		tie(streamSourceType, pushProtocol, pushEncoderKey, pushPublicEncoderName, pushServerPort, pushUri) =
			stream_pushInfo(workspaceKey, configurationLabel);

		string url;
		if (streamSourceType == "IP_PUSH")
		{
			if (pushEncoderKey < 0)
			{
				string errorMessage = std::format(
					"Wrong pushEncoderKey in case of IP_PUSH"
					", configurationLabel: {}"
					", pushEncoderKey: {}",
					configurationLabel, pushEncoderKey
				);

				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			url = getStreamPushServerUrl(workspaceKey, ingestionJobKey, configurationLabel, pushEncoderKey, pushPublicEncoderName, true);
		}

		return make_pair(pushEncoderKey, url);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"getStreamInputPushDetails failed"
			", ingestionJobKey: {}"
			", configurationLabel: {}"
			", e.what(): {}",
			ingestionJobKey, configurationLabel, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getStreamInputPushDetails failed"
			", ingestionJobKey: {}"
			", configurationLabel: {}"
			", e.what(): {}",
			ingestionJobKey, configurationLabel, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getStreamInputPushDetails failed"
			", ingestionJobKey: {}"
			", configurationLabel: {}",
			ingestionJobKey, configurationLabel
		);

		throw e;
	}
}

string MMSEngineDBFacade::getStreamPushServerUrl(
	int64_t workspaceKey, int64_t ingestionJobKey, string streamConfigurationLabel, int64_t pushEncoderKey, bool pushPublicEncoderName,
	bool pushUriToBeAdded
)
{
	try
	{
		auto [pushEncoderLabel, publicServerName, internalServerName] = // getEncoderDetails(pushEncoderKey);
			encoder_LabelPublicServerNameInternalServerName(pushEncoderKey);

		string pushProtocol;
		int pushServerPort;
		string pushUri;
		tie(ignore, pushProtocol, ignore, ignore, pushServerPort, pushUri) = stream_pushInfo(workspaceKey, streamConfigurationLabel);

		string url;
		if (pushProtocol == "srt")
			url = pushProtocol + "://" + (pushPublicEncoderName ? publicServerName : internalServerName) + ":" + to_string(pushServerPort) +
				  "?mode=listener";
		else
		{
			url = pushProtocol + "://" + (pushPublicEncoderName ? publicServerName : internalServerName) + ":" + to_string(pushServerPort);
			if (pushUriToBeAdded)
				url += pushUri;
		}

		return url;
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"getPushServerUrl failed"
			", ingestionJobKey: {}"
			", e.what(): {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getPushServerUrl failed"
			", ingestionJobKey: {}"
			", e.what(): {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getPushServerUrl failed"
			", ingestionJobKey: {}",
			ingestionJobKey
		);

		throw e;
	}
}

// this method is added here just because it is called by both API and MMSServiceProcessor
json MMSEngineDBFacade::getVodInputRoot(
	MMSEngineDBFacade::ContentType vodContentType, vector<tuple<int64_t, string, string, string>> &sources, json filtersRoot, string otherInputOptions
)
{
	json vodInputRoot;

	try
	{
		vodInputRoot["vodContentType"] = MMSEngineDBFacade::toString(vodContentType);
		vodInputRoot["otherInputOptions"] = otherInputOptions;

		json sourcesRoot = json::array();

		for (tuple<int64_t, string, string, string> source : sources)
		{
			int64_t physicalPathKey;
			string mediaItemTitle;
			string sourcePhysicalPathName;
			string sourcePhysicalDeliveryURL;

			tie(physicalPathKey, mediaItemTitle, sourcePhysicalPathName, sourcePhysicalDeliveryURL) = source;

			json sourceRoot;

			sourceRoot["mediaItemTitle"] = mediaItemTitle;

			sourceRoot["sourcePhysicalPathName"] = sourcePhysicalPathName;

			sourceRoot["physicalPathKey"] = physicalPathKey;

			sourceRoot["sourcePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;

			sourcesRoot.push_back(sourceRoot);
		}

		vodInputRoot["sources"] = sourcesRoot;

		vodInputRoot["filters"] = filtersRoot;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getVodInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getVodInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}

	return vodInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
json MMSEngineDBFacade::getCountdownInputRoot(
	string mmsSourceVideoAssetPathName, string mmsSourceVideoAssetDeliveryURL, int64_t physicalPathKey, int64_t videoDurationInMilliSeconds,
	json filtersRoot
)
{
	json countdownInputRoot;

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

		field = "filters";
		countdownInputRoot[field] = filtersRoot;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getCountdownInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getCountdownInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}

	return countdownInputRoot;
}

// this method is added here just because it is called by both API and MMSServiceProcessor
json MMSEngineDBFacade::getDirectURLInputRoot(string url, json filtersRoot)
{
	json directURLInputRoot;

	try
	{
		string field = "url";
		directURLInputRoot[field] = url;

		field = "filters";
		directURLInputRoot[field] = filtersRoot;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"getDirectURLInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"getDirectURLInputRoot failed"
			", e.what(): {}",
			e.what()
		);

		throw e;
	}

	return directURLInputRoot;
}
