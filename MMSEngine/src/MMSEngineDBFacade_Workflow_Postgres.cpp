
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/spdlog.h"
#include <ranges>

shared_ptr<PostgresConnection> MMSEngineDBFacade::beginWorkflow()
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();

	return conn;
}

void MMSEngineDBFacade::endWorkflow(
	shared_ptr<PostgresConnection> conn, work &trans, bool commit, int64_t ingestionRootKey, string processedMetadataContent
)
{
	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	try
	{
		_logger->info(__FILEREF__ + "endWorkflow" + ", getConnectionId: " + to_string(conn->getConnectionId()) + ", commit: " + to_string(commit));

		if (ingestionRootKey != -1)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionRoot set processedMetaDataContent = {} "
				"where ingestionRootKey = {} returning 1) select count(*) from rows",
				trans.quote(processedMetadataContent), ingestionRootKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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

		if (commit)
			trans.commit();
		else
			trans.abort();

		connectionPool->unborrow(conn);
		conn = nullptr;
	}
	catch (sql_error const &e)
	{
		SPDLOG_ERROR(
			"SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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

int64_t MMSEngineDBFacade::addWorkflow(
	shared_ptr<PostgresConnection> conn, work &trans, int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel, string metaDataContent
)
{
	int64_t ingestionRootKey;

	try
	{
		{
			string sqlStatement = fmt::format(
				"insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, userKey, type, label, "
				"metaDataContent, ingestionDate, lastUpdate, status) "
				"values (                       DEFAULT,          {},            {},       {},    {}, "
				"{},               NOW() at time zone 'utc',         NOW() at time zone 'utc',      {}) returning ingestionRootKey",
				workspaceKey, userKey, trans.quote(rootType), trans.quote(rootLabel), trans.quote(metaDataContent),
				trans.quote(toString(MMSEngineDBFacade::IngestionRootStatus::NotCompleted))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			ingestionRootKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}
	}
	catch (sql_error const &e)
	{
		SPDLOG_ERROR(
			"SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}

	return ingestionRootKey;
}

pair<int64_t, string> MMSEngineDBFacade::workflowQuery_WorkspaceKeyIngestionDate(int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		vector<pair<bool, string>> requestedColumns = {{false, "mms_ingestionroot:.workspaceKey"}, {false, "mms_ingestionroot:.ingestionDate"}};
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

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", ingestionRootKey: {}"
			", fromMaster: {}",
			ingestionRootKey, fromMaster
		);

		throw e;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::workflowQuery(
	vector<pair<bool, string>> &requestedColumns,
	// 2021-02-20: workspaceKey is used just to be sure the ingestionJobKey
	//	will belong to the specified workspaceKey. We do that because the updateIngestionJob API
	//	calls this method, to be sure an end user can do an update of any IngestionJob (also
	//	belonging to another workspace)
	int64_t workspaceKey, int64_t ingestionRootKey, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = fmt::format(
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
			string errorMessage = fmt::format(
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
				where += fmt::format("{} workspaceKey = {} ", where.size() > 0 ? "and" : "", workspaceKey);
			if (ingestionRootKey != -1)
				where += fmt::format("{} ingestionRootKey = {} ", where.size() > 0 ? "and" : "", ingestionRootKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = fmt::format("limit {} ", rows);
			if (startIndex != -1)
				offset = fmt::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = fmt::format("order by {} ", orderBy);

			string sqlStatement = fmt::format(
				"select {} "
				"from MMS_IngestionRoot "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (empty(res) && ingestionRootKey != -1 && notFoundAsException)
			{
				string errorMessage = fmt::format(
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

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return sqlResultSet;
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (sql_error const &e)
	{
		SPDLOG_ERROR(
			"SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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

json MMSEngineDBFacade::getIngestionRootsStatus(
	shared_ptr<Workspace> workspace, int64_t ingestionRootKey, int64_t mediaItemKey, int start, int rows,
	// bool startAndEndIngestionDatePresent,
	string startIngestionDate, string endIngestionDate, string label, string status, bool asc, bool dependencyInfo, bool ingestionJobOutputs,
	bool fromMaster
)
{
	json statusListRoot;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterPostgresConnectionPool;
	else
		connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

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

			field = "requestParameters";
			statusListRoot[field] = requestParametersRoot;
		}

		vector<int64_t> ingestionTookKeysByMediaItemKey;
		if (mediaItemKey != -1)
		{
			string sqlStatement = fmt::format(
				"select distinct ir.ingestionRootKey "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_IngestionJobOutput ijo "
				"where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ijo.ingestionJobKey "
				"and ir.workspaceKey = {} and ijo.mediaItemKey = {} ",
				workspace->_workspaceKey, mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
				ingestionTookKeysByMediaItemKey.push_back(row["ingestionRootKey"].as<int64_t>());
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		string sqlWhere = fmt::format("where workspaceKey = {} ", workspace->_workspaceKey);
		if (ingestionRootKey != -1 || ingestionTookKeysByMediaItemKey.size() > 0)
		{
			string ingestionRootKeysWhere;

			if (ingestionRootKey != -1)
				ingestionRootKeysWhere = to_string(ingestionRootKey);

			for (int ingestionRookKeyIndex = 0; ingestionRookKeyIndex < ingestionTookKeysByMediaItemKey.size(); ingestionRookKeyIndex++)
			{
				if (ingestionRootKeysWhere == "")
					ingestionRootKeysWhere = to_string(ingestionTookKeysByMediaItemKey[ingestionRookKeyIndex]);
				else
					ingestionRootKeysWhere += fmt::format(", {}", ingestionTookKeysByMediaItemKey[ingestionRookKeyIndex]);
			}

			sqlWhere += fmt::format("and ingestionRootKey in ({}) ", ingestionRootKeysWhere);
		}
		if (startIngestionDate != "")
			sqlWhere += fmt::format("and ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startIngestionDate));
		if (endIngestionDate != "")
			sqlWhere += fmt::format("and ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endIngestionDate));
		if (label != "")
			sqlWhere += fmt::format("and LOWER(label) like LOWER({}) ", trans.quote("%" + label + "%"));
		{
			string allStatus = "all";
			// compare case insensitive
			if (!StringUtils::equalCaseInsensitive(status, allStatus))
				sqlWhere += fmt::format("and status = {} ", trans.quote(status));
		}

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_IngestionRoot {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		json workflowsRoot = json::array();
		{
			string sqlStatement = fmt::format(
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
			result res = trans.exec(sqlStatement);
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
					pair<string, string> userDetails = getUserDetails(userKey);

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
					string sqlStatement = fmt::format(
						"select ingestionRootKey, ingestionJobKey, label, "
						"ingestionType, metaDataContent, processorMMS, "
						"to_char(processingStartingFrom, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as processingStartingFrom, "
						"to_char(startProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as startProcessing, "
						"to_char(endProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as endProcessing, "
						"case when startProcessing IS NULL then NOW() at time zone 'utc' else startProcessing end as newStartProcessing, "
						"case when endProcessing IS NULL then NOW() at time zone 'utc' else endProcessing end as newEndProcessing, "
						"downloadingProgress, uploadingProgress, "
						"status, errorMessage from MMS_IngestionJob where ingestionRootKey = {} "
						"order by ingestionJobKey asc",
						currentIngestionRootKey
					);
					// "order by newStartProcessing asc, newEndProcessing asc";
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row : res)
					{
						json ingestionJobRoot = getIngestionJobRoot(workspace, row, dependencyInfo, ingestionJobOutputs, conn, trans);

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
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@getIngestionRootsStatus@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
			);
		}

		field = "workflows";
		responseRoot[field] = workflowsRoot;

		field = "response";
		statusListRoot[field] = responseRoot;

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
	}
	catch (sql_error const &e)
	{
		SPDLOG_ERROR(
			"SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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

	return statusListRoot;
}

void MMSEngineDBFacade::retentionOfIngestionData()
{
	_logger->info(__FILEREF__ + "retentionOfIngestionData");

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
			_logger->info(__FILEREF__ + "retentionOfIngestionData. IngestionRoot");
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			// we will remove by steps to avoid error because of transaction log overflow
			int maxToBeRemoved = 100;
			int totalRowsRemoved = 0;
			bool moreRowsToBeRemoved = true;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeRemoved && currentRetriesOnError < maxRetriesOnError)
			{
				try
				{
					// 2022-01-30: we cannot remove any OLD ingestionroot/ingestionjob/encodingjob
					//			because we have the sheduled jobs (recording, proxy, ...) that
					//			can be scheduled to be run on the future
					//			For this reason I added the status condition
					// scenarios anomalous:
					//	- encoding is in a final state but ingestion is not: we already have the
					//		fixIngestionJobsHavingWrongStatus method
					//	- ingestion is in a final state but encoding is not: we already have the
					//		fixEncodingJobsHavingWrongStatus method
					string sqlStatement = fmt::format(
						"WITH rows AS (delete from MMS_IngestionRoot where ingestionRootKey in "
						"(select ingestionRootKey from MMS_IngestionRoot "
						"where ingestionDate < NOW() at time zone 'utc' - INTERVAL '{} days' "
						"and status like 'Completed%' limit {}) "
						"returning 1) select count(*) from rows",
						_ingestionWorkflowRetentionInDays, maxToBeRemoved
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					totalRowsRemoved += rowsUpdated;
					if (rowsUpdated == 0)
						moreRowsToBeRemoved = false;

					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					SPDLOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
					);

					currentRetriesOnError++;

					int secondsBetweenRetries = 15;
					_logger->info(
						__FILEREF__ + "retentionOfIngestionData. IngestionRoot failed, " + "waiting before to try again" +
						", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
						", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionRoot" + ", totalRowsRemoved: " + to_string(totalRowsRemoved) +
				", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}

		// IngestionJobs taking too time to download/move/copy/upload
		// the content are set to failed
		{
			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time "
							  "to download/move/copy/upload the content"
			);
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			long totalRowsUpdated = 0;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			bool toBeExecutedAgain = true;
			while (toBeExecutedAgain)
			{
				try
				{
					string sqlStatement = fmt::format(
						"select ingestionJobKey from MMS_IngestionJob "
						"where status in ({}, {}, {}, {}) and sourceBinaryTransferred = false "
						"and startProcessing + INTERVAL '{} hours' <= NOW() at time zone 'utc'",
						trans.quote(toString(IngestionStatus::SourceDownloadingInProgress)),
						trans.quote(toString(IngestionStatus::SourceMovingInProgress)),
						trans.quote(toString(IngestionStatus::SourceCopingInProgress)),
						trans.quote(toString(IngestionStatus::SourceUploadingInProgress)), _contentNotTransferredRetentionInHours
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row : res)
					{
						int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
						{
							IngestionStatus newIngestionStatus = IngestionStatus::End_IngestionFailure;

							string errorMessage = "Set to Failure by MMS because of timeout to download/move/copy/upload the content";
							string processorMMS;
							_logger->info(
								__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " +
								toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
							);
							updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
							totalRowsUpdated++;
						}
					}
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);

					toBeExecutedAgain = false;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					SPDLOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
					);

					currentRetriesOnError++;
					if (currentRetriesOnError >= maxRetriesOnError)
						toBeExecutedAgain = false;
					else
					{
						int secondsBetweenRetries = 15;
						_logger->info(
							__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time failed, " + "waiting before to try again" +
							", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
							", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
						);
						this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
					}
				}
			}

			_logger->info(
				__FILEREF__ +
				"retentionOfIngestionData. IngestionJobs taking too time "
				"to download/move/copy/upload the content" +
				", totalRowsUpdated: " + to_string(totalRowsUpdated) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}

		{
			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionJobs not completed " + "(state Start_TaskQueued) and too old to be considered " +
				"by MMSEngineDBFacade::getIngestionsToBeManaged"
			);
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			long totalRowsUpdated = 0;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			bool toBeExecutedAgain = true;
			while (toBeExecutedAgain)
			{
				try
				{
					// 2021-07-17: In this scenario the IngestionJobs would remain infinite time:
					string sqlStatement = fmt::format(
						"select ingestionJobKey from MMS_IngestionJob "
						"where status = {} and NOW() at time zone 'utc' > processingStartingFrom + INTERVAL '{} days'",
						trans.quote(toString(IngestionStatus::Start_TaskQueued)), _doNotManageIngestionsOlderThanDays
					);
					// "where (processorMMS is NULL or processorMMS = ?) "
					// "and status = ? and NOW() > DATE_ADD(processingStartingFrom, INTERVAL ? DAY)";
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row : res)
					{
						int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();

						string errorMessage = "Canceled by MMS because not completed and too old";

						_logger->info(
							__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_CanceledByMMS" + ", errorMessage: " + errorMessage
						);
						updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_CanceledByMMS, errorMessage);
						totalRowsUpdated++;
					}
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);

					toBeExecutedAgain = false;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					SPDLOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
					);

					currentRetriesOnError++;
					if (currentRetriesOnError >= maxRetriesOnError)
						toBeExecutedAgain = false;
					else
					{
						int secondsBetweenRetries = 15;
						_logger->info(
							__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time failed, " + "waiting before to try again" +
							", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
							", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
						);
						this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
					}
				}
			}

			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionJobs not completed " + "(state Start_TaskQueued) and too old to be considered " +
				"by MMSEngineDBFacade::getIngestionsToBeManaged" + ", totalRowsUpdated: " + to_string(totalRowsUpdated) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
	}
	catch (sql_error const &e)
	{
		SPDLOG_ERROR(
			"SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"abort failed"
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
