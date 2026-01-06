
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

int64_t MMSEngineDBFacade::addEncodingProfilesSetIfNotAlreadyPresent(
	int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string label, bool removeEncodingProfilesIfPresent
)
{
	int64_t encodingProfilesSetKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select encodingProfilesSetKey from MMS_EncodingProfilesSet "
				"where workspaceKey = {} and contentType = {} and label = {}",
				workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(label)
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
				encodingProfilesSetKey = res[0]["encodingProfilesSetKey"].as<int64_t>();

				if (removeEncodingProfilesIfPresent)
				{
					string sqlStatement = std::format(
						"delete from MMS_EncodingProfilesSetMapping "
						"where encodingProfilesSetKey = {} ",
						encodingProfilesSetKey
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
			else
			{
				string sqlStatement = std::format(
					"insert into MMS_EncodingProfilesSet (encodingProfilesSetKey, workspaceKey, contentType, label) values ("
					"DEFAULT, {}, {}, {}) returning encodingProfilesSetKey",
					workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(label)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodingProfilesSetKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return encodingProfilesSetKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
	int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType, DeliveryTechnology deliveryTechnology, string jsonEncodingProfile
)
{
	int64_t encodingProfileKey;
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
		int64_t encodingProfilesSetKey = -1;

		encodingProfileKey =
			addEncodingProfile(trans, workspaceKey, label, contentType, deliveryTechnology, jsonEncodingProfile, encodingProfilesSetKey);
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

	return encodingProfileKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
	PostgresConnTrans &trans, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType, DeliveryTechnology deliveryTechnology,
	string jsonProfile,
	int64_t encodingProfilesSetKey // -1 if it is not associated to any Set
)
{
	int64_t encodingProfileKey;

	try
	{
		{
			string sqlStatement = std::format(
				"select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
				workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(label)
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
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();

				string sqlStatement = std::format(
					"update MMS_EncodingProfile set deliveryTechnology = {}, jsonProfile = {} "
					"where encodingProfileKey = {} ",
					trans.transaction->quote(toString(deliveryTechnology)), trans.transaction->quote(jsonProfile), encodingProfileKey
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
						", rowsUpdated: {}"
						", sqlStatement: {}",
						rowsUpdated, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string sqlStatement = std::format(
					"insert into MMS_EncodingProfile ("
					"encodingProfileKey, workspaceKey, label, contentType, deliveryTechnology, jsonProfile) values ("
					"DEFAULT, {}, {}, {}, {}, {}) returning encodingProfileKey",
					workspaceKey, trans.transaction->quote(label), trans.transaction->quote(toString(contentType)),
					trans.transaction->quote(toString(deliveryTechnology)), trans.transaction->quote(jsonProfile)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodingProfileKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		if (encodingProfilesSetKey != -1)
		{
			{
				string sqlStatement = std::format(
					"select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping "
					"where encodingProfilesSetKey = {} and encodingProfileKey = {}",
					encodingProfilesSetKey, encodingProfileKey
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
					{
						string sqlStatement = std::format(
							"select workspaceKey from MMS_EncodingProfilesSet "
							"where encodingProfilesSetKey = {}",
							encodingProfilesSetKey
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
							int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
							if (localWorkspaceKey != workspaceKey)
							{
								string errorMessage = std::format(
									"It is not possible to use an EncodingProfilesSet if you are not the owner"
									", encodingProfilesSetKey: {}"
									", workspaceKey: {}"
									", localWorkspaceKey: {}"
									", sqlStatement: {}",
									encodingProfilesSetKey, workspaceKey, localWorkspaceKey, sqlStatement
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}

					{
						string sqlStatement = std::format(
							"insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey) "
							"values ({}, {})",
							encodingProfilesSetKey, encodingProfileKey
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

	return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfile(int64_t workspaceKey, int64_t encodingProfileKey)
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
				"delete from MMS_EncodingProfile "
				"where encodingProfileKey = {} and workspaceKey = {} ",
				encodingProfileKey, workspaceKey
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
			if (rowsUpdated == 0)
			{
				string errorMessage = std::format(
					"no delete was done"
					", encodingProfileKey: {}"
					", workspaceKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					encodingProfileKey, workspaceKey, rowsUpdated, sqlStatement
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
}

int64_t MMSEngineDBFacade::addEncodingProfileIntoSetIfNotAlreadyPresent(
	int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType, int64_t encodingProfilesSetKey
)
{
	int64_t encodingProfileKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
				workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(label)
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
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
			}
			else
			{
				string errorMessage = std::format(
					"Encoding profile label was not found"
					", workspaceKey: {}"
					", contentType: {}"
					", label: {}",
					workspaceKey, toString(contentType), label
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			{
				string sqlStatement = std::format(
					"select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping "
					"where encodingProfilesSetKey = {} and encodingProfileKey = {}",
					encodingProfilesSetKey, encodingProfileKey
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
					{
						string sqlStatement = std::format(
							"select workspaceKey from MMS_EncodingProfilesSet "
							"where encodingProfilesSetKey = {}",
							encodingProfilesSetKey
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
							int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
							if (localWorkspaceKey != workspaceKey)
							{
								string errorMessage = std::format(
									"It is not possible to use an EncodingProfilesSet if you are not the owner"
									", encodingProfilesSetKey: {}"
									", workspaceKey: {}"
									", localWorkspaceKey: {}"
									", sqlStatement: {}",
									encodingProfilesSetKey, workspaceKey, localWorkspaceKey, sqlStatement
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}

					{
						string sqlStatement = std::format(
							"insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey) "
							"values ({}, {})",
							encodingProfilesSetKey, encodingProfileKey
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

	return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfilesSet(int64_t workspaceKey, int64_t encodingProfilesSetKey)
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
				"delete from MMS_EncodingProfilesSet "
				"where encodingProfilesSetKey = {} and workspaceKey = {} ",
				encodingProfilesSetKey, workspaceKey
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
			if (rowsUpdated == 0)
			{
				string errorMessage = std::format(
					"no delete was done"
					", encodingProfilesSetKey: {}"
					", workspaceKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					encodingProfilesSetKey, workspaceKey, rowsUpdated, sqlStatement
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
}

json MMSEngineDBFacade::getEncodingProfilesSetList(
	int64_t workspaceKey, int64_t encodingProfilesSetKey, optional<ContentType> contentType
)
{
	json contentListRoot;
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
			"getEncodingProfilesSetList"
			", workspaceKey: {}"
			", encodingProfilesSetKey: {}"
			", contentType: {}",
			workspaceKey, encodingProfilesSetKey, (contentType ? toString(*contentType) : "")
		);

		{
			json requestParametersRoot;

			if (encodingProfilesSetKey != -1)
			{
				field = "encodingProfilesSetKey";
				requestParametersRoot[field] = encodingProfilesSetKey;
			}

			if (contentType)
			{
				field = "contentType";
				requestParametersRoot[field] = toString(*contentType);
			}

			field = "requestParameters";
			contentListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (encodingProfilesSetKey != -1)
			sqlWhere += std::format("and encodingProfilesSetKey = {} ", encodingProfilesSetKey);
		if (contentType)
			sqlWhere += std::format("and contentType = {} ", trans.transaction->quote(toString(*contentType)));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_EncodingProfilesSet {}", sqlWhere);
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

		json encodingProfilesSetsRoot = json::array();
		{
			string sqlStatement = std::format("select encodingProfilesSetKey, contentType, label from MMS_EncodingProfilesSet {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json encodingProfilesSetRoot;

				int64_t localEncodingProfilesSetKey = row["encodingProfilesSetKey"].as<int64_t>();

				field = "encodingProfilesSetKey";
				encodingProfilesSetRoot[field] = localEncodingProfilesSetKey;

				field = "label";
				encodingProfilesSetRoot[field] = row["label"].as<string>();

				ContentType localContentType = MMSEngineDBFacade::toContentType(row["contentType"].as<string>());
				field = "contentType";
				encodingProfilesSetRoot[field] = row["contentType"].as<string>();

				json encodingProfilesRoot = json::array();
				{
					string sqlStatement = std::format(
						"select ep.encodingProfileKey, ep.contentType, ep.label, ep.deliveryTechnology, ep.jsonProfile "
						"from MMS_EncodingProfilesSetMapping epsm, MMS_EncodingProfile ep "
						"where epsm.encodingProfileKey = ep.encodingProfileKey and "
						"epsm.encodingProfilesSetKey = {}",
						localEncodingProfilesSetKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						json encodingProfileRoot;

						field = "global";
						encodingProfileRoot[field] = false;

						field = "encodingProfileKey";
						encodingProfileRoot[field] = row["encodingProfileKey"].as<int64_t>();

						field = "label";
						encodingProfileRoot[field] = row["label"].as<string>();

						field = "contentType";
						encodingProfileRoot[field] = row["contentType"].as<string>();

						field = "deliveryTechnology";
						encodingProfileRoot[field] = row["deliveryTechnology"].as<string>();

						{
							string jsonProfile = row["jsonProfile"].as<string>();

							json profileRoot;
							try
							{
								profileRoot = JSONUtils::toJson<json>(jsonProfile);
							}
							catch (exception &e)
							{
								string errorMessage = std::format(
									"Json metadata failed during the parsing"
									", json data: {}",
									jsonProfile
								);
								SPDLOG_ERROR(errorMessage);

								continue;
							}

							field = "profile";
							encodingProfileRoot[field] = profileRoot;
						}

						encodingProfilesRoot.push_back(encodingProfileRoot);
					}
				}

				field = "encodingProfiles";
				encodingProfilesSetRoot[field] = encodingProfilesRoot;

				encodingProfilesSetsRoot.push_back(encodingProfilesSetRoot);
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

		field = "encodingProfilesSets";
		responseRoot[field] = encodingProfilesSetsRoot;

		field = "response";
		contentListRoot[field] = responseRoot;
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

	return contentListRoot;
}

json MMSEngineDBFacade::getEncodingProfileList(
	int64_t workspaceKey, int64_t encodingProfileKey, optional<ContentType> contentType, string label
)
{
	json contentListRoot;
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
			"getEncodingProfileList"
			", workspaceKey: {}"
			", encodingProfileKey: {}"
			", contentType: {}"
			", label: {}",
			workspaceKey, encodingProfileKey, (contentType ? toString(*contentType) : ""), label
		);

		{
			json requestParametersRoot;

			if (encodingProfileKey != -1)
			{
				field = "encodingProfileKey";
				requestParametersRoot[field] = encodingProfileKey;
			}

			if (contentType)
			{
				field = "contentType";
				requestParametersRoot[field] = toString(*contentType);
			}

			if (label != "")
			{
				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			contentListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where (workspaceKey = {} or workspaceKey is null) ", workspaceKey);
		if (encodingProfileKey != -1)
			sqlWhere += std::format("and encodingProfileKey = {} ", encodingProfileKey);
		if (contentType)
			sqlWhere += std::format("and contentType = {} ", trans.transaction->quote(toString(*contentType)));
		if (label != "")
			sqlWhere += std::format("and lower(label) like lower({}) ", trans.transaction->quote("%" + label + "%"));

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_EncodingProfile {}", sqlWhere);
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

		json encodingProfilesRoot = json::array();
		{
			string sqlStatement = std::format(
				"select workspaceKey, encodingProfileKey, label, contentType, deliveryTechnology, "
				"jsonProfile from MMS_EncodingProfile {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json encodingProfileRoot;

				field = "global";
				if (row["workspaceKey"].is_null())
					encodingProfileRoot[field] = true;
				else
					encodingProfileRoot[field] = false;

				field = "encodingProfileKey";
				encodingProfileRoot[field] = row["encodingProfileKey"].as<int64_t>();

				field = "label";
				encodingProfileRoot[field] = row["label"].as<string>();

				field = "contentType";
				encodingProfileRoot[field] = row["contentType"].as<string>();

				field = "deliveryTechnology";
				encodingProfileRoot[field] = row["deliveryTechnology"].as<string>();

				{
					string jsonProfile = row["jsonProfile"].as<string>();

					json profileRoot;
					try
					{
						profileRoot = JSONUtils::toJson<json>(jsonProfile);
					}
					catch (exception &e)
					{
						string errorMessage = std::format(
							"Json metadata failed during the parsing"
							", json data: {}",
							jsonProfile
						);
						SPDLOG_ERROR(errorMessage);

						continue;
					}

					field = "profile";
					encodingProfileRoot[field] = profileRoot;
				}

				encodingProfilesRoot.push_back(encodingProfileRoot);
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

		field = "encodingProfiles";
		responseRoot[field] = encodingProfilesRoot;

		field = "response";
		contentListRoot[field] = responseRoot;
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

	return contentListRoot;
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetKey(int64_t workspaceKey, int64_t encodingProfilesSetKey)
{
	vector<int64_t> encodingProfilesSetKeys;
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
		{
			string sqlStatement =
				std::format("select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = {}", encodingProfilesSetKey);
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
				int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
				if (localWorkspaceKey != workspaceKey)
				{
					string errorMessage = std::format(
						"WorkspaceKey does not match "
						", encodingProfilesSetKey: {}"
						", workspaceKey: {}"
						", localWorkspaceKey: {}"
						", sqlStatement: {}",
						encodingProfilesSetKey, workspaceKey, localWorkspaceKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = std::format(
					"WorkspaceKey was not found"
					", workspaceKey: {}"
					", sqlStatement: {}",
					workspaceKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = {}", encodingProfilesSetKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
				encodingProfilesSetKeys.push_back(row["encodingProfileKey"].as<int64_t>());
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

	return encodingProfilesSetKeys;
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetLabel(int64_t workspaceKey, string label)
{
	vector<int64_t> encodingProfilesSetKeys;
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
		int64_t encodingProfilesSetKey;
		{
			string sqlStatement = std::format(
				"select encodingProfilesSetKey from MMS_EncodingProfilesSet where workspaceKey = {} and label = {}", workspaceKey,
				trans.transaction->quote(label)
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
				encodingProfilesSetKey = res[0]["encodingProfilesSetKey"].as<int64_t>();
			}
			else
			{
				string errorMessage = std::format(
					"WorkspaceKey/encodingProfilesSetLabel was not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = {}", encodingProfilesSetKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
				encodingProfilesSetKeys.push_back(row["encodingProfileKey"].as<int64_t>());
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

	return encodingProfilesSetKeys;
}

int64_t MMSEngineDBFacade::getEncodingProfileKeyByLabel(
	int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string encodingProfileLabel, bool contentTypeToBeUsed
)
{

	int64_t encodingProfileKey;
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
		{
			string sqlStatement;
			if (contentTypeToBeUsed)
				sqlStatement = std::format(
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
					workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(encodingProfileLabel)
				);
			else
				sqlStatement = std::format(
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = {} or workspaceKey is null) and label = {}",
					workspaceKey, trans.transaction->quote(encodingProfileLabel)
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
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
				if (!contentTypeToBeUsed && res.size() > 1)
				{
					string errorMessage = std::format(
						"contentType has to be used because the label is not unique"
						", workspaceKey: {}"
						", contentType: {}"
						", contentTypeToBeUsed: {}"
						", encodingProfileLabel: {}"
						", sqlStatement: {}",
						workspaceKey, toString(contentType), contentTypeToBeUsed, encodingProfileLabel, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = std::format(
					"encodingProfileKey not found "
					", workspaceKey: {}"
					", contentType: {}"
					", contentTypeToBeUsed: {}"
					", encodingProfileLabel: {}"
					", sqlStatement: {}",
					workspaceKey, toString(contentType), contentTypeToBeUsed, encodingProfileLabel, sqlStatement
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

	return encodingProfileKey;
}

tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
MMSEngineDBFacade::getEncodingProfileDetailsByKey(int64_t workspaceKey, int64_t encodingProfileKey)
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
		string label;
		MMSEngineDBFacade::ContentType contentType;
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		string jsonProfile;
		{
			string sqlStatement = std::format(
				"select label, contentType, deliveryTechnology, jsonProfile from MMS_EncodingProfile where "
				"(workspaceKey = {} or workspaceKey is null) and encodingProfileKey = {}",
				workspaceKey, encodingProfileKey
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
				label = res[0]["label"].as<string>();
				contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
				deliveryTechnology = MMSEngineDBFacade::toDeliveryTechnology(res[0]["deliveryTechnology"].as<string>());
				jsonProfile = res[0]["jsonProfile"].as<string>();
			}
			else
			{
				string errorMessage = std::format(
					"encodingProfileKey not found "
					", workspaceKey: {}"
					", encodingProfileKey: {}"
					", sqlStatement: {}",
					workspaceKey, encodingProfileKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return make_tuple(label, contentType, deliveryTechnology, jsonProfile);
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
