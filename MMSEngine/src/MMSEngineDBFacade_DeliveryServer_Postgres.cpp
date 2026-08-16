
#include "CurlWrapper.h"
#include "JsonPath.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <chrono>
#include <random>

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

int64_t MMSEngineDBFacade::addDeliveryServer(
	const string& label, const string& type, const optional<int64_t> originDeliveryServerKey, bool external, bool enabled,
	const string& publicIP, const string& internalIP, const string& hostname, double latitude, double longitude,
	double maxTXBandwidthInGbps
)
{
	int64_t deliveryServerKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(R"(
				insert into MMS_DeliveryServer(label, type, originDeliveryServerKey, external, enabled, publicIP,
					internalIP, hostname, latitude, longitude, maxTXBandwidthInGbps) values (
					{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) returning deliveryServerKey)",
				trans.transaction->quote(label),
				trans.transaction->quote(type), originDeliveryServerKey ? to_string(originDeliveryServerKey) : "null",
				external, enabled, trans.transaction->quote(publicIP), trans.transaction->quote(internalIP),
				trans.transaction->quote(hostname), latitude, longitude, maxTXBandwidthInGbps
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
				StringUtils::normalizeWhitespace(sqlStatement, true), trans.connection->getConnectionId(), elapsed
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
	optional<bool> external, optional<bool> enabled, const optional<string>& publicIP, const optional<string>& internalIP,
	const optional<string>& hostname, const optional<double>& latitude, const optional<double>& longitude,
	const optional<double>& maxTXBandwidthInGbps
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// modify lo lasciamo "libero" di modificare, eventualmente il DB indica se ci sono errori

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

			if (publicIP)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("publicIP = {}", trans.transaction->quote(*publicIP));
				oneParameterPresent = true;
			}

			if (internalIP)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("internalIP = {}", trans.transaction->quote(*internalIP));
				oneParameterPresent = true;
			}

			if (hostname)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("hostname = {}", trans.transaction->quote(*hostname));
				oneParameterPresent = true;
			}

			if (latitude)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("latitude = {}", *latitude);
				oneParameterPresent = true;
			}

			if (longitude)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("longitude = {}", *longitude);
				oneParameterPresent = true;
			}

			if (maxTXBandwidthInGbps)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += std::format("maxTXBandwidthInGbps = {}", *maxTXBandwidthInGbps);
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
	const int64_t deliveryServerKey,
	const uint64_t& rxAvgBandwidthUsage, const uint64_t& txAvgBandwidthUsage,
	const uint64_t& rxPeakBandwidthUsage, const uint64_t& txPeakBandwidthUsage
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format( R"(
				update MMS_DeliveryServer
				set rxAvgBandwidthUsage = {}, txAvgBandwidthUsage = {},
				rxPeakBandwidthUsage = {}, txPeakBandwidthUsage = {},
				bandwidthUsageUpdateTime = NOW() at time zone 'utc'
				where deliveryServerKey = {}
				)",
				rxAvgBandwidthUsage, txAvgBandwidthUsage, rxPeakBandwidthUsage, txPeakBandwidthUsage, deliveryServerKey
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
	int64_t deliveryServerKey, uint16_t& cpuUsage
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
	bool admin, int start, int rows, bool allDeliveryServers, int64_t workspaceKey, optional<int64_t> deliveryServerKey,
	optional<string> label, optional<string> serverIP, optional<string> hostname, optional<string> type,
	optional<string> labelOrder // "" or "asc" or "desc"
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
			", serverIP: {}"
			", hostname: {}"
			", type: {}"
			", labelOrder: {}",
			start, rows, allDeliveryServers, workspaceKey, deliveryServerKey ? *deliveryServerKey : -1,
			label ? *label : "", serverIP ? *serverIP : "", hostname ? *hostname : "", type ? *type : "",
			labelOrder ? *labelOrder : ""
		);

		{
			json requestParametersRoot;

			if (deliveryServerKey)
				requestParametersRoot["deliveryServerKey"] = *deliveryServerKey;
			requestParametersRoot["start"] = start;
			requestParametersRoot["rows"] = rows;
			if (label && !(*label).empty())
				requestParametersRoot["label"] = *label;
			if (serverIP && !(*serverIP).empty())
				requestParametersRoot["serverIP"] = *serverIP;
			if (hostname && !(*hostname).empty())
				requestParametersRoot["hostname"] = *hostname;
			if (type && !(*type).empty())
				requestParametersRoot["type"] = *type;
			if (labelOrder && !(*labelOrder).empty())
				requestParametersRoot["labelOrder"] = *labelOrder;
			deliveryServerListRoot["requestParameters"] = requestParametersRoot;
		}

		string sqlWhere;
		if (deliveryServerKey)
			sqlWhere += std::format("{} d.deliveryServerKey = {} ",
				sqlWhere.empty() ? "" : "AND", *deliveryServerKey);
		if (label && !(*label).empty())
			sqlWhere += std::format("{} LOWER(d.label) like LOWER({}) ",
				sqlWhere.empty() ? "" : "AND", trans.transaction->quote("%" + *label + "%"));
		if (serverIP && !(*serverIP).empty())
			sqlWhere += std::format(
				"{} (d.publicIP like {} or d.internalIP like {}) ",
				sqlWhere.empty() ? "" : "AND", trans.transaction->quote("%" + *serverIP + "%"),
				trans.transaction->quote("%" + *serverIP + "%")
			);
		if (hostname && !(*hostname).empty())
			sqlWhere += std::format("{} LOWER(d.hostname) like LOWER({}) ",
				sqlWhere.empty() ? "" : "AND", trans.transaction->quote("%" + *hostname + "%"));
		if (type && !(*type).empty())
			sqlWhere += std::format("{} d.type = {} ", sqlWhere.empty() ? "" : "AND", trans.transaction->quote(*type));

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
			if (labelOrder && !(*labelOrder).empty())
				orderByCondition = std::format("order by label {} ", *labelOrder);
			else
				orderByCondition = "order by d.deliveryServerKey "; // aggiunto solo perchè in base a explain analyze, è piu veloce nella sua esecuzione

			string sqlStatement;
			if (allDeliveryServers)
				sqlStatement = std::format(R"(
					select d.deliveryServerKey, d.label, d.type, d.originDeliveryServerKey, d.external, d.enabled,
					d.publicIP, d.internalIP, d.hostname, d.latitude, d.longitude, d.maxTXBandwidthInGbps,
					to_char(d.selectedLastTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as selectedLastTime,
					d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as cpuUsageUpdateTime,
					d.rxAvgBandwidthUsage, d.txAvgBandwidthUsage, d.rxPeakBandwidthUsage, d.txPeakBandwidthUsage,
					to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as bandwidthUsageUpdateTime
					from MMS_DeliveryServer d {} {} limit {} offset {}
					)",
					sqlWhere, orderByCondition, rows, start
				);
			else
				sqlStatement = std::format(R"(
					select d.deliveryServerKey, d.label, d.type, d.originDeliveryServerKey, d.external, d.enabled,
					d.publicIP, d.internalIP, d.hostname, d.latitude, d.longitude, d.maxTXBandwidthInGbps,
					to_char(d.selectedLastTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as selectedLastTime,
					d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as cpuUsageUpdateTime,
					d.rxAvgBandwidthUsage, d.txAvgBandwidthUsage, d.rxPeakBandwidthUsage, d.txPeakBandwidthUsage,
					to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD"T"HH24:MI:SS"Z"') as bandwidthUsageUpdateTime
					from MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping dwm {} {} limit {} offset {}
					)",
					sqlWhere, orderByCondition, rows, start
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
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
				StringUtils::normalizeWhitespace(sqlStatement, true), trans.connection->getConnectionId(), internalSqlDuration.count(), elapsed
			);
		}

		responseRoot["deliveryServers"] = deliveryServersRoot;
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
		DeliveryServerListParams deliveryServerListParams {
			.requestedColumns = vector<string>(1, std::format("mms_deliveryserver:.{}", columnName)),
			.deliveryServerKey = deliveryServerKey,
			.enabled = nullopt,
			.fromMaster = fromMaster
		};
		const shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = deliveryServerQuery(deliveryServerListParams);

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

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::deliveryServerQuery(DeliveryServerListParams& deliveryServerListParams)
{
	PostgresConnTrans trans(deliveryServerListParams.fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (deliveryServerListParams.rows && *deliveryServerListParams.rows > _maxRows)
		{
			string errorMessage = std::format(
				"Too many rows requested"
				", rows: {}"
				", maxRows: {}",
				*deliveryServerListParams.rows, _maxRows
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		if ((deliveryServerListParams.start || deliveryServerListParams.rows) && deliveryServerListParams.orderBy.empty())
		{
			// The query optimizer takes LIMIT into account when generating query plans, so you are very likely to get different plans (yielding
			// different row orders) depending on what you give for LIMIT and OFFSET. Thus, using different LIMIT/OFFSET values to select different
			// subsets of a query result will give inconsistent results unless you enforce a predictable result ordering with ORDER BY. This is not a
			// bug; it is an inherent consequence of the fact that SQL does not promise to deliver the results of a query in any particular order
			// unless ORDER BY is used to constrain the order. The rows skipped by an OFFSET clause still have to be computed inside the server;
			// therefore a large OFFSET might be inefficient.
			string errorMessage = std::format(
				"Using start/rows without orderBy will give inconsistent results"
				", start: {}"
				", rows: {}"
				", orderBy: {}",
				deliveryServerListParams.start.value_or(-1), deliveryServerListParams.rows.value_or(-1), deliveryServerListParams.orderBy
			);
			LOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (deliveryServerListParams.workspaceKey)
			{
				// join with MMS_EncoderWorkspaceMapping
				where = std::format(
					"d.deliveryServerKey = dwm.deliveryServerKey "
					"and dwm.workspaceKey = {} ",
					*deliveryServerListParams.workspaceKey
				);
			}

			if (deliveryServerListParams.deliveryServerKey)
				where += std::format("{} d.deliveryServerKey = {} ", !where.empty() ? "and" : "",
					*deliveryServerListParams.deliveryServerKey);
			if (deliveryServerListParams.enabled)
				where += std::format("{} d.enabled = {} ", !where.empty() ? "and" : "",
					*deliveryServerListParams.enabled);
			if (deliveryServerListParams.external)
				where += std::format("{} d.external = {} ", !where.empty() ? "and" : "",
					*deliveryServerListParams.external);
			if (!deliveryServerListParams.type.empty())
				where += std::format("{} d.type = {} ", !where.empty() ? "and" : "",
					trans.transaction->quote(deliveryServerListParams.type));

			string limit;
			string offset;
			string orderByCondition;
			if (deliveryServerListParams.rows)
				limit = std::format("limit {} ", *deliveryServerListParams.rows);
			if (deliveryServerListParams.start)
				offset = std::format("offset {} ", *deliveryServerListParams.start);
			if (!deliveryServerListParams.orderBy.empty())
				orderByCondition = std::format("order by {} ", deliveryServerListParams.orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from {} "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(deliveryServerListParams.requestedColumns),
				deliveryServerListParams.workspaceKey ? "MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping dwm "
					: "MMS_DeliveryServer d ",
				!where.empty() ? "where " : "", where,
				limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			sqlResultSet = PostgresHelper::buildResult(res);
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = sqlResultSet->getSqlDuration().count();
			if (deliveryServerListParams.sqlDuration != nullptr)
				*deliveryServerListParams.sqlDuration = sqlResultSet->getSqlDuration();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && deliveryServerListParams.deliveryServerKey && deliveryServerListParams.notFoundAsException)
			{
				string errorMessage = std::format(
					"deliveryServer not found"
					", deliveryServerKey: {}",
					*deliveryServerListParams.deliveryServerKey
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

string MMSEngineDBFacade::getBestDeliveryServerBasedOnGeoProximityAndMetrics(
	const int64_t workspaceKey, const optional<bool> external, const double playerLatitude, const double playerLongitude)
{
	/*
	Geo-proximity–based server selection. Limiti di questa soluzione:
	- la Distanza geografica è diversa dalla distanza di rete
	- non viene considerato che un edge puo essere vicino al client ma lontano dal contenuto, ad esempio:
		- origin -> mid-origin -> mid-origin -> edge -> client
	- non viene considerato se l'edge deve leggere via NFS da un origin congestionato
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		{
			/*
			SELECT *, earth_distance(ll_to_earth(:client_lat, :client_lon), ll_to_earth(lat, lon)) AS distance_m
				FROM delivery_server
				WHERE enabled = true
				ORDER BY distance_m, cpuUsage, txAvgBandwidthUsage
				LIMIT 1;
			Abbiamo due opzioni:
				1) ordinare per distanza, poi per cpuUsage, poi per txAvgBandwidthUsage
				2) calcolare uno score che combina distanza, cpuUsage e txAvgBandwidthUsage, e ordinare per score
				Opzione 1: usarlo quando vogliamo una priorità assoluta, una gerarchia di importanza
					ORDER BY geoClass, (txAvgBandwidthUsage / maxBandwidth), (cpuUsage / 100.0)
					Questo significa:
					1.	Prima scegli tra i più vicini
					2.	Tra quelli scegli il meno saturo di banda
					3.	Se pari, scegli quello con meno CPU
					Questo rispetta veramente la gerarchia.
				Opzione 2: calcoliamo uno score. Se lo score lo calcoliamo come
					(geoClass * 0.4 + (cpuUsage / 100.0) * 0.3 + (txAvgBandwidthUsage / (maxTXBandwidthInGbps * 1000000000)) * 0.3) AS score
					è necessario normalizzare tutti i fattori che devono essere portati sulla stessa scala (tipicamente 0–1).
					Altrimenti la banda, essendo un valore molto grande, dominerebbe completamente lo score, anche con moltiplicatore 50.
					Nel calcolo dello score si potrebbe aggiungere anche '+ activeConnections * 10', ma non abbiamo ancora questa informazione in DB,
					e comunque è un'informazione molto volatile, che potrebbe essere obsoleta al momento della lettura, per cui per ora non la consideriamo
				Inoltre usiamo classi di distanza, questo evita micro-ottimizzazioni inutili.
			 */
			// Query 1 – tentativo “ideale” (metriche fresche)
			// ll_to_earth: converte latitudine e longitudine in un punto (x, y, z) sulla superficie terrestre rappresentato come un valore
			// di tipo 'earth', che è un tipo di dato specifico di PostgreSQL per rappresentare posizioni geografiche sulla Terra.
			// earth_distance: calcola la distanza in metri tra due punti sulla superficie terrestre
			// earth_distance(ll_to_earth(41.89, 12.50), ll_to_earth(51.507351, -0.127758)) tra roma e londra:                 1436700 metri
			// earth_distance(ll_to_earth(41.89, 12.50), ll_to_earth(40.712784, -74.005941)) tra roma e new york:              6898673 metri
			// earth_distance(ll_to_earth(41.89, 12.50), ll_to_earth(47.606209, -122.332071)) tra roma e seattle:              9127714 metri
			// earth_distance(ll_to_earth(40.712784, -74.005941), ll_to_earth(47.606209, -122.332071)) tra new york e seattle: 3869880 metri
			// LIMIT 5: se ne prendiamo solo 1 abbiamo un rischio reale di "sticky server". Invece ne prendiamo 5 e ne selezioniamo uno
			// random lato applicativo. Questo riduce moltissimo l’effetto "tutti sullo stesso".
			string sqlStatement = fmt::format(R"(
				WITH rankedServers AS (
					SELECT hostname, txAvgBandwidthUsage, cpuUsage,
						CASE
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 1000000 THEN 0.0 -- very close
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 2000000 THEN 0.1
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 3000000 THEN 0.2
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 4000000 THEN 0.3
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 5000000 THEN 0.4
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 6000000 THEN 0.5
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 7000000 THEN 0.6
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 8000000 THEN 0.7
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 9000000 THEN 0.8
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 10000000 THEN 0.9
							ELSE 1.0   -- far
						END AS geoClass
					FROM MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping a
					WHERE d.deliveryServerKey = a.deliveryServerKey
					AND a.workspaceKey = {workspaceKey}
					AND enabled = true
					{externalCondition}
					-- 0.75: 75% di utilizzo della banda massima, 1/8: conversione da bit a byte
					AND d.txAvgBandwidthUsage <= d.maxTXBandwidthInGbps * 1000000000 * {maxTXBandwidthPerCent} / 8
					AND cpuUsage < {maxCPUPerCent} -- escludiamo server con CPU > 80%
					AND cpuUsageUpdateTime IS NOT NULL
					AND bandwidthUsageUpdateTime IS NOT NULL
					AND (NOW() at time zone 'utc' - bandwidthUsageUpdateTime) <= INTERVAL '{deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds} seconds'
					AND (NOW() at time zone 'utc' - cpuUsageUpdateTime) <= INTERVAL '{deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds} seconds'
				)
				SELECT hostname FROM rankedServers
				ORDER BY geoClass, txAvgBandwidthUsage, cpuUsage
				LIMIT 5
					)",
				fmt::arg("workspaceKey", workspaceKey),
				fmt::arg("externalCondition", external ? std::format("AND external = {}", *external) : ""),
				fmt::arg("playerLatitude", playerLatitude), fmt::arg("playerLongitude", playerLongitude),
				fmt::arg("maxTXBandwidthPerCent", _maxTXBandwidthPerCent), fmt::arg("maxCPUPerCent", _maxCPUPerCent),
				fmt::arg("deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds", _deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			const shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = sqlResultSet->getSqlDuration().count();
			SQLQUERYLOG(
				"getBestDeliveryServer", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				StringUtils::normalizeWhitespace(sqlStatement, true), trans.connection->getConnectionId(), elapsed
			);
			{
				if (!sqlResultSet->empty())
				{
					if (sqlResultSet->size() == 1)
						return (*sqlResultSet)[0]["hostname"].as<string>();

					// selezione di un indice casuale con std::uniform_int_distribution

					// RNG per-thread: evita contese tra thread e non re-seeda ad ogni chiamata
					thread_local std::mt19937 rng{std::random_device{}()};

					std::uniform_int_distribution<std::size_t> dist(0, sqlResultSet->size() - 1);
					const int randomIndex = static_cast<int>(dist(rng));
					// LOG_INFO("uniform_int_distribution {}/{}", randomIndex, sqlResultSet->size() - 1);
					return (*sqlResultSet)[randomIndex]["hostname"].as<string>();
				}
			}
		}

		LOG_WARN("Ideal deliveryServer not selected, trying without considering cpu/bandwidth usage");
		return getBestDeliveryServerBasedOnGeoProximityWithoutMetrics(workspaceKey, external, playerLatitude, playerLongitude);
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

string MMSEngineDBFacade::getBestDeliveryServerBasedOnGeoProximityWithoutMetrics(
	const int64_t workspaceKey, const optional<bool> external, const double playerLatitude, const double playerLongitude)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// Query 2 – fallback (metriche stale ammesse)
		// se cpu/banda smettono di aggiornarsi è importante ritornare un deliveryServer ed evitare che il sistema si blocchi
		// selectedLastTime viene usato per fare round-robin tra i server con metriche stale, in modo da non sovraccaricare
		// sempre lo stesso server quando le metriche sono stale
		// usiamo FOR UPDATE SKIP LOCKED perchè sotto altissima concorrenza rischi:
		// - thundering herd: molte transazioni cercano la stessa riga “più vecchia”;
		// - a seconda dell’isolamento e del piano, alcune transazioni possono aspettare lock e aumentare la latenza
		// Con FOR UPDATE SKIP LOCKED dentro una CTE, ogni transazione "prende" un server diverso senza aspettare.
		// Anche in questa select sono state aggiunte le condizioni su bandwidthUsageUpdateTime e cpuUsageUpdateTime perchè
		// in questo caso indicano che il servizio è running (health check per il server)
		{
			string sqlStatement = fmt::format(R"(
				WITH rankedServers AS (
					SELECT d.deliveryServerKey
					FROM MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping a
					WHERE d.deliveryServerKey = a.deliveryServerKey
					AND a.workspaceKey = {workspaceKey}
					AND enabled = true
					{externalCondition}
					AND cpuUsageUpdateTime IS NOT NULL
					AND bandwidthUsageUpdateTime IS NOT NULL
					AND (NOW() at time zone 'utc' - bandwidthUsageUpdateTime) <= INTERVAL '{deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds} seconds'
					AND (NOW() at time zone 'utc' - cpuUsageUpdateTime) <= INTERVAL '{deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds} seconds'
					ORDER BY
						CASE
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 1000000 THEN 0.0 -- very close
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 2000000 THEN 0.1
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 3000000 THEN 0.2
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 4000000 THEN 0.3
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 5000000 THEN 0.4
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 6000000 THEN 0.5
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 7000000 THEN 0.6
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 8000000 THEN 0.7
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 9000000 THEN 0.8
							WHEN earth_distance(ll_to_earth({playerLatitude}, {playerLongitude}), d.earthCoord) < 10000000 THEN 0.9
							ELSE 1.0   -- far
						END,
						d.selectedLastTime
					LIMIT 1 FOR UPDATE SKIP LOCKED
				)
				-- Aggiorna le righe della tabella MMS_DeliveryServer usando dati provenienti da rankedServers
				-- ma solo dove la condizione del WHERE è vera
				UPDATE MMS_DeliveryServer d
				SET selectedLastTime = NOW() at time zone 'utc'
				FROM rankedServers r
				WHERE d.deliveryServerKey = r.deliveryServerKey
				RETURNING d.hostname
					)",
				fmt::arg("workspaceKey", workspaceKey),
				fmt::arg("externalCondition", external ? std::format("AND external = {}", *external) : ""),
				fmt::arg("playerLatitude", playerLatitude), fmt::arg("playerLongitude", playerLongitude),
				fmt::arg("deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds", _deliveryServersUnavailableIfNotReceivedStatsUpdatesInSeconds)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(trans.transaction->exec(sqlStatement));
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = sqlResultSet->getSqlDuration().count();
			SQLQUERYLOG(
				"getBestDeliveryServer", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				StringUtils::normalizeWhitespace(sqlStatement, true), trans.connection->getConnectionId(), elapsed
			);
			if (sqlResultSet->empty())
			{
				string errorMessage = std::format(
					"deliveryServer was not found"
					", workspaceKey: {}"
					", playerLatitude: {}"
					", playerLongitude: {}",
					workspaceKey, playerLatitude, playerLongitude
				);
				LOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			return (*sqlResultSet)[0]["hostname"].as<string>();
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

json MMSEngineDBFacade::getDeliveryServerRoot(const bool admin, PostgresHelper::SqlResultSet::SqlRow &row,
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
		auto type = row["type"].as<string>();
		deliveryServerRoot["type"] = type;
		if (type == "edge" || type == "mid-origin")
		{
			if (row["originDeliveryServerKey"].isNull())
				deliveryServerRoot["originDeliveryServerKey"] = nullptr;
			else
				deliveryServerRoot["originDeliveryServerKey"] = row["originDeliveryServerKey"].as<int64_t>();
		}
		else
			deliveryServerRoot["originDeliveryServerKey"] = nullptr;
		deliveryServerRoot["external"] = row["external"].as<bool>();
		deliveryServerRoot["enabled"] = row["enabled"].as<bool>();
		deliveryServerRoot["publicIP"] = row["publicIP"].as<string>();
		deliveryServerRoot["internalIP"] = row["internalIP"].as<string>();
		deliveryServerRoot["hostname"] = row["hostname"].as<string>();
		deliveryServerRoot["latitude"] = row["latitude"].as<double>();
		deliveryServerRoot["longitude"] = row["longitude"].as<double>();
		deliveryServerRoot["maxTXBandwidthInGbps"] = row["maxTXBandwidthInGbps"].as<double>();
		deliveryServerRoot["selectedLastTime"] = row["selectedLastTime"].as<string>();
		if (row["cpuUsage"].isNull())
			deliveryServerRoot["cpuUsage"] = nullptr;
		else
			deliveryServerRoot["cpuUsage"] = row["cpuUsage"].as<int32_t>();
		if (row["cpuUsageUpdateTime"].isNull())
			deliveryServerRoot["cpuUsageUpdateTime"] = nullptr;
		else
			deliveryServerRoot["cpuUsageUpdateTime"] = row["cpuUsageUpdateTime"].as<string>();

		if (row["rxAvgBandwidthUsage"].isNull())
			deliveryServerRoot["rxAvgBandwidthUsage"] = nullptr;
		else
			deliveryServerRoot["rxAvgBandwidthUsage"] = row["rxAvgBandwidthUsage"].as<int64_t>();
		if (row["txAvgBandwidthUsage"].isNull())
			deliveryServerRoot["txAvgBandwidthUsage"] = nullptr;
		else
			deliveryServerRoot["txAvgBandwidthUsage"] = row["txAvgBandwidthUsage"].as<int64_t>();

		if (row["rxPeakBandwidthUsage"].isNull())
			deliveryServerRoot["rxPeakBandwidthUsage"] = nullptr;
		else
			deliveryServerRoot["rxPeakBandwidthUsage"] = row["rxPeakBandwidthUsage"].as<int64_t>();
		if (row["txPeakBandwidthUsage"].isNull())
			deliveryServerRoot["txPeakBandwidthUsage"] = nullptr;
		else
			deliveryServerRoot["txPeakBandwidthUsage"] = row["txPeakBandwidthUsage"].as<int64_t>();

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
