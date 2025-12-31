
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
using namespace pqxx;

json MMSEngineDBFacade::addYouTubeConf(int64_t workspaceKey, string label, string tokenType, string refreshToken, string accessToken)
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
		int64_t confKey;
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_YouTube(workspaceKey, label, tokenType, "
				"refreshToken, accessToken) values ("
				"{}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(tokenType), trans.transaction->quote(refreshToken),
				trans.transaction->quote(accessToken)
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

		json youTubeConfRoot;
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
				youTubeConfRoot[field] = nullptr;
			}
			else
			{
				youTubeConfRoot[field] = nullptr;

				field = "accessToken";
				youTubeConfRoot[field] = accessToken;
			}
		}

		return youTubeConfRoot;
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

json MMSEngineDBFacade::modifyYouTubeConf(
	int64_t confKey, int64_t workspaceKey, string label, bool labelModified, string tokenType, bool tokenTypeModified, string refreshToken,
	bool refreshTokenModified, string accessToken, bool accessTokenModified
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

			if (labelModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.transaction->quote(label));
				oneParameterPresent = true;
			}

			if (tokenTypeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tokenType = " + trans.transaction->quote(tokenType));
				oneParameterPresent = true;
			}

			if (refreshTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("refreshToken = " + (refreshToken == "" ? "null" : trans.transaction->quote(refreshToken)));
				oneParameterPresent = true;
			}

			if (accessTokenModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("accessToken = " + (accessToken == "" ? "null" : trans.transaction->quote(accessToken)));
				oneParameterPresent = true;
			}

			string sqlStatement = std::format(
				"update MMS_Conf_YouTube {} "
				"where confKey = {} and workspaceKey = {} ",
				setSQL, confKey, workspaceKey
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

		json youTubeConfRoot;
		{
			string sqlStatement = std::format(
				"select confKey, label, tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where confKey = {} and workspaceKey = {}",
				confKey, workspaceKey
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
					"No YouTube conf found"
					", confKey: {}"
					", workspaceKey: {}"
					", sqlStatement: {}",
					confKey, workspaceKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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
					youTubeConfRoot[field] = nullptr;
				else
					youTubeConfRoot[field] = res[0]["refreshToken"].as<string>();

				field = "accessToken";
				if (res[0]["accessToken"].is_null())
					youTubeConfRoot[field] = nullptr;
				else
					youTubeConfRoot[field] = res[0]["accessToken"].as<string>();
			}
		}

		return youTubeConfRoot;
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

void MMSEngineDBFacade::removeYouTubeConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_YouTube where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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

json MMSEngineDBFacade::getYouTubeConfList(int64_t workspaceKey, string label)
{
	json youTubeConfListRoot;

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
			"getYouTubeConfList"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			field = "requestParameters";
			youTubeConfListRoot[field] = requestParametersRoot;

			if (label != "")
			{
				field = "label";
				youTubeConfListRoot[field] = label;
			}
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (label != "")
			sqlWhere += std::format("and LOWER(label) = LOWER({}) ", trans.transaction->quote(label));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_YouTube {}", sqlWhere);
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

		json youTubeRoot = json::array();
		{
			string sqlStatement = std::format(
				"select confKey, label, tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json youTubeConfRoot;

				field = "confKey";
				youTubeConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				youTubeConfRoot[field] = row["label"].as<string>();

				field = "tokenType";
				youTubeConfRoot[field] = row["tokenType"].as<string>();

				field = "refreshToken";
				if (row["refreshToken"].is_null())
					youTubeConfRoot[field] = nullptr;
				else
					youTubeConfRoot[field] = row["refreshToken"].as<string>();

				field = "accessToken";
				if (row["accessToken"].is_null())
					youTubeConfRoot[field] = nullptr;
				else
					youTubeConfRoot[field] = row["accessToken"].as<string>();

				youTubeRoot.push_back(youTubeConfRoot);
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

		field = "youTubeConf";
		responseRoot[field] = youTubeRoot;

		field = "response";
		youTubeConfListRoot[field] = responseRoot;
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

	return youTubeConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getYouTubeDetailsByConfigurationLabel(int64_t workspaceKey, string youTubeConfigurationLabel)
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
		string youTubeTokenType;
		string youTubeRefreshToken;
		string youTubeAccessToken;

		SPDLOG_INFO(
			"getYouTubeDetailsByConfigurationLabel"
			", workspaceKey: {}"
			", youTubeConfigurationLabel: {}",
			workspaceKey, youTubeConfigurationLabel
		);

		{
			string sqlStatement = std::format(
				"select tokenType, refreshToken, accessToken "
				"from MMS_Conf_YouTube "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.transaction->quote(youTubeConfigurationLabel)
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
					"Configuration label not found"
					", workspaceKey: {}"
					", youTubeConfigurationLabel: {}",
					workspaceKey, youTubeConfigurationLabel
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			youTubeTokenType = res[0]["tokenType"].as<string>();
			if (!res[0]["refreshToken"].is_null())
				youTubeRefreshToken = res[0]["refreshToken"].as<string>();
			if (!res[0]["accessToken"].is_null())
				youTubeAccessToken = res[0]["accessToken"].as<string>();
		}

		return make_tuple(youTubeTokenType, youTubeRefreshToken, youTubeAccessToken);
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

int64_t MMSEngineDBFacade::addFacebookConf(int64_t workspaceKey, string label, string userAccessToken)
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
				"insert into MMS_Conf_Facebook(workspaceKey, label, modificationDate, userAccessToken) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(userAccessToken)
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

void MMSEngineDBFacade::modifyFacebookConf(int64_t confKey, int64_t workspaceKey, string label, string userAccessToken)
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
				"update MMS_Conf_Facebook set label = {}, userAccessToken = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(userAccessToken), confKey, workspaceKey
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

void MMSEngineDBFacade::removeFacebookConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_Facebook where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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

json MMSEngineDBFacade::getFacebookConfList(int64_t workspaceKey, int64_t confKey, string label)
{
	json facebookConfListRoot;

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
			"getFacebookConfList"
			", workspaceKey: {}"
			", confKey: {}"
			", label: {}",
			workspaceKey, confKey, label
		);

		{
			json requestParametersRoot;

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

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += std::format("and label = {} ", trans.transaction->quote(label));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_Facebook {}", sqlWhere);
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

		json facebookRoot = json::array();
		{
			string sqlStatement = std::format(
				"select confKey, label, userAccessToken, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
				"from MMS_Conf_Facebook {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json facebookConfRoot;

				field = "confKey";
				facebookConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				facebookConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				facebookConfRoot[field] = row["modificationDate"].as<string>();

				field = "userAccessToken";
				facebookConfRoot[field] = row["userAccessToken"].as<string>();

				facebookRoot.push_back(facebookConfRoot);
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

		field = "facebookConf";
		responseRoot[field] = facebookRoot;

		field = "response";
		facebookConfListRoot[field] = responseRoot;
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

	return facebookConfListRoot;
}

string MMSEngineDBFacade::getFacebookUserAccessTokenByConfigurationLabel(int64_t workspaceKey, string facebookConfigurationLabel)
{
	string facebookUserAccessToken;

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
			"getFacebookUserAccessTokenByConfigurationLabel"
			", workspaceKey: {}"
			", facebookConfigurationLabel: {}",
			workspaceKey, facebookConfigurationLabel
		);

		{
			string sqlStatement = std::format(
				"select userAccessToken from MMS_Conf_Facebook where workspaceKey = {} and label = {}", workspaceKey,
				trans.transaction->quote(facebookConfigurationLabel)
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
					"Configuration label not found"
					", workspaceKey: {}"
					", facebookConfigurationLabel: {}",
					workspaceKey, facebookConfigurationLabel
				);

				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			facebookUserAccessToken = res[0]["userAccessToken"].as<string>();
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

	return facebookUserAccessToken;
}

int64_t MMSEngineDBFacade::addTwitchConf(int64_t workspaceKey, string label, string refreshToken)
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
				"insert into MMS_Conf_Twitch(workspaceKey, label, modificationDate, refreshToken) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(refreshToken)
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

void MMSEngineDBFacade::modifyTwitchConf(int64_t confKey, int64_t workspaceKey, string label, string refreshToken)
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
				"update MMS_Conf_Twitch set label = {}, refreshToken = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(refreshToken), confKey, workspaceKey
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

void MMSEngineDBFacade::removeTwitchConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_Twitch where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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

json MMSEngineDBFacade::getTwitchConfList(int64_t workspaceKey, int64_t confKey, string label)
{
	json twitchConfListRoot;

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
			"getTwitchConfList"
			", workspaceKey: {}"
			", confKey: {}"
			", label: {}",
			workspaceKey, confKey, label
		);

		{
			json requestParametersRoot;

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

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += std::format("and label = {} ", trans.transaction->quote(label));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_Twitch {}", sqlWhere);
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

		json twitchRoot = json::array();
		{
			string sqlStatement = std::format(
				"select confKey, label, refreshToken, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
				"from MMS_Conf_Twitch {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json twitchConfRoot;

				field = "confKey";
				twitchConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				twitchConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				twitchConfRoot[field] = row["modificationDate"].as<string>();

				field = "refreshToken";
				twitchConfRoot[field] = row["refreshToken"].as<string>();

				twitchRoot.push_back(twitchConfRoot);
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

		field = "twitchConf";
		responseRoot[field] = twitchRoot;

		field = "response";
		twitchConfListRoot[field] = responseRoot;
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

	return twitchConfListRoot;
}

string MMSEngineDBFacade::getTwitchUserAccessTokenByConfigurationLabel(int64_t workspaceKey, string twitchConfigurationLabel)
{
	string twitchRefreshToken;

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
			"getTwitchUserAccessTokenByConfigurationLabel"
			", workspaceKey: {}"
			", twitchConfigurationLabel: {}",
			workspaceKey, twitchConfigurationLabel
		);

		{
			string sqlStatement = std::format(
				"select refreshToken from MMS_Conf_Twitch where workspaceKey = {} and label = {}", workspaceKey,
				trans.transaction->quote(twitchConfigurationLabel)
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
					"Configuration label not found"
					", workspaceKey: {}"
					", twitchConfigurationLabel: {}",
					workspaceKey, twitchConfigurationLabel
				);

				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			twitchRefreshToken = res[0]["refreshToken"].as<string>();
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

	return twitchRefreshToken;
}

int64_t MMSEngineDBFacade::addTiktokConf(int64_t workspaceKey, string label, string token)
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
				"insert into MMS_Conf_Tiktok(workspaceKey, label, modificationDate, token) "
				"values ({}, {}, now() at time zone 'utc', {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(token)
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

void MMSEngineDBFacade::modifyTiktokConf(int64_t confKey, int64_t workspaceKey, string label, string token)
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
				"update MMS_Conf_Tiktok set label = {}, token = {}, "
				"modificationDate = now() at time zone 'utc' "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(token), confKey, workspaceKey
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

void MMSEngineDBFacade::removeTiktokConf(int64_t workspaceKey, int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_Tiktok where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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

json MMSEngineDBFacade::getTiktokConfList(int64_t workspaceKey, int64_t confKey, string label)
{
	json tiktokConfListRoot;

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
			"getTiktokConfList"
			", workspaceKey: {}"
			", confKey: {}"
			", label: {}",
			workspaceKey, confKey, label
		);

		{
			json requestParametersRoot;

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

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += std::format("and label = {} ", trans.transaction->quote(label));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_Tiktok {}", sqlWhere);
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

		json tiktokRoot = json::array();
		{
			string sqlStatement = std::format(
				"select confKey, label, token, "
				"to_char(modificationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as modificationDate "
				"from MMS_Conf_Tiktok {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json tiktokConfRoot;

				field = "confKey";
				tiktokConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				tiktokConfRoot[field] = row["label"].as<string>();

				field = "modificationDate";
				tiktokConfRoot[field] = row["modificationDate"].as<string>();

				field = "token";
				tiktokConfRoot[field] = row["token"].as<string>();

				tiktokRoot.push_back(tiktokConfRoot);
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

		field = "tiktokConf";
		responseRoot[field] = tiktokRoot;

		field = "response";
		tiktokConfListRoot[field] = responseRoot;
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

	return tiktokConfListRoot;
}

string MMSEngineDBFacade::getTiktokTokenByConfigurationLabel(int64_t workspaceKey, string tiktokConfigurationLabel)
{
	string tiktokToken;

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
			"getTiktokTokenByConfigurationLabel"
			", workspaceKey: {}"
			", tiktokConfigurationLabel: {}",
			workspaceKey, tiktokConfigurationLabel
		);

		{
			string sqlStatement = std::format(
				"select token from MMS_Conf_Tiktok where workspaceKey = {} and label = {}", workspaceKey,
				trans.transaction->quote(tiktokConfigurationLabel)
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
					"Configuration label not found"
					", workspaceKey: {}"
					", tiktokConfigurationLabel: {}",
					workspaceKey, tiktokConfigurationLabel
				);

				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			tiktokToken = res[0]["token"].as<string>();
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

	return tiktokToken;
}
