
#include "Convert.h"
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <chrono>
#include <spdlog/fmt/bundled/ranges.h>

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

int64_t MMSEngineDBFacade::addDeliveryServer(
	const string& label, const string& type, optional<int64_t> originDeliveryServerKey, bool external, bool enabled,
	const string& publicServerName, const string& internalServerName
)
{
	int64_t deliveryServerKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
			R"(
				insert into MMS_DeliveryServer(label, type, originDeliveryServerKey external, enabled, publicServerName,
					internalServerName) values (
					{}, {}, {}, {}, {}, {}, {}) returning deliveryServerKey)",
				trans.transaction->quote(label),
				trans.transaction->quote(type), originDeliveryServerKey ? to_string(originDeliveryServerKey) : "null",
				external, enabled, trans.transaction->quote(publicServerName),
				trans.transaction->quote(internalServerName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			deliveryServerKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return deliveryServerKey;
}

void MMSEngineDBFacade::modifyDeliveryServer(
	int64_t deliveryServerKey, const optional<string>& label, const optional<string>& type, const optional<int64_t>& originDeliveryServerKey,
	optional<bool> external, optional<bool> enabled, const optional<string>& publicServerName,
	const optional<string>& internalServerName
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (label)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("label = {}", trans.transaction->quote(*label));
				oneParameterPresent = true;
			}

			if (type)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("type = {}", trans.transaction->quote(*type));
				oneParameterPresent = true;
			}

			if (originDeliveryServerKey)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("originDeliveryServerKey = {}", *originDeliveryServerKey);
				oneParameterPresent = true;
			}

			if (external)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("external = {}", *external);
				oneParameterPresent = true;
			}

			if (enabled)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("enabled = {}", *enabled);
				oneParameterPresent = true;
			}

			if (publicServerName)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("publicServerName = {}", trans.transaction->quote(*publicServerName));
				oneParameterPresent = true;
			}

			if (internalServerName)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("internalServerName = {}", trans.transaction->quote(*internalServerName));
				oneParameterPresent = true;
			}

			if (!oneParameterPresent)
			{
				string errorMessage = std::format(
					"Wrong input, no parameters to be updated"
					", deliveryServerKey: {}"
					", oneParameterPresent: {}",
					deliveryServerKey, oneParameterPresent
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sqlStatement = std::format(
				"update MMS_DeliveryServer {} "
				"where deliveryServerKey = {} ",
				setSQL, deliveryServerKey
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
			LOG_WARN(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::updateDeliveryServerAvgBandwidthUsage(
	int64_t deliveryServerKey, uint64_t& txAvgBandwidthUsage, uint64_t& rxAvgBandwidthUsage
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_DeliveryServer set txAvgBandwidthUsage = {}, rxAvgBandwidthUsage = {}, "
				"bandwidthUsageUpdateTime = NOW() at time zone 'utc' "
				"where deliveryServerKey = {} ",
				txAvgBandwidthUsage, rxAvgBandwidthUsage, deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec0(sqlStatement);
			const int rowsUpdated = res.affected_rows();
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
				const string errorMessage = std::format("no update was done"
					", deliveryServerKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					deliveryServerKey, rowsUpdated, sqlStatement);
				LOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::updateDeliveryServerCPUUsage(
	int64_t deliveryServerKey, uint32_t& cpuUsage
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_DeliveryServer set cpuUsage = {}, "
				"cpuUsageUpdateTime = NOW() at time zone 'utc' "
				"where deliveryServerKey = {} ",
				cpuUsage, deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec0(sqlStatement);
			const int rowsUpdated = res.affected_rows();
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
				const string errorMessage = std::format("no update was done"
					", deliveryServerKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					deliveryServerKey, rowsUpdated, sqlStatement);
				LOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::removeDeliveryServer(int64_t deliveryServerKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_DeliveryServer where deliveryServerKey = {} ", deliveryServerKey);
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
					", deliveryServerKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					deliveryServerKey, rowsUpdated, sqlStatement
				);
				LOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getDeliveryServerList(
	bool admin, int start, int rows, bool allDeliveryServers, int64_t workspaceKey, int64_t deliveryServerKey, string label, string serverName,
	string labelOrder // "" or "asc" or "desc"
)
{
	json deliveryServerListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		LOG_INFO(
			"getDeliveryServerList"
			", start: {}"
			", rows: {}"
			", allDeliveryServers: {}"
			", workspaceKey: {}"
			", deliveryServerKey: {}"
			", label: {}"
			", serverName: {}"
			", labelOrder: {}",
			start, rows, allDeliveryServers, workspaceKey, deliveryServerKey, label, serverName, labelOrder
		);

		{
			json requestParametersRoot;

			if (deliveryServerKey != -1)
				requestParametersRoot["deliveryServerKey"] = deliveryServerKey;
			requestParametersRoot["start"] = start;
			requestParametersRoot["rows"] = rows;
			if (!label.empty())
				requestParametersRoot["label"] = label;
			if (!serverName.empty())
				requestParametersRoot["serverName"] = serverName;
			if (!labelOrder.empty())
				requestParametersRoot["labelOrder"] = labelOrder;
			deliveryServerListRoot["requestParameters"] = requestParametersRoot;
		}

		string sqlWhere;
		if (deliveryServerKey != -1)
		{
			if (!sqlWhere.empty())
				sqlWhere += std::format("and d.deliveryServerKey = {} ", deliveryServerKey);
			else
				sqlWhere += std::format("d.deliveryServerKey = {} ", deliveryServerKey);
		}
		if (!label.empty())
		{
			if (!sqlWhere.empty())
				sqlWhere += std::format("and LOWER(d.label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
			else
				sqlWhere += std::format("LOWER(d.label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
		}
		if (!serverName.empty())
		{
			if (!sqlWhere.empty())
				sqlWhere += std::format(
					"and (d.publicServerName like {} or d.internalServerName like {}) ", trans.transaction->quote("%" + serverName + "%"),
					trans.transaction->quote("%" + serverName + "%")
				);
			else
				sqlWhere += std::format(
					"(d.publicServerName like {} or d.internalServerName like {}) ", trans.transaction->quote(serverName),
					trans.transaction->quote(serverName)
				);
		}

		if (allDeliveryServers)
		{
			// using just MMS_Encoder
			if (!sqlWhere.empty())
				sqlWhere = std::format("where {}", sqlWhere);
		}
		else
		{
			// join with MMS_EncoderWorkspaceMapping
			if (!sqlWhere.empty())
				sqlWhere = std::format(
				   "where d.deliveryServerKey = dwm.deliveryServerKey "
				   "and dwm.workspaceKey = {} and {}",
				   workspaceKey, sqlWhere);
			else
				sqlWhere = std::format(
					"where d.deliveryServerKey = dwm.deliveryServerKey "
					"and dwm.workspaceKey = {} ",
					workspaceKey
				);
		}

		json responseRoot;
		{
			string sqlStatement;
			if (allDeliveryServers)
				sqlStatement = std::format("select count(*) from MMS_DeliveryServer d {}", sqlWhere);
			else
			{
				sqlStatement = std::format(
					"select count(*) "
					"from MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping dwm {}",
					sqlWhere
				);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			responseRoot["numFound"] = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		json deliveryServersRoot = json::array();
		{
			string orderByCondition;
			if (labelOrder.empty())
				orderByCondition = "order by d.deliveryServerKey "; // aggiunto solo perchè in base a explain analyze, è piu veloce nella sua esecuzione
			else
				orderByCondition = std::format("order by label {} ", labelOrder);

			string sqlStatement;
			if (allDeliveryServers)
				sqlStatement = std::format(
				R"(
					select d.deliveryServerKey, d.label, d.type, d.originDeliveryServerKey, d.external, d.enabled,
					d.publicServerName, d.internalServerName,
					to_char(d.selectedLastTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as selectedLastTime,
					d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as cpuUsageUpdateTime,
					d.txAvgBandwidthUsage, d.rxAvgBandwidthUsage, to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as bandwidthUsageUpdateTime
					from MMS_DeliveryServer d {} {} limit {} offset {}
					)",
					sqlWhere, orderByCondition, rows, start
				);
			else
				sqlStatement = std::format(
				R"(
					select d.deliveryServerKey, d.label, d.type, d.originDeliveryServerKey, d.external, d.enabled,
					d.publicServerName, d.internalServerName,
					to_char(d.selectedLastTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as selectedLastTime,
					d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as cpuUsageUpdateTime,
					d.txAvgBandwidthUsage, d.rxAvgBandwidthUsage, to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as bandwidthUsageUpdateTime
					from MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping dwm {} {} limit {} offset {}
					)",
					sqlWhere, orderByCondition, rows, start
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
			chrono::milliseconds internalSqlDuration(0);
			for (auto& sqlRow : *sqlResultSet)
			{
				chrono::milliseconds localSqlDuration(0);
				json deliveryServerRoot = getDeliveryServerRoot(admin, sqlRow, &localSqlDuration);
				internalSqlDuration += localSqlDuration;

				deliveryServersRoot.push_back(deliveryServerRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", internalSqlDuration: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), internalSqlDuration.count(), elapsed
			);
		}

		responseRoot["deliversServers"] = deliveryServersRoot;
		deliveryServerListRoot["response"] = responseRoot;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return deliveryServerListRoot;
}

string MMSEngineDBFacade::deliveryServer_columnAsString(string columnName, int64_t deliveryServerKey, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_deliveryserver:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		const shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = deliveryServerQuery(requestedColumns, deliveryServerKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>();
	}
	catch (exception &e)
	{
		if (!dynamic_cast<DBRecordNotFound*>(&e))
			LOG_ERROR(
				"deliveryServer_columnAsString failed"
				", deliveryServerKey: {}"
				", fromMaster: {}"
				", exception: {}",
				deliveryServerKey, fromMaster, e.what()
			);

		throw;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::deliveryServerQuery(
	vector<string> &requestedColumns, int64_t deliveryServerKey, bool fromMaster,
	int startIndex, int rows, string orderBy, bool notFoundAsException,
	chrono::milliseconds *sqlDuration
)
{
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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		if ((startIndex != -1 || rows != -1) && orderBy.empty())
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
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (deliveryServerKey != -1)
				where += std::format("{} deliveryServerKey = {} ", !where.empty() ? "and" : "", deliveryServerKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (!orderBy.empty())
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_DeliveryServer "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), !where.empty() ? "where " : "", where,
				limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			sqlResultSet = PostgresHelper::buildResult(res);
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = sqlResultSet->getSqlDuration().count();
			if (sqlDuration != nullptr)
				*sqlDuration = sqlResultSet->getSqlDuration();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && deliveryServerKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"deliveryServer not found"
					", deliveryServerKey: {}",
					deliveryServerKey
				);
				// abbiamo il log nel catch
				// LOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getDeliveryServerRoot(bool admin, PostgresHelper::SqlResultSet::SqlRow &row,
	chrono::milliseconds *extraDuration)
{
	json deliveryServerRoot;

	try
	{
		const chrono::system_clock::time_point start = chrono::system_clock::now();
		if (extraDuration != nullptr)
			*extraDuration = chrono::milliseconds::zero();;

		auto deliveryServerKey = row["deliveryServerKey"].as<int64_t>();

		deliveryServerRoot["deliveryServerKey"] = deliveryServerKey;
		deliveryServerRoot["label"] = row["label"].as<string>();
		deliveryServerRoot["type"] = row["type"].as<string>();
		if (row["originDeliveryServerKey"].isNull())
			deliveryServerRoot["originDeliveryServerKey"] = nullptr;
		else
		{
			auto originDeliveryServerKey = row["originDeliveryServerKey"].as<int64_t>();
			deliveryServerRoot["originDeliveryServerKey"] = originDeliveryServerKey;
			// deliveryServerRoot["originDeliveryServerLabel"] = deliveryServer_columnAsString("label",  originDeliveryServerKey);
		}
		deliveryServerRoot["external"] = row["external"].as<bool>();
		deliveryServerRoot["enabled"] = row["enabled"].as<bool>();
		deliveryServerRoot["publicServerName"] = row["publicServerName"].as<string>();
		deliveryServerRoot["internalServerName"] = row["internalServerName"].as<string>();
		deliveryServerRoot["selectedLastTime"] = row["selectedLastTime"].as<string>();
		if (row["cpuUsage"].isNull())
			deliveryServerRoot["cpuUsage"] = nullptr;
		else
			deliveryServerRoot["cpuUsage"] = row["cpuUsage"].as<int32_t>();
		if (row["cpuUsageUpdateTime"].isNull())
			deliveryServerRoot["cpuUsageUpdateTime"] = nullptr;
		else
			deliveryServerRoot["cpuUsageUpdateTime"] = row["cpuUsageUpdateTime"].as<string>();
		if (row["txAvgBandwidthUsage"].isNull())
			deliveryServerRoot["txAvgBandwidthUsage"] = nullptr;
		else
			deliveryServerRoot["txAvgBandwidthUsage"] = row["txAvgBandwidthUsage"].as<int64_t>();
		deliveryServerRoot["rxAvgBandwidthUsage"] = row["rxAvgBandwidthUsage"].as<int64_t>();
		if (row["bandwidthUsageUpdateTime"].isNull())
			deliveryServerRoot["bandwidthUsageUpdateTime"] = nullptr;
		else
			deliveryServerRoot["bandwidthUsageUpdateTime"] = row["bandwidthUsageUpdateTime"].as<string>();

		if (admin)
		{
			chrono::milliseconds localDuration(0);
			deliveryServerRoot["workspacesAssociated"] = getDeliveryServerWorkspacesAssociation(deliveryServerKey, &localDuration);
			if (extraDuration != nullptr)
				*extraDuration += localDuration;
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}",
				se->query(), se->what()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}",
				e.what()
			);

		throw;
	}

	return deliveryServerRoot;
}

void MMSEngineDBFacade::addAssociationWorkspaceDeliveryServer(int64_t workspaceKey, int64_t deliveryServerKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		addAssociationWorkspaceDeliveryServer(workspaceKey, deliveryServerKey, trans);
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::addAssociationWorkspaceDeliveryServer(int64_t workspaceKey, int64_t deliveryServerKey, PostgresConnTrans &trans)
{
	LOG_INFO(
		"Received addAssociationWorkspaceDeliveryServer"
		", workspaceKey: {}"
		", deliveryServerKey: {}",
		workspaceKey, deliveryServerKey
	);

	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_DeliveryServerWorkspaceMapping (workspaceKey, deliveryServerKey) "
				"values ({}, {})",
				workspaceKey, deliveryServerKey
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		throw;
	}
}

void MMSEngineDBFacade::removeAssociationWorkspaceDeliveryServer(int64_t workspaceKey, int64_t deliveryServerKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// se il deliveryServer che vogliamo rimuovere da un workspace è all'interno di qualche DeliveryServersPool,
		// bisogna rimuoverlo
		{
			string sqlStatement = std::format(
				"delete from MMS_DeliveryServerDeliveryServersPoolMapping "
				"where deliveryServersPoolKey in (select deliveryServersPoolKey from MMS_DeliveryServersPool where workspaceKey = {}) "
				"and deliveryServerKey = {} ",
				workspaceKey, deliveryServerKey
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
				"delete from MMS_DeliveryServerWorkspaceMapping "
				"where workspaceKey = {} and deliveryServerKey = {} ",
				workspaceKey, deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			const result res = trans.transaction->exec(sqlStatement);
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
					", deliveryServerKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					deliveryServerKey, rowsUpdated, sqlStatement
				);
				LOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getDeliveryServerWorkspacesAssociation(int64_t deliveryServerKey, chrono::milliseconds *sqlDuration)
{
	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		json deliveryServerWorkspacesAssociatedRoot = json::array();
		{
			string sqlStatement = std::format(
				"select w.workspaceKey, w.name "
				"from MMS_Workspace w, MMS_DeliveryServerWorkspaceMapping dwm "
				"where w.workspaceKey = dwm.workspaceKey and dwm.deliveryServerKey = {}",
				deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json deliveryServerWorkspaceAssociatedRoot;

				deliveryServerWorkspaceAssociatedRoot["workspaceKey"] = row["workspaceKey"].as<int64_t>();
				deliveryServerWorkspaceAssociatedRoot["workspaceName"] = row["name"].as<string>();

				deliveryServerWorkspacesAssociatedRoot.push_back(deliveryServerWorkspaceAssociatedRoot);
			}
			if (sqlDuration != nullptr)
				*sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
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

		return deliveryServerWorkspacesAssociatedRoot;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

json MMSEngineDBFacade::getDeliveryServersPoolList(
	int start, int rows, int64_t workspaceKey, int64_t deliveryServersPoolKey, string label,
	string labelOrder // "" or "asc" or "desc"
)
{
	json deliveryServersPoolListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		LOG_INFO(
			"getDeliveryServersPoolList"
			", start: {}"
			", rows: {}"
			", workspaceKey: {}"
			", deliveryServersPoolKey: {}"
			", label: {}"
			", labelOrder: {}",
			start, rows, workspaceKey, deliveryServersPoolKey, label, labelOrder
		);

		{
			json requestParametersRoot;

			if (deliveryServersPoolKey != -1)
				requestParametersRoot["deliveryServersPoolKey"] = deliveryServersPoolKey;

			requestParametersRoot["start"] = start;
			requestParametersRoot["rows"] = rows;

			if (!label.empty())
				requestParametersRoot["label"] = label;

			if (!labelOrder.empty())
				requestParametersRoot["labelOrder"] = labelOrder;

			deliveryServersPoolListRoot["requestParameters"] = requestParametersRoot;
		}

		// label == NULL is the "internal" EncodersPool representing the default encoders pool
		// for a workspace, the one using all the internal encoders associated to the workspace
		string sqlWhere = std::format("where workspaceKey = {} and label is not NULL ", workspaceKey);
		if (deliveryServersPoolKey != -1)
			sqlWhere += std::format("and deliveryServersPoolKey = {} ", deliveryServersPoolKey);
		if (!label.empty())
			sqlWhere += std::format("and LOWER(label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_DeliveryServersPool {}", sqlWhere);
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

		json deliveryServersPoolsRoot = json::array();
		{
			string orderByCondition;
			if (!labelOrder.empty())
				orderByCondition = "order by label " + labelOrder + " ";

			string sqlStatement =
				std::format("select deliveryServersPoolKey, label from MMS_DeliveryServersPool {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			auto sqlResultSet  = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
			for (auto row : *sqlResultSet)
			{
				json deliveryServersPoolRoot;

				auto deliveryServersPoolKey = row["deliveryServersPoolKey"].as<int64_t>();

				deliveryServersPoolRoot["deliveryServersPoolKey"] = deliveryServersPoolKey;
				deliveryServersPoolRoot["label"] = row["label"].as<string>();

				json deliveryServersRoot = json::array();
				{
					string sqlStatement = std::format(
						"select deliveryServerKey from MMS_DeliveryServerDeliveryServersPoolMapping "
						"where deliveryServersPoolKey = {}",
						deliveryServersPoolKey
					);
					// chrono::system_clock::time_point startSql = chrono::system_clock::now();
					auto sqlResultSet = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
					for (auto& sqlRow : *sqlResultSet)
					{
						auto deliveryServerKey = sqlRow["deliveryServerKey"].as<int64_t>();

						{
							string sqlStatement = std::format(
								"select deliveryServerKey, label, type, originDeliveryServerKey, external, enabled, "
								"publicServerName, internalServerName, "
								"to_char(selectedLastTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as selectedLastTime, "
								"cpuUsage, to_char(cpuUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as cpuUsageUpdateTime, "
								"txAvgBandwidthUsage, rxAvgBandwidthUsage, to_char(bandwidthUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as bandwidthUsageUpdateTime "
								"from MMS_DeliveryServer "
								"where deliveryServerKey = {} ",
								deliveryServerKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							auto sqlResultSet = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
							long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
							SQLQUERYLOG(
								"default", elapsed,
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, trans.connection->getConnectionId(), elapsed
							);
							if (!sqlResultSet->empty())
							{
								bool admin = false;
								auto& row1 = (*sqlResultSet)[0];

								deliveryServersRoot.push_back(getDeliveryServerRoot(admin, row1));
							}
							else
							{
								string errorMessage = std::format(
									"No deliveryServerKey found"
									", deliveryServerKey: {}",
									deliveryServerKey
								);
								LOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}
				}

				deliveryServersPoolRoot["deliveryServers"] = deliveryServersRoot;

				deliveryServersPoolsRoot.push_back(deliveryServersPoolRoot);
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

		responseRoot["deliveryServersPool"] = deliveryServersPoolsRoot;

		deliveryServersPoolListRoot["response"] = responseRoot;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return deliveryServersPoolListRoot;
}

int64_t MMSEngineDBFacade::addDeliveryServersPool(int64_t workspaceKey, const string& label, vector<int64_t> &deliveryServerKeys)
{
	int64_t deliveryServersPoolKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// check: every encoderKey shall be already associated to the workspace
		for (int64_t deliveryServerKey : deliveryServerKeys)
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_DeliveryServerWorkspaceMapping "
				"where workspaceKey = {} and deliveryServerKey = {} ",
				workspaceKey, deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			auto count = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
					"DeliveryServer is not already associated to the workspace"
					", workspaceKey: {}"
					", deliveryServerKey: {}",
					workspaceKey, deliveryServerKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"insert into MMS_DeliveryServersPool(workspaceKey, label) values ( "
				"{}, {}) returning deliveryServersPoolKey",
				workspaceKey, trans.transaction->quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			deliveryServersPoolKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		for (int64_t deliveryServerKey : deliveryServerKeys)
		{
			string sqlStatement = std::format(
				"insert into MMS_DeliveryServerDeliveryServersPoolMapping(deliveryServersPoolKey, "
				"deliveryServerKey) values ( "
				"{}, {})",
				deliveryServersPoolKey, deliveryServerKey
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return deliveryServersPoolKey;
}

int64_t MMSEngineDBFacade::modifyDeliveryServersPool(int64_t deliveryServersPoolKey, int64_t workspaceKey, string newLabel,
	vector<int64_t> &newDeliveryServerKeys)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		LOG_INFO(
			"Received modifyDeliveryServersPool"
			", deliveryServersPoolKey: {}"
			", workspaceKey: {}"
			", newLabel: {}"
			", newDeliveryServerKeys.size: {}",
			deliveryServersPoolKey, workspaceKey, newLabel, newDeliveryServerKeys.size()
		);

		// check: every encoderKey shall be already associated to the workspace
		for (int64_t deliveryServerKey : newDeliveryServerKeys)
		{
			string sqlStatement = std::format(
				"select count(*) from MMS_DeliveryServerWorkspaceMapping "
				"where workspaceKey = {} and deliveryServerKey = {} ",
				workspaceKey, deliveryServerKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			auto count = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
					"DeliveryServer is not already associated to the workspace"
					", workspaceKey: {}"
					", deliveryServerKey: {}",
					workspaceKey, deliveryServerKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"select label from MMS_DeliveryServersPool "
				"where deliveryServersPoolKey = {} ",
				deliveryServersPoolKey
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
				auto savedLabel = res[0]["label"].as<string>();
				if (savedLabel != newLabel)
				{
					string sqlStatement = std::format(
						"update MMS_DeliveryServersPool "
						"set label = {} "
						"where deliveryServersPoolKey = {} ",
						trans.transaction->quote(newLabel), deliveryServersPoolKey
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
						string errorMessage = std::format(
							"no update was done"
							", newLabel: {}"
							", deliveryServersPoolKey: {}"
							", rowsUpdated: {}"
							", sqlStatement: {}",
							newLabel, deliveryServersPoolKey, rowsUpdated, sqlStatement
						);
						LOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				vector<int64_t> savedDeliveryServerKeys;
				{
					string sqlStatement = std::format(
						"select deliveryServerKey from MMS_DeliveryServerDeliveryServersPoolMapping "
						"where deliveryServersPoolKey = {}",
						deliveryServersPoolKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						auto deliveryServerKey = row["deliveryServerKey"].as<int64_t>();

						savedDeliveryServerKeys.push_back(deliveryServerKey);
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
				for (int64_t newDeliveryServerKey : newDeliveryServerKeys)
				{
					if (find(savedDeliveryServerKeys.begin(), savedDeliveryServerKeys.end(), newDeliveryServerKey) == savedDeliveryServerKeys.end())
					{
						string sqlStatement = std::format(
							"insert into MMS_DeliveryServerDeliveryServersPoolMapping("
							"deliveryServersPoolKey, deliveryServerKey) values ( "
							"{}, {})",
							deliveryServersPoolKey, newDeliveryServerKey
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
				for (int64_t savedDeliveryServerKey : savedDeliveryServerKeys)
				{
					if (find(newDeliveryServerKeys.begin(), newDeliveryServerKeys.end(), savedDeliveryServerKey) == newDeliveryServerKeys.end())
					{
						string sqlStatement = std::format(
							"delete from MMS_DeliveryServerDeliveryServersPoolMapping "
							"where deliveryServersPoolKey = {} and deliveryServerKey = {} ",
							deliveryServersPoolKey, savedDeliveryServerKey
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
							string errorMessage = std::format(
								"no delete was done"
								", deliveryServersPoolKey: {}"
								", savedDeliveryServerKey: {}"
								", rowsUpdated: {}"
								", sqlStatement: {}",
								deliveryServersPoolKey, savedDeliveryServerKey, rowsUpdated, sqlStatement
							);
							LOG_WARN(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
			else
			{
				string errorMessage = std::format(
					"No deliveryServersPool found"
					", deliveryServersPoolKey: {}",
					deliveryServersPoolKey
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}

	return deliveryServersPoolKey;
}

void MMSEngineDBFacade::removeDeliveryServersPool(int64_t deliveryServersPoolKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_DeliveryServersPool where deliveryServersPoolKey = {} ", deliveryServersPoolKey);
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
					", deliveryServersPoolKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					deliveryServersPoolKey, rowsUpdated, sqlStatement
				);
				LOG_WARN(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}
