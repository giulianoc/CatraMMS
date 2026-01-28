
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
	const string& label, bool external, bool enabled, const string& publicServerName,
	const string& internalServerName
)
{
	int64_t deliveryServerKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_DeliveryServer(label, external, enabled, publicServerName, internalServerName "
				") values ("
				"{}, {}, {}, {}, {}) returning deliveryServerKey",
				trans.transaction->quote(label), external, enabled, trans.transaction->quote(publicServerName),
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
	int64_t deliveryServerKey, bool labelToBeModified, const string& label, bool externalToBeModified, bool external, bool enabledToBeModified,
	bool enabled, bool publicServerNameToBeModified, const string& publicServerName,
	bool internalServerNameToBeModified, const string& internalServerName
)
{
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
				   "where d.deliveryServerKey = ewm.deliveryServerKey "
				   "and ewm.workspaceKey = {} and {}",
				   workspaceKey, sqlWhere);
			else
				sqlWhere = std::format(
					"where d.deliveryServerKey = ewm.deliveryServerKey "
					"and ewm.workspaceKey = {} ",
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
					"select d.deliveryServerKey, d.label, d.external, d.enabled, "
					"d.publicServerName, d.internalServerName, "
					"to_char(d.selectedLastTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as selectedLastTime, "
					"d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as cpuUsageUpdateTime, "
					"d.txAvgBandwidthUsage, d.rxAvgBandwidthUsage, to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as bandwidthUsageUpdateTime "
					"from MMS_DeliveryServer d {} {} limit {} offset {}",
					sqlWhere, orderByCondition, rows, start
				);
			else
				sqlStatement = std::format(
					"select d.deliveryServerKey, d.label, d.external, d.enabled, "
					"d.publicServerName, d.internalServerName, "
					"to_char(d.selectedLastTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as selectedLastTime, "
					"d.cpuUsage, to_char(d.cpuUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as cpuUsageUpdateTime, "
					"d.txAvgBandwidthUsage, d.rxAvgBandwidthUsage, to_char(d.bandwidthUsageUpdateTime, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as bandwidthUsageUpdateTime "
					"from MMS_DeliveryServer d, MMS_DeliveryServerWorkspaceMapping dwm {} {} limit {} offset {}",
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
		deliveryServerRoot["external"] = row["external"].as<bool>();
		deliveryServerRoot["enabled"] = row["enabled"].as<bool>();
		deliveryServerRoot["publicServerName"] = row["publicServerName"].as<string>();
		deliveryServerRoot["internalServerName"] = row["internalServerName"].as<string>();
		deliveryServerRoot["selectedLastTime"] = row["selectedLastTime"].as<string>();
		deliveryServerRoot["cpuUsage"] = row["cpuUsage"].as<int32_t>();
		if (row["cpuUsageUpdateTime"].isNull())
			deliveryServerRoot["cpuUsageUpdateTime"] = nullptr;
		else
			deliveryServerRoot["cpuUsageUpdateTime"] = row["cpuUsageUpdateTime"].as<string>();
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
		/* TODO
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
		*/

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
