
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include <random>

int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	int64_t userKey, int64_t workspaceKey, string label, int64_t thumbnailMediaItemKey, const string_view& jsonWorkflow, bool admin
)
{
	int64_t workflowLibraryKey;
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
		workflowLibraryKey = addUpdateWorkflowAsLibrary(trans, userKey, workspaceKey, label, thumbnailMediaItemKey, jsonWorkflow, admin);
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

	return workflowLibraryKey;
}

int64_t MMSEngineDBFacade::addUpdateWorkflowAsLibrary(
	PostgresConnTrans &trans, int64_t userKey, int64_t workspaceKey, string label, int64_t thumbnailMediaItemKey,
	const string_view& jsonWorkflow, bool admin
)
{
	int64_t workflowLibraryKey;

	try
	{
		{
			// in case workspaceKey == -1 (MMS library), only ADMIN can add/update it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = std::format(
					"select workflowLibraryKey from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = {}",
					trans.transaction->quote(label)
				);
			else
				sqlStatement = std::format(
					"select workflowLibraryKey, creatorUserKey from MMS_WorkflowLibrary "
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
			if (!empty(res))
			{
				// two options:
				// 1. MMS library: it is an admin user (check already done, update can be done
				// 2. NO MMS Library: only creatorUserKey can do the update
				//		We should add a rights like 'Allow Update Workflow Library even if not the creator'
				//		but this is not present yet
				workflowLibraryKey = res[0]["workflowLibraryKey"].as<int64_t>();
				if (workspaceKey != -1)
				{
					int64_t creatorUserKey = res[0]["creatorUserKey"].as<int64_t>();
					if (creatorUserKey != userKey && !admin)
					{
						string errorMessage = string("User does not have the permission to add/update MMS WorkflowAsLibrary") +
											  ", creatorUserKey: " + to_string(creatorUserKey) + ", userKey: " + to_string(userKey);
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				string sqlStatement = std::format(
					"update MMS_WorkflowLibrary set lastUpdateUserKey = {}, "
					"thumbnailMediaItemKey = {}, jsonWorkflow = {} "
					"where workflowLibraryKey = {} ",
					userKey == -1 ? "null" : to_string(userKey), thumbnailMediaItemKey == -1 ? "null" : to_string(thumbnailMediaItemKey),
					trans.transaction->quote(jsonWorkflow), workflowLibraryKey
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
			else
			{
				string sqlStatement = std::format(
					"insert into MMS_WorkflowLibrary ("
					"workspaceKey, creatorUserKey, lastUpdateUserKey, "
					"label, thumbnailMediaItemKey, jsonWorkflow) values ("
					"{}, {}, {}, {}, {}, {}) returning workflowLibraryKey",
					workspaceKey == -1 ? "null" : to_string(workspaceKey), workspaceKey == -1 ? "null" : to_string(userKey),
					userKey == -1 ? "null" : to_string(userKey), trans.transaction->quote(label),
					thumbnailMediaItemKey == -1 ? "null" : to_string(thumbnailMediaItemKey), trans.transaction->quote(jsonWorkflow)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				workflowLibraryKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return workflowLibraryKey;
}

void MMSEngineDBFacade::removeWorkflowAsLibrary(int64_t userKey, int64_t workspaceKey, int64_t workflowLibraryKey, bool admin)
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
			// in case workspaceKey == -1 (MMS library), only ADMIN can remove it
			// and this is checked in the calling call (API_WorkflowLibrary.cpp)
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = std::format(
					"delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
					"and workspaceKey is null ",
					workflowLibraryKey
				);
			else
			{
				// admin is able to remove also WorkflowLibrarys created by others
				// for this reason the condition on creatorUserKey has to be removed
				if (admin)
					sqlStatement = std::format(
						"delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
						"and workspaceKey = {} ",
						workflowLibraryKey, workspaceKey
					);
				else
					sqlStatement = std::format(
						"delete from MMS_WorkflowLibrary where workflowLibraryKey = {} "
						"and creatorUserKey = {} and workspaceKey = {} ",
						workspaceKey, userKey, workspaceKey
					);
			}
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
			if (rowsUpdated == 0)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", workflowLibraryKey: " + to_string(workflowLibraryKey) +
									  ", userKey: " + to_string(userKey) + ", workspaceKey: " + to_string(workspaceKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

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

json MMSEngineDBFacade::getWorkflowsAsLibraryList(int64_t workspaceKey)
{
	json workflowsLibraryRoot;
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

		_logger->info(__FILEREF__ + "getWorkflowsAsLibraryList" + ", workspaceKey: " + to_string(workspaceKey));

		{
			json requestParametersRoot;

			field = "requestParameters";
			workflowsLibraryRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where (workspaceKey = {} or workspaceKey is null) ", workspaceKey);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_WorkflowLibrary {}", sqlWhere);
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

		json workflowsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select workspaceKey, workflowLibraryKey, creatorUserKey, label, "
				"thumbnailMediaItemKey, jsonWorkflow from MMS_WorkflowLibrary {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json workflowLibraryRoot;

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
					workflowLibraryRoot[field] = nullptr;
				else
					workflowLibraryRoot[field] = row["thumbnailMediaItemKey"].as<int64_t>();

				{
					string jsonWorkflow = row["jsonWorkflow"].as<string>();

					json workflowRoot;
					try
					{
						workflowRoot = JSONUtils::toJson<json>(jsonWorkflow);
					}
					catch (runtime_error &e)
					{
						_logger->error(__FILEREF__ + e.what());

						continue;
					}

					field = "variables";
					if (!JSONUtils::isPresent(workflowRoot, field))
						workflowLibraryRoot["variables"] = nullptr;
					else
						workflowLibraryRoot["variables"] = workflowRoot[field];
				}

				workflowsRoot.push_back(workflowLibraryRoot);
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

		field = "workflowsLibrary";
		responseRoot[field] = workflowsRoot;

		field = "response";
		workflowsLibraryRoot[field] = responseRoot;
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

	return workflowsLibraryRoot;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent(int64_t workspaceKey, int64_t workflowLibraryKey)
{
	string workflowLibraryContent;
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

		_logger->info(
			__FILEREF__ + "getWorkflowAsLibraryContent" + ", workspaceKey: " + to_string(workspaceKey) +
			", workflowLibraryKey: " + to_string(workflowLibraryKey)
		);

		{
			string sqlStatement = std::format(
				"select jsonWorkflow from MMS_WorkflowLibrary "
				"where (workspaceKey = {} or workspaceKey is null) "
				"and workflowLibraryKey = {}",
				workspaceKey, workflowLibraryKey
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
				string errorMessage = __FILEREF__ + "WorkflowLibrary was not found" + ", workspaceKey: " + to_string(workspaceKey) +
									  ", workflowLibraryKey: " + to_string(workflowLibraryKey);

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			workflowLibraryContent = res[0]["jsonWorkflow"].as<string>();
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

	return workflowLibraryContent;
}

string MMSEngineDBFacade::getWorkflowAsLibraryContent(int64_t workspaceKey, string label)
{
	string workflowLibraryContent;
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

		_logger->info(__FILEREF__ + "getWorkflowAsLibraryContent" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label);

		{
			string sqlStatement;
			if (workspaceKey == -1)
				sqlStatement = std::format(
					"select jsonWorkflow from MMS_WorkflowLibrary "
					"where workspaceKey is null and label = {}",
					trans.transaction->quote(label)
				);
			else
				sqlStatement = std::format(
					"select jsonWorkflow from MMS_WorkflowLibrary "
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
				string errorMessage =
					__FILEREF__ + "WorkflowLibrary was not found" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			workflowLibraryContent = res[0]["jsonWorkflow"].as<string>();
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

	return workflowLibraryContent;
}
