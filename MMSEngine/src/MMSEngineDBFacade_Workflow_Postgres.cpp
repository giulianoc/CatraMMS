
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "StringUtils.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"
#include <ranges>

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

/*
shared_ptr<PostgresConnection> MMSEngineDBFacade::beginWorkflow()
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();

	return conn;
}
*/

void MMSEngineDBFacade::endWorkflow(PostgresConnTrans &trans, bool commit, int64_t ingestionRootKey, string processedMetadataContent)
{
	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	try
	{
		_logger->info(
			__FILEREF__ + "endWorkflow" + ", getConnectionId: " + to_string(trans.connection->getConnectionId()) + ", commit: " + to_string(commit)
		);

		if (ingestionRootKey != -1)
		{
			string sqlStatement = std::format(
				"update MMS_IngestionRoot set processedMetaDataContent = {} "
				"where ingestionRootKey = {} ",
				trans.transaction->quote(processedMetadataContent), ingestionRootKey
			);
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
				string errorMessage = __FILEREF__ + "no update was done" + ", processedMetadataContent: " + processedMetadataContent +
									  ", ingestionRootKey: " + to_string(ingestionRootKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!commit)
			trans.setAbort();
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

int64_t MMSEngineDBFacade::addWorkflow(
	PostgresConnTrans &trans, int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel, bool rootHidden,
	const string_view& metaDataContent
)
{
	int64_t ingestionRootKey;

	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, userKey, type, label, hidden, "
				"metaDataContent, ingestionDate, lastUpdate, status) "
				"values (                       DEFAULT,          {},           {},      {},   {},     {}, "
				"{},               NOW() at time zone 'utc',         NOW() at time zone 'utc',      {}) returning ingestionRootKey",
				workspaceKey, userKey, trans.transaction->quote(rootType), trans.transaction->quote(rootLabel), rootHidden,
				trans.transaction->quote(metaDataContent), trans.transaction->quote(toString(MMSEngineDBFacade::IngestionRootStatus::NotCompleted))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			ingestionRootKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return ingestionRootKey;
}

pair<int64_t, string> MMSEngineDBFacade::workflowQuery_WorkspaceKeyIngestionDate(int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionroot:.workspaceKey", "mms_ingestionroot:.ingestionDate"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = workflowQuery(requestedColumns, -1, ingestionRootKey, fromMaster);

		return make_pair(
			(*sqlResultSet)[0][0].as<int64_t>((int64_t)(-1)),
			(*sqlResultSet)[0][2].as<string>(string()) // iso format of the date
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			ingestionRootKey, fromMaster, e.what()
		);

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", ingestionRootKey: {}"
			", fromMaster: {}",
			ingestionRootKey, fromMaster
		);

		throw;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::workflowQuery(
	vector<string> &requestedColumns,
	// 2021-02-20: workspaceKey is used just to be sure the ingestionJobKey
	//	will belong to the specified workspaceKey. We do that because the updateIngestionJob API
	//	calls this method, to be sure an end user can do an update of any IngestionJob (also
	//	belonging to another workspace)
	int64_t workspaceKey, int64_t ingestionRootKey, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
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
			if (workspaceKey != -1) // in uno scenario, dato il ingestionRootKey si richiede il workspaceKey (quindi potremmo non avere workspaceKey)
				where += std::format("{} workspaceKey = {} ", where.size() > 0 ? "and" : "", workspaceKey);
			if (ingestionRootKey != -1)
				where += std::format("{} ingestionRootKey = {} ", where.size() > 0 ? "and" : "", ingestionRootKey);

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
				"from MMS_IngestionRoot "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && ingestionRootKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"ingestionRoot not found"
					", workspaceKey: {}"
					", ingestionRootKey: {}",
					workspaceKey, ingestionRootKey
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
		DBRecordNotFound const *de = dynamic_cast<DBRecordNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (de != nullptr)
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
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

json MMSEngineDBFacade::getIngestionRootsStatus(
	shared_ptr<Workspace> workspace, int64_t ingestionRootKey, int64_t mediaItemKey, int start, int rows,
	// bool startAndEndIngestionDatePresent,
	string startIngestionDate, string endIngestionDate, string label, string status, bool asc, bool dependencyInfo, bool ingestionJobOutputs,
	bool hiddenToo, bool fromMaster
)
{
	json statusListRoot;

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		string field;

		{
			json requestParametersRoot;

			field = "start";
			requestParametersRoot[field] = start;

			field = "rows";
			requestParametersRoot[field] = rows;

			if (ingestionRootKey != -1)
			{
				field = "ingestionRootKey";
				requestParametersRoot[field] = ingestionRootKey;
			}

			if (mediaItemKey != -1)
			{
				field = "mediaItemKey";
				requestParametersRoot[field] = mediaItemKey;
			}

			if (startIngestionDate != "")
			{
				field = "startIngestionDate";
				requestParametersRoot[field] = startIngestionDate;
			}
			if (endIngestionDate != "")
			{
				field = "endIngestionDate";
				requestParametersRoot[field] = endIngestionDate;
			}

			field = "label";
			requestParametersRoot[field] = label;

			field = "ingestionJobOutputs";
			requestParametersRoot[field] = ingestionJobOutputs;

			field = "status";
			requestParametersRoot[field] = status;

			field = "hiddenToo";
			requestParametersRoot[field] = hiddenToo;

			field = "requestParameters";
			statusListRoot[field] = requestParametersRoot;
		}

		vector<int64_t> ingestionRookKeys;
		{
			if (mediaItemKey != -1)
			{
				string sqlStatement = std::format(
					"select distinct ir.ingestionRootKey "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_IngestionJobOutput ijo "
					"where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ijo.ingestionJobKey "
					"and ir.workspaceKey = {} and ijo.mediaItemKey = {} ",
					workspace->_workspaceKey, mediaItemKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
				for (auto row : res)
					ingestionRookKeys.push_back(row["ingestionRootKey"].as<int64_t>());
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

			if (ingestionRootKey != -1)
				ingestionRookKeys.push_back(ingestionRootKey);
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspace->_workspaceKey);
		if (ingestionRookKeys.size() > 0)
		{
			string ingestionRootKeysWhere = accumulate(
				begin(ingestionRookKeys), end(ingestionRookKeys), string(), [](const string &s, int64_t localIngestionRootKey)
				{ return (s == "" ? std::format("{}", localIngestionRootKey) : (s + std::format(", {}", localIngestionRootKey))); }
			);

			if (ingestionRookKeys.size() == 1)
				sqlWhere += std::format("and ingestionRootKey = {} ", ingestionRootKeysWhere);
			else
				sqlWhere += std::format("and ingestionRootKey in ({}) ", ingestionRootKeysWhere);
		}
		if (startIngestionDate != "")
			sqlWhere +=
				std::format("and ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startIngestionDate));
		if (endIngestionDate != "")
			sqlWhere +=
				std::format("and ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endIngestionDate));
		if (label != "")
			sqlWhere += std::format("and LOWER(label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
		if (!hiddenToo)
			sqlWhere += std::format("and hidden = {} ", false);
		{
			string allStatus = "all";
			// compare case insensitive
			if (!StringUtils::equalCaseInsensitive(status, allStatus))
				sqlWhere += std::format("and status = {} ", trans.transaction->quote(status));
		}

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_IngestionRoot {}", sqlWhere);
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
				"select ingestionRootKey, userKey, label, status, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as formattedIngestionDate, "
				"to_char(lastUpdate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as lastUpdate "
				"from MMS_IngestionRoot {} "
				"order by ingestionDate {} "
				"limit {} offset {}",
				sqlWhere, asc ? "asc" : "desc", rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			chrono::milliseconds internalSqlDuration(0);
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json workflowRoot;

				int64_t currentIngestionRootKey = row["ingestionRootKey"].as<int64_t>();
				field = "ingestionRootKey";
				workflowRoot[field] = currentIngestionRootKey;

				int64_t userKey = row["userKey"].as<int64_t>();
				field = "userKey";
				workflowRoot[field] = userKey;

				{
					chrono::milliseconds sqlDuration(0);
					pair<string, string> userDetails = getUserDetails(userKey, &sqlDuration);
					internalSqlDuration += sqlDuration;

					string userName;

					tie(ignore, userName) = userDetails;

					field = "userName";
					workflowRoot[field] = userName;
				}

				field = "label";
				workflowRoot[field] = row["label"].as<string>();

				field = "status";
				workflowRoot[field] = row["status"].as<string>();

				field = "ingestionDate";
				workflowRoot[field] = row["formattedIngestionDate"].as<string>();

				field = "lastUpdate";
				workflowRoot[field] = row["lastUpdate"].as<string>();

				json ingestionJobsRoot = json::array();
				{
					string sqlStatement = std::format(
						"select ingestionRootKey, ingestionJobKey, label, "
						"ingestionType, metaDataContent, processorMMS, "
						"to_char(processingStartingFrom, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as processingStartingFrom, "
						"to_char(startProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as startProcessing, "
						"to_char(endProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as endProcessing, "
						"case when startProcessing IS NULL then NOW() at time zone 'utc' else startProcessing end as newStartProcessing, "
						"case when endProcessing IS NULL then NOW() at time zone 'utc' else endProcessing end as newEndProcessing, "
						"downloadingProgress, uploadingProgress, "
						"status, errorMessages from MMS_IngestionJob where ingestionRootKey = {} "
						"order by ingestionJobKey asc",
						currentIngestionRootKey
					);
					// "order by newStartProcessing asc, newEndProcessing asc";
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
					for (auto& sqlRow : *sqlResultSet)
					{
						json ingestionJobRoot = getIngestionJobRoot(workspace, sqlRow, dependencyInfo, ingestionJobOutputs, trans);

						ingestionJobsRoot.push_back(ingestionJobRoot);
					}
					chrono::milliseconds sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
					internalSqlDuration += sqlDuration;
					_logger->info(
						__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement +
						", currentIngestionRootKey: " + to_string(currentIngestionRootKey) + ", res.size: " + to_string(res.size()) +
						", elapsed (millisecs): @" + to_string(sqlDuration.count()) + "@"
					);
				}

				field = "ingestionJobs";
				workflowRoot[field] = ingestionJobsRoot;

				workflowsRoot.push_back(workflowRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count();
			SQLQUERYLOG(
				"getIngestionRootsStatus", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@getIngestionRootsStatus@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		field = "workflows";
		responseRoot[field] = workflowsRoot;

		field = "response";
		statusListRoot[field] = responseRoot;
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

	return statusListRoot;
}
