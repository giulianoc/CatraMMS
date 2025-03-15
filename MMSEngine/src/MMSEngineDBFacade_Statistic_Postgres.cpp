
#include "CurlWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

json MMSEngineDBFacade::addRequestStatistic(
	int64_t workspaceKey, string ipAddress, string userId, int64_t physicalPathKey, int64_t confStreamKey, string title
)
{
	if (!_statisticsEnabled)
	{
		json statisticRoot;

		return statisticRoot;
	}

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
		// if (_geoServiceEnabled)
		// 	saveGEOInfo(ipAddress, &trans, conn);

		int64_t requestStatisticKey;
		{
			string sqlStatement = std::format(
				"insert into MMS_RequestStatistic(workspaceKey, ipAddress, userId, physicalPathKey, "
				"confStreamKey, title, requestTimestamp) values ("
				"{}, {}, {}, {}, {}, {}, now() at time zone 'utc') returning requestStatisticKey",
				workspaceKey, (ipAddress == "" ? "null" : trans.transaction->quote(ipAddress)), trans.transaction->quote(userId),
				(physicalPathKey == -1 ? "null" : to_string(physicalPathKey)), (confStreamKey == -1 ? "null" : to_string(confStreamKey)),
				trans.transaction->quote(title)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			requestStatisticKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		// update upToNextRequestInSeconds
		{
			string sqlStatement = std::format(
				"select max(requestStatisticKey) as maxRequestStatisticKey from MMS_RequestStatistic "
				"where workspaceKey = {} and requestStatisticKey < {} and userId = {}",
				workspaceKey, requestStatisticKey, trans.transaction->quote(userId)
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
				if (!res[0][0].is_null())
				{
					int64_t previoudRequestStatisticKey = res[0][0].as<int64_t>();

					{
						sqlStatement = std::format(
							"WITH rows AS (update MMS_RequestStatistic "
							"set upToNextRequestInSeconds = EXTRACT(EPOCH FROM (now() at time zone 'utc' - requestTimestamp)) "
							"where requestStatisticKey = {} returning 1) SELECT count(*) FROM rows",
							previoudRequestStatisticKey
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
								", previoudRequestStatisticKey: {}"
								", rowsUpdated: {}"
								", sqlStatement: {}",
								previoudRequestStatisticKey, rowsUpdated, sqlStatement
							);
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}
			}
		}

		json statisticRoot;
		{
			string field = "requestStatisticKey";
			statisticRoot[field] = requestStatisticKey;

			field = "ipAddress";
			statisticRoot[field] = ipAddress;

			field = "userId";
			statisticRoot[field] = userId;

			field = "physicalPathKey";
			statisticRoot[field] = physicalPathKey;

			field = "confStreamKey";
			statisticRoot[field] = confStreamKey;

			field = "title";
			statisticRoot[field] = title;
		}

		return statisticRoot;
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

int64_t MMSEngineDBFacade::saveLoginStatistics(int userKey, string ip)
{
	int64_t loginStatisticKey;
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
		// if (_geoServiceEnabled)
		// 	saveGEOInfo(ip, &trans, conn);

		{
			string sqlStatement = std::format(
				"insert into MMS_LoginStatistic (userKey, ip, successfulLogin) values ("
				"{}, {}, now() at time zone 'utc') returning loginStatisticKey",
				userKey, ip == "" ? "null" : trans.transaction->quote(ip)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			loginStatisticKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return loginStatisticKey;
}

void MMSEngineDBFacade::updateRequestStatisticGEOInfo()
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
		// limit 100 perchè ip-api gestisce fino a 100 reqs
		int limit = 100;
		bool moreGeoInfoToBeUpdated = true;
		while (moreGeoInfoToBeUpdated)
		{
			try
			{
				vector<string> ipsToBeUpdated;
				{
					string sqlStatement = std::format("select distinct ipAddress from MMS_RequestStatistic where geoInfoKey is null limit {}", limit);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						if (!row["ipAddress"].is_null())
							ipsToBeUpdated.push_back(row["ipAddress"].as<string>());
					}
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@"
						", ipsToBeUpdated.size: {}",
						sqlStatement, trans.connection->getConnectionId(), elapsed, ipsToBeUpdated.size()
					);
				}
				if (ipsToBeUpdated.size() < limit)
					moreGeoInfoToBeUpdated = false;

				// https://members.ip-api.com/docs/batch
				vector<tuple<string, string, string, string, string, string, string, string, string>> ipsAPIGEOInfo =
					getGEOInfo_ipAPI(ipsToBeUpdated);
				for (tuple<string, string, string, string, string, string, string, string, string> ipAPIGEOInfo : ipsAPIGEOInfo)
				{
					auto [ip, continent, continentCode, country, countryCode, regionName, city, org, isp] = ipAPIGEOInfo;

					int64_t geoInfoKey;
					{
						string sqlWhere;
						if (continent != "")
							sqlWhere += std::format("continent = {} ", trans.transaction->quote(continent));
						else
							sqlWhere += std::format("continent is null ");
						if (continentCode != "")
							sqlWhere += std::format("and continentcode = {} ", trans.transaction->quote(continentCode));
						else
							sqlWhere += std::format("and continentcode is null ");
						if (country != "")
							sqlWhere += std::format("and country = {} ", trans.transaction->quote(country));
						else
							sqlWhere += std::format("and country is null ");
						if (countryCode != "")
							sqlWhere += std::format("and countrycode = {} ", trans.transaction->quote(countryCode));
						else
							sqlWhere += std::format("and countrycode is null ");
						if (regionName != "")
							sqlWhere += std::format("and region = {} ", trans.transaction->quote(regionName));
						else
							sqlWhere += std::format("and region is null ");
						if (city != "")
							sqlWhere += std::format("and city = {} ", trans.transaction->quote(city));
						else
							sqlWhere += std::format("and city is null ");
						if (org != "")
							sqlWhere += std::format("and org = {} ", trans.transaction->quote(org));
						else
							sqlWhere += std::format("and org is null ");
						if (isp != "")
							sqlWhere += std::format("and isp = {} ", trans.transaction->quote(isp));
						else
							sqlWhere += std::format("and isp is null ");

						string sqlStatement = std::format(
							"select geoInfoKey from MMS_GEOInfo "
							"where {} ",
							sqlWhere
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
						if (res.empty())
						{
							string sqlStatement = std::format(
								"insert into MMS_GEOInfo(continent, continentcode, country, countrycode, region, city, org, isp) values ("
								"{}, {}, {}, {}, {}, {}, {}, {}) returning geoInfoKey",
								continent == "" ? "null" : trans.transaction->quote(continent),
								continentCode == "" ? "null" : trans.transaction->quote(continentCode),
								country == "" ? "null" : trans.transaction->quote(country),
								countryCode == "" ? "null" : trans.transaction->quote(countryCode),
								regionName == "" ? "null" : trans.transaction->quote(regionName),
								city == "" ? "null" : trans.transaction->quote(city), org == "" ? "null" : trans.transaction->quote(org),
								isp == "" ? "null" : trans.transaction->quote(isp)
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							geoInfoKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
							geoInfoKey = res[0][0].as<int64_t>(-1);
					}

					{
						string sqlStatement = std::format(
							"update MMS_RequestStatistic "
							"set geoInfoKey = {} "
							"where ipAddress = {} and geoInfoKey is null ",
							geoInfoKey, trans.transaction->quote(ip)
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

void MMSEngineDBFacade::updateLoginStatisticGEOInfo()
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
		// limit 100 perchè ip-api gestisce fino a 100 reqs
		int limit = 100;
		bool moreGeoInfoToBeUpdated = true;
		while (moreGeoInfoToBeUpdated)
		{
			try
			{
				vector<string> ipsToBeUpdated;
				{
					string sqlStatement = std::format("select distinct ip from MMS_LoginStatistic where geoInfoKey is null limit {}", limit);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						if (!row["ip"].is_null())
							ipsToBeUpdated.push_back(row["ip"].as<string>());
					}
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@"
						", ipsToBeUpdated.size: {}",
						sqlStatement, trans.connection->getConnectionId(), elapsed, ipsToBeUpdated.size()
					);
				}
				if (ipsToBeUpdated.size() < limit)
					moreGeoInfoToBeUpdated = false;

				// https://members.ip-api.com/docs/batch
				vector<tuple<string, string, string, string, string, string, string, string, string>> ipsAPIGEOInfo =
					getGEOInfo_ipAPI(ipsToBeUpdated);
				for (tuple<string, string, string, string, string, string, string, string, string> ipAPIGEOInfo : ipsAPIGEOInfo)
				{
					auto [ip, continent, continentCode, country, countryCode, regionName, city, org, isp] = ipAPIGEOInfo;

					int64_t geoInfoKey;
					{
						string sqlWhere;
						if (continent != "")
							sqlWhere += std::format("continent = {} ", trans.transaction->quote(continent));
						else
							sqlWhere += std::format("continent is null ");
						if (continentCode != "")
							sqlWhere += std::format("and continentcode = {} ", trans.transaction->quote(continentCode));
						else
							sqlWhere += std::format("and continentcode is null ");
						if (country != "")
							sqlWhere += std::format("and country = {} ", trans.transaction->quote(country));
						else
							sqlWhere += std::format("and country is null ");
						if (countryCode != "")
							sqlWhere += std::format("and countrycode = {} ", trans.transaction->quote(countryCode));
						else
							sqlWhere += std::format("and countrycode is null ");
						if (regionName != "")
							sqlWhere += std::format("and region = {} ", trans.transaction->quote(regionName));
						else
							sqlWhere += std::format("and region is null ");
						if (city != "")
							sqlWhere += std::format("and city = {} ", trans.transaction->quote(city));
						else
							sqlWhere += std::format("and city is null ");
						if (org != "")
							sqlWhere += std::format("and org = {} ", trans.transaction->quote(org));
						else
							sqlWhere += std::format("and org is null ");
						if (isp != "")
							sqlWhere += std::format("and isp = {} ", trans.transaction->quote(isp));
						else
							sqlWhere += std::format("and isp is null ");

						string sqlStatement = std::format(
							"select geoInfoKey from MMS_GEOInfo "
							"where {} ",
							sqlWhere
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
						if (res.empty())
						{
							string sqlStatement = std::format(
								"insert into MMS_GEOInfo(continent, continentcode, country, countrycode, region, city, org, isp) values ("
								"{}, {}, {}, {}, {}, {}, {}, {}) returning geoInfoKey",
								continent == "" ? "null" : trans.transaction->quote(continent),
								continentCode == "" ? "null" : trans.transaction->quote(continentCode),
								country == "" ? "null" : trans.transaction->quote(country),
								countryCode == "" ? "null" : trans.transaction->quote(countryCode),
								regionName == "" ? "null" : trans.transaction->quote(regionName),
								city == "" ? "null" : trans.transaction->quote(city), org == "" ? "null" : trans.transaction->quote(org),
								isp == "" ? "null" : trans.transaction->quote(isp)
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							geoInfoKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
							geoInfoKey = res[0][0].as<int64_t>(-1);
					}

					{
						string sqlStatement = std::format(
							"update MMS_LoginStatistic "
							"set geoInfoKey = {} "
							"where ip = {} and geoInfoKey is null ",
							geoInfoKey, trans.transaction->quote(ip)
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

vector<tuple<string, string, string, string, string, string, string, string, string>> MMSEngineDBFacade::getGEOInfo_ipAPI(vector<string> &ips)
{
	vector<tuple<string, string, string, string, string, string, string, string, string>> ipsAPIGEOInfo;

	if (ips.empty())
		return ipsAPIGEOInfo;

	string fields = "status,message,query,continent,continentCode,country,countryCode,regionName,city,org,isp";
	try
	{
		if (ips.size() > 1)
		{
			// https://pro.ip-api.com/batch?key=GvoGDQ05j7fyQmj
			string geoServiceURL = std::format("{}/batch?key={}", _geoServiceURL, _geoServiceKey);

			json bodyRoot = json::array();
			for (string ip : ips)
			{
				json ipRoot;
				ipRoot["query"] = ip;
				ipRoot["fields"] = fields;
				bodyRoot.push_back(ipRoot);
			}

			vector<string> otherHeaders;
			json geoServiceResponseRoot = CurlWrapper::httpPostStringAndGetJson(
				geoServiceURL, _geoServiceTimeoutInSeconds, "", JSONUtils::toString(bodyRoot), "application/json", otherHeaders
			);

			for (int index = 0, length = geoServiceResponseRoot.size(); index < length; index++)
			{
				json geoServiceResponseIpRoot = geoServiceResponseRoot[index];

				string status = JSONUtils::asString(geoServiceResponseIpRoot, "status", "");
				if (status != "success")
				{
					SPDLOG_ERROR(
						"geoService failed"
						", message: {}",
						JSONUtils::asString(geoServiceResponseIpRoot, "message", "")
					);

					continue;
				}

				string query = JSONUtils::asString(geoServiceResponseIpRoot, "query", "");
				string continent = JSONUtils::asString(geoServiceResponseIpRoot, "continent", "");
				string continentCode = JSONUtils::asString(geoServiceResponseIpRoot, "continentCode", "");
				string country = JSONUtils::asString(geoServiceResponseIpRoot, "country", "");
				string countryCode = JSONUtils::asString(geoServiceResponseIpRoot, "countryCode", "");
				string regionName = JSONUtils::asString(geoServiceResponseIpRoot, "regionName", "");
				string city = JSONUtils::asString(geoServiceResponseIpRoot, "city", "");
				string org = JSONUtils::asString(geoServiceResponseIpRoot, "org", "");
				string isp = JSONUtils::asString(geoServiceResponseIpRoot, "isp", "");

				ipsAPIGEOInfo.push_back(make_tuple(query, continent, continentCode, country, countryCode, regionName, city, org, isp));
			}
		}
		else // if (ips.size() == 1)
		{
			// https://pro.ip-api.com/json/24.48.0.1?key=GvoGDQ05j7fyQmj
			string geoServiceURL = std::format("{}/json/{}?fields={}&key={}", _geoServiceURL, ips[0], fields, _geoServiceKey);

			vector<string> otherHeaders;
			json geoServiceResponseIp = CurlWrapper::httpGetJson(geoServiceURL, _geoServiceTimeoutInSeconds, "", otherHeaders);

			string status = JSONUtils::asString(geoServiceResponseIp, "status", "");
			if (status != "success")
			{
				SPDLOG_ERROR(
					"geoService failed"
					", message: {}",
					JSONUtils::asString(geoServiceResponseIp, "message", "")
				);

				return ipsAPIGEOInfo;
			}

			string query = JSONUtils::asString(geoServiceResponseIp, "query", "");
			string continent = JSONUtils::asString(geoServiceResponseIp, "continent", "");
			string continentCode = JSONUtils::asString(geoServiceResponseIp, "continentCode", "");
			string country = JSONUtils::asString(geoServiceResponseIp, "country", "");
			string countryCode = JSONUtils::asString(geoServiceResponseIp, "countryCode", "");
			string regionName = JSONUtils::asString(geoServiceResponseIp, "regionName", "");
			string city = JSONUtils::asString(geoServiceResponseIp, "city", "");
			string org = JSONUtils::asString(geoServiceResponseIp, "org", "");
			string isp = JSONUtils::asString(geoServiceResponseIp, "isp", "");

			ipsAPIGEOInfo.push_back(make_tuple(query, continent, continentCode, country, countryCode, regionName, city, org, isp));
		}
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "geoService failed (exception)" + ", exception: " + e.what());

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "geoService failed (exception)" + ", exception: " + e.what());

		throw e;
	}

	return ipsAPIGEOInfo;
}

vector<tuple<string, string, string, string, string, string, string, string, string>> MMSEngineDBFacade::getGEOInfo_ipwhois(vector<string> &ips)
{
	vector<tuple<string, string, string, string, string, string, string, string, string>> ipsAPIGEOInfo;

	if (ips.empty())
		return ipsAPIGEOInfo;

	try
	{
		for (string ip : ips)
		{
			string geoServiceURL = _geoServiceURL + ip;

			vector<string> otherHeaders;
			json geoServiceResponse = CurlWrapper::httpGetJson(geoServiceURL, _geoServiceTimeoutInSeconds, "", otherHeaders);

			bool geoSuccess;
			string field = "success";
			geoSuccess = JSONUtils::asBool(geoServiceResponse, field, false);
			if (!geoSuccess)
			{
				string errorMessage = __FILEREF__ + "geoService failed" + ", geoSuccess: " + to_string(geoSuccess);
				_logger->error(errorMessage);

				continue;
			}

			field = "continent";
			string continent = JSONUtils::asString(geoServiceResponse, field, "");
			field = "continent_code";
			string continentCode = JSONUtils::asString(geoServiceResponse, field, "");
			field = "country";
			string country = JSONUtils::asString(geoServiceResponse, field, "");
			field = "country_code";
			string countryCode = JSONUtils::asString(geoServiceResponse, field, "");
			field = "region";
			string regionName = JSONUtils::asString(geoServiceResponse, field, "");
			field = "city";
			string city = JSONUtils::asString(geoServiceResponse, field, "");
			field = "org";
			string org = JSONUtils::asString(geoServiceResponse, field, "");
			field = "isp";
			string isp = JSONUtils::asString(geoServiceResponse, field, "");

			ipsAPIGEOInfo.push_back(make_tuple(ip, continent, continentCode, country, countryCode, regionName, city, org, isp));
		}
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "geoService failed (exception)" + ", exception: " + e.what());

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "geoService failed (exception)" + ", exception: " + e.what());

		throw e;
	}

	return ipsAPIGEOInfo;
}

json MMSEngineDBFacade::getRequestStatisticList(
	int64_t workspaceKey, string userId, string title, string startStatisticDate, string endStatisticDate, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticList"
		", workspaceKey: {}"
		", userId: {}"
		", title: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, userId, title, startStatisticDate, endStatisticDate, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (userId != "")
			sqlWhere += std::format("and userId like {} ", trans.transaction->quote("%" + userId + "%"));
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_RequestStatistic {}", sqlWhere);
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

			field = "numFound";
			responseRoot[field] = count;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select requestStatisticKey, ipAddress, userId, physicalPathKey, confStreamKey, title, "
				"to_char(requestTimestamp, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as formattedRequestTimestamp "
				"from MMS_RequestStatistic {}"
				"order by requestTimestamp asc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "requestStatisticKey";
				statisticRoot[field] = row["requestStatisticKey"].as<int64_t>();

				field = "ipAddress";
				if (row["ipAddress"].is_null())
					statisticRoot[field] = nullptr;
				else
					statisticRoot[field] = row["ipAddress"].as<string>();

				field = "userId";
				statisticRoot[field] = row["userId"].as<string>();

				field = "physicalPathKey";
				if (row["physicalPathKey"].is_null())
					statisticRoot[field] = nullptr;
				else
					statisticRoot[field] = row["physicalPathKey"].as<int64_t>();

				field = "confStreamKey";
				if (row["confStreamKey"].is_null())
					statisticRoot[field] = nullptr;
				else
					statisticRoot[field] = row["confStreamKey"].as<int64_t>();

				field = "title";
				statisticRoot[field] = row["title"].as<string>();

				field = "requestTimestamp";
				statisticRoot[field] = row["formattedRequestTimestamp"].as<string>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerContentList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerContentList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", totalNumFoundToBeCalculated: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, totalNumFoundToBeCalculated, start,
		rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select title, count(*) from MMS_RequestStatistic {}"
				"group by title ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select title, count(*) as count from MMS_RequestStatistic {}"
				"group by title order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "title";
				statisticRoot[field] = row["title"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerUserList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerUserList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select userId, count(*) from MMS_RequestStatistic {}"
				"group by userId ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select userId, count(*) as count from MMS_RequestStatistic {}"
				"group by userId order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "userId";
				statisticRoot[field] = row["userId"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerMonthList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerMonthList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM') ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM') order by date asc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "date";
				statisticRoot[field] = row["date"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerDayList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerDayList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD') as date, count(*) as count "
				"from MMS_RequestStatistic {} "
				"group by to_char(requestTimestamp, 'YYYY-MM-DD') ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD') order by date asc " // order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "date";
				statisticRoot[field] = row["date"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerHourList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerHourList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD HH24') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD HH24') ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select to_char(requestTimestamp, 'YYYY-MM-DD HH24') as date, count(*) as count "
				"from MMS_RequestStatistic {}"
				"group by to_char(requestTimestamp, 'YYYY-MM-DD HH24') order by date asc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "date";
				statisticRoot[field] = row["date"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getRequestStatisticPerCountryList(
	int64_t workspaceKey, string title, string userId, string startStatisticDate, string endStatisticDate,
	int64_t minimalNextRequestDistanceInSeconds, bool totalNumFoundToBeCalculated, int start, int rows
)
{
	SPDLOG_INFO(
		"getRequestStatisticPerCountryList"
		", workspaceKey: {}"
		", title: {}"
		", userId: {}"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", minimalNextRequestDistanceInSeconds: {}"
		", start: {}"
		", rows: {}",
		workspaceKey, title, userId, startStatisticDate, endStatisticDate, minimalNextRequestDistanceInSeconds, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (title != "")
			{
				field = "title";
				requestParametersRoot[field] = title;
			}

			if (userId != "")
			{
				field = "userId";
				requestParametersRoot[field] = userId;
			}

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "minimalNextRequestDistanceInSeconds";
				requestParametersRoot[field] = minimalNextRequestDistanceInSeconds;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = "where r.geoinfokey = g.geoinfokey and ";
		sqlWhere += std::format("r.workspaceKey = {} ", workspaceKey);
		if (title != "")
			sqlWhere += std::format("and LOWER(r.title) like LOWER({}) ", trans.transaction->quote("%" + title + "%"));
		if (userId != "")
			sqlWhere += std::format("and LOWER(r.userId) like LOWER({}) ", trans.transaction->quote("%" + userId + "%"));
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and r.requestTimestamp >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and r.requestTimestamp <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);
		if (minimalNextRequestDistanceInSeconds > 0)
			sqlWhere += std::format("and r.upToNextRequestInSeconds >= {} ", minimalNextRequestDistanceInSeconds);

		json responseRoot;
		if (totalNumFoundToBeCalculated)
		{
			string sqlStatement = std::format(
				"select g.country, count(*) as count "
				"from MMS_RequestStatistic r, MMS_GeoInfo g {}"
				"group by g.country ", // order by count(*) desc ",
				sqlWhere
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

			field = "numFound";
			responseRoot[field] = res.size();
		}
		else
		{
			field = "numFound";
			responseRoot[field] = -1;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select g.country, count(*) as count "
				"from MMS_RequestStatistic r, MMS_GeoInfo g {}"
				"group by g.country order by count(*) desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				statisticRoot["country"] = row["country"].as<string>();

				field = "count";
				statisticRoot[field] = row["count"].as<int64_t>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "requestStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

void MMSEngineDBFacade::retentionOfStatisticData()
{
	SPDLOG_INFO("retentionOfStatisticData");

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
		// check if next partition already exist and, if not, create it
		{
			string partition_start;
			string partition_end;
			string partitionName;
			{
				// 2022-05-01: changed from one to two months because, the first of each
				//	month, until this procedure do not run, it would not work
				chrono::duration<int, ratio<60 * 60 * 24 * 32>> one_month(1);

				// char strDateTime[64];
				string strDateTime;
				tm tmDateTime;
				time_t utcTime;
				chrono::system_clock::time_point today = chrono::system_clock::now();

				chrono::system_clock::time_point nextMonth = today + one_month;
				utcTime = chrono::system_clock::to_time_t(nextMonth);
				localtime_r(&utcTime, &tmDateTime);
				// sprintf(strDateTime, "%04d-%02d-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}-{:0>2}-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partition_start = strDateTime;

				// sprintf(strDateTime, "%04d_%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}_{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partitionName = std::format("requeststatistic_{}", strDateTime);

				chrono::system_clock::time_point nextNextMonth = nextMonth + one_month;
				utcTime = chrono::system_clock::to_time_t(nextNextMonth);
				localtime_r(&utcTime, &tmDateTime);
				// sprintf(strDateTime, "%04d-%02d-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}-{:0>2}-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partition_end = strDateTime;
			}

			/*
			SELECT
				nmsp_parent.nspname AS parent_schema,
				parent.relname      AS parent,
				nmsp_child.nspname  AS child_schema,
				child.relname       AS child
			FROM pg_inherits
				JOIN pg_class parent            ON pg_inherits.inhparent = parent.oid
				JOIN pg_class child             ON pg_inherits.inhrelid   = child.oid
				JOIN pg_namespace nmsp_parent   ON nmsp_parent.oid  = parent.relnamespace
				JOIN pg_namespace nmsp_child    ON nmsp_child.oid   = child.relnamespace
			WHERE parent.relname='mms_requeststatistic';
			*/
			string sqlStatement = std::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.transaction->quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.transaction->exec1(sqlStatement)[0].as<int>();
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
				string sqlStatement = std::format(
					"CREATE TABLE {} PARTITION OF MMS_RequestStatistic "
					"FOR VALUES FROM ({}) TO ({}) ",
					partitionName, trans.transaction->quote(partition_start), trans.transaction->quote(partition_end)
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

		// check if a partition has to be removed because "expired", if yes, remove it
		{
			string partitionName;
			{
				chrono::duration<int, ratio<60 * 60 * 24 * 31>> retentionMonths(_statisticRetentionInMonths);

				chrono::system_clock::time_point today = chrono::system_clock::now();
				chrono::system_clock::time_point retention = today - retentionMonths;
				time_t utcTime_retention = chrono::system_clock::to_time_t(retention);

				// char strDateTime[64];
				string strDateTime;
				tm tmDateTime;

				localtime_r(&utcTime_retention, &tmDateTime);

				// sprintf(strDateTime, "%04d_%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}_{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partitionName = std::format("requeststatistic_{}", strDateTime);
			}

			string sqlStatement = std::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.transaction->quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.transaction->exec1(sqlStatement)[0].as<int>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (count > 0)
			{
				string sqlStatement = std::format("DROP TABLE {}", partitionName);
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

json MMSEngineDBFacade::getLoginStatisticList(string startStatisticDate, string endStatisticDate, int start, int rows)
{
	SPDLOG_INFO(
		"getLoginStatisticList"
		", startStatisticDate: {}"
		", endStatisticDate: {}"
		", start: {}"
		", rows: {}",
		startStatisticDate, endStatisticDate, start, rows
	);

	json statisticsListRoot;

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

		{
			json requestParametersRoot;

			if (startStatisticDate != "")
			{
				field = "startStatisticDate";
				requestParametersRoot[field] = startStatisticDate;
			}

			if (endStatisticDate != "")
			{
				field = "endStatisticDate";
				requestParametersRoot[field] = endStatisticDate;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			statisticsListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = "where s.userKey = u.userKey ";
		if (startStatisticDate != "")
			sqlWhere += std::format(
				"and s.successfulLogin >= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startStatisticDate)
			);
		if (endStatisticDate != "")
			sqlWhere += std::format(
				"and s.successfulLogin <= TO_TIMESTAMP({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endStatisticDate)
			);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_LoginStatistic s, MMS_User u {}", sqlWhere);
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

			field = "numFound";
			responseRoot[field] = count;
		}

		json statisticsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select u.name as userName, u.eMailAddress as emailAddress, s.loginStatisticKey, "
				"s.userKey, s.ip, s.geoinfokey, "
				"to_char(s.successfulLogin, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as formattedSuccessfulLogin "
				"from MMS_LoginStatistic s, MMS_User u {}"
				"order by s.successfulLogin desc "
				"limit {} offset {}",
				sqlWhere, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json statisticRoot;

				field = "loginStatisticKey";
				statisticRoot[field] = row["loginStatisticKey"].as<int64_t>();

				field = "userKey";
				statisticRoot[field] = row["userKey"].as<int64_t>();

				field = "userName";
				statisticRoot[field] = row["userName"].as<string>();

				field = "emailAddress";
				statisticRoot[field] = row["emailAddress"].as<string>();

				field = "ip";
				if (row["ip"].is_null())
					statisticRoot[field] = nullptr;
				else
					statisticRoot[field] = row["ip"].as<string>();

				field = "geoInfoKey";
				if (row["geoinfokey"].is_null())
					statisticRoot[field] = nullptr;
				else
				{
					int64_t geoInfoKey = row["geoinfokey"].as<int64_t>();
					statisticRoot[field] = geoInfoKey;
					{
						field = "geoInfo";
						statisticRoot[field] = getGEOInfo(geoInfoKey);
					}
				}

				field = "successfulLogin";
				if (row["formattedSuccessfulLogin"].is_null())
					statisticRoot[field] = nullptr;
				else
					statisticRoot[field] = row["formattedSuccessfulLogin"].as<string>();

				statisticsRoot.push_back(statisticRoot);
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

		field = "loginStatistics";
		responseRoot[field] = statisticsRoot;

		field = "response";
		statisticsListRoot[field] = responseRoot;
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

	return statisticsListRoot;
}

json MMSEngineDBFacade::getGEOInfo(int64_t geoInfoKey)
{
	SPDLOG_INFO(
		"getGEOInfo"
		", geoInfoKey: {}",
		geoInfoKey
	);

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
		json geoInfoRoot;

		{
			string sqlStatement = std::format(
				"select continent, continentCode, country, countryCode, region, city, org, isp "
				"from MMS_GEOInfo where geoinfokey = {} ",
				geoInfoKey
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
				SPDLOG_WARN(
					"geoInfoKey was not found"
					", geoInfoKey: {}",
					geoInfoKey
				);

				return nullptr;
			}

			string field = "continent";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "continentCode";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "country";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "countryCode";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "region";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "city";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "org";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();

			field = "isp";
			if (res[0][field].is_null())
				geoInfoRoot[field] = nullptr;
			else
				geoInfoRoot[field] = res[0][field].as<string>();
		}

		return geoInfoRoot;
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
