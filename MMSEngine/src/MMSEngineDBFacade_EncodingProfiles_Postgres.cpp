
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

int64_t MMSEngineDBFacade::addEncodingProfilesSetIfNotAlreadyPresent(
	transaction_base *trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string label,
	bool removeEncodingProfilesIfPresent
)
{
	int64_t encodingProfilesSetKey;

	try
	{
		{
			string sqlStatement = fmt::format(
				"select encodingProfilesSetKey from MMS_EncodingProfilesSet "
				"where workspaceKey = {} and contentType = {} and label = {}",
				workspaceKey, trans->quote(toString(contentType)), trans->quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans->exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				encodingProfilesSetKey = res[0]["encodingProfilesSetKey"].as<int64_t>();

				if (removeEncodingProfilesIfPresent)
				{
					string sqlStatement = fmt::format(
						"WITH rows AS (delete from MMS_EncodingProfilesSetMapping "
						"where encodingProfilesSetKey = {} returning 1) select count(*) from rows",
						encodingProfilesSetKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans->exec1(sqlStatement)[0].as<int64_t>();
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
				}
			}
			else
			{
				string sqlStatement = fmt::format(
					"insert into MMS_EncodingProfilesSet (encodingProfilesSetKey, workspaceKey, contentType, label) values ("
					"DEFAULT, {}, {}, {}) returning encodingProfilesSetKey",
					workspaceKey, trans->quote(toString(contentType)), trans->quote(label)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodingProfilesSetKey = trans->exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
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

	return encodingProfilesSetKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
	int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType, DeliveryTechnology deliveryTechnology, string jsonEncodingProfile
)
{
	int64_t encodingProfileKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		int64_t encodingProfilesSetKey = -1;

		encodingProfileKey =
			addEncodingProfile(trans, conn, workspaceKey, label, contentType, deliveryTechnology, jsonEncodingProfile, encodingProfilesSetKey);

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

	return encodingProfileKey;
}

int64_t MMSEngineDBFacade::addEncodingProfile(
	nontransaction &trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
	DeliveryTechnology deliveryTechnology, string jsonProfile,
	int64_t encodingProfilesSetKey // -1 if it is not associated to any Set
)
{
	int64_t encodingProfileKey;

	try
	{
		{
			string sqlStatement = fmt::format(
				"select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
				workspaceKey, trans.quote(toString(contentType)), trans.quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();

				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingProfile set deliveryTechnology = {}, jsonProfile = {} "
					"where encodingProfileKey = {} returning 1) select count(*) from rows",
					trans.quote(toString(deliveryTechnology)), trans.quote(jsonProfile), encodingProfileKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage =
						__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string sqlStatement = fmt::format(
					"insert into MMS_EncodingProfile ("
					"encodingProfileKey, workspaceKey, label, contentType, deliveryTechnology, jsonProfile) values ("
					"DEFAULT, {}, {}, {}, {}, {}) returning encodingProfileKey",
					workspaceKey, trans.quote(label), trans.quote(toString(contentType)), trans.quote(toString(deliveryTechnology)),
					trans.quote(jsonProfile)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				encodingProfileKey = trans.exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
		}

		if (encodingProfilesSetKey != -1)
		{
			{
				string sqlStatement = fmt::format(
					"select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping "
					"where encodingProfilesSetKey = {} and encodingProfileKey = {}",
					encodingProfilesSetKey, encodingProfileKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.exec(sqlStatement);
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (empty(res))
				{
					{
						string sqlStatement = fmt::format(
							"select workspaceKey from MMS_EncodingProfilesSet "
							"where encodingProfilesSetKey = {}",
							encodingProfilesSetKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						result res = trans.exec(sqlStatement);
						SPDLOG_INFO(
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
						if (!empty(res))
						{
							int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
							if (localWorkspaceKey != workspaceKey)
							{
								string errorMessage = __FILEREF__ + "It is not possible to use an EncodingProfilesSet if you are not the owner" +
													  ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey) +
													  ", workspaceKey: " + to_string(workspaceKey) +
													  ", localWorkspaceKey: " + to_string(localWorkspaceKey) + ", sqlStatement: " + sqlStatement;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}

					{
						string sqlStatement = fmt::format(
							"insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey) "
							"values ({}, {})",
							encodingProfilesSetKey, encodingProfileKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						trans.exec0(sqlStatement);
						SPDLOG_INFO(
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
					}
				}
			}
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

	return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfile(int64_t workspaceKey, int64_t encodingProfileKey)
{
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
			string sqlStatement = fmt::format(
				"WITH rows AS (delete from MMS_EncodingProfile "
				"where encodingProfileKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				encodingProfileKey, workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated == 0)
			{
				string errorMessage = __FILEREF__ + "no delete was done" + ", encodingProfileKey: " + to_string(encodingProfileKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
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

int64_t MMSEngineDBFacade::addEncodingProfileIntoSetIfNotAlreadyPresent(
	transaction_base *trans, shared_ptr<PostgresConnection> conn, int64_t workspaceKey, string label, MMSEngineDBFacade::ContentType contentType,
	int64_t encodingProfilesSetKey
)
{
	int64_t encodingProfileKey;

	try
	{
		{
			string sqlStatement = fmt::format(
				"select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
				workspaceKey, trans->quote(toString(contentType)), trans->quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans->exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
			}
			else
			{
				string errorMessage = string("Encoding profile label was not found") + ", workspaceKey: " + to_string(workspaceKey) +
									  ", contentType: " + MMSEngineDBFacade::toString(contentType) + ", label: " + label;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			{
				string sqlStatement = fmt::format(
					"select encodingProfilesSetKey from MMS_EncodingProfilesSetMapping "
					"where encodingProfilesSetKey = {} and encodingProfileKey = {}",
					encodingProfilesSetKey, encodingProfileKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (empty(res))
				{
					{
						string sqlStatement = fmt::format(
							"select workspaceKey from MMS_EncodingProfilesSet "
							"where encodingProfilesSetKey = {}",
							encodingProfilesSetKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						result res = trans->exec(sqlStatement);
						SPDLOG_INFO(
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
						if (!empty(res))
						{
							int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
							if (localWorkspaceKey != workspaceKey)
							{
								string errorMessage = __FILEREF__ + "It is not possible to use an EncodingProfilesSet if you are not the owner" +
													  ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey) +
													  ", workspaceKey: " + to_string(workspaceKey) +
													  ", localWorkspaceKey: " + to_string(localWorkspaceKey) + ", sqlStatement: " + sqlStatement;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}

					{
						string sqlStatement = fmt::format(
							"insert into MMS_EncodingProfilesSetMapping (encodingProfilesSetKey, encodingProfileKey) "
							"values ({}, {})",
							encodingProfilesSetKey, encodingProfileKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						trans->exec0(sqlStatement);
						SPDLOG_INFO(
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
					}
				}
			}
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

	return encodingProfileKey;
}

void MMSEngineDBFacade::removeEncodingProfilesSet(int64_t workspaceKey, int64_t encodingProfilesSetKey)
{
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
			string sqlStatement = fmt::format(
				"WITH rows AS (delete from MMS_EncodingProfilesSet "
				"where encodingProfilesSetKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				encodingProfilesSetKey, workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated == 0)
			{
				string errorMessage = __FILEREF__ + "no delete was done" + ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
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

json MMSEngineDBFacade::getEncodingProfilesSetList(
	int64_t workspaceKey, int64_t encodingProfilesSetKey, bool contentTypePresent, ContentType contentType
)
{
	json contentListRoot;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "getEncodingProfilesSetList" + ", workspaceKey: " + to_string(workspaceKey) +
			", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey) + ", contentTypePresent: " + to_string(contentTypePresent) +
			", contentType: " + (contentTypePresent ? toString(contentType) : "")
		);

		{
			json requestParametersRoot;

			if (encodingProfilesSetKey != -1)
			{
				field = "encodingProfilesSetKey";
				requestParametersRoot[field] = encodingProfilesSetKey;
			}

			if (contentTypePresent)
			{
				field = "contentType";
				requestParametersRoot[field] = toString(contentType);
			}

			field = "requestParameters";
			contentListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
		if (encodingProfilesSetKey != -1)
			sqlWhere += fmt::format("and encodingProfilesSetKey = {} ", encodingProfilesSetKey);
		if (contentTypePresent)
			sqlWhere += fmt::format("and contentType = {} ", trans.quote(toString(contentType)));

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_EncodingProfilesSet {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		json encodingProfilesSetsRoot = json::array();
		{
			string sqlStatement = fmt::format("select encodingProfilesSetKey, contentType, label from MMS_EncodingProfilesSet {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json encodingProfilesSetRoot;

				int64_t localEncodingProfilesSetKey = row["encodingProfilesSetKey"].as<int64_t>();

				field = "encodingProfilesSetKey";
				encodingProfilesSetRoot[field] = localEncodingProfilesSetKey;

				field = "label";
				encodingProfilesSetRoot[field] = row["label"].as<string>();

				ContentType contentType = MMSEngineDBFacade::toContentType(row["contentType"].as<string>());
				field = "contentType";
				encodingProfilesSetRoot[field] = row["contentType"].as<string>();

				json encodingProfilesRoot = json::array();
				{
					string sqlStatement = fmt::format(
						"select ep.encodingProfileKey, ep.contentType, ep.label, ep.deliveryTechnology, ep.jsonProfile "
						"from MMS_EncodingProfilesSetMapping epsm, MMS_EncodingProfile ep "
						"where epsm.encodingProfileKey = ep.encodingProfileKey and "
						"epsm.encodingProfilesSetKey = {}",
						localEncodingProfilesSetKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
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
								profileRoot = JSONUtils::toJson(jsonProfile);
							}
							catch (exception &e)
							{
								string errorMessage = string(
									"Json metadata failed during the parsing"
									", json data: " +
									jsonProfile
								);
								_logger->error(__FILEREF__ + errorMessage);

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
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "encodingProfilesSets";
		responseRoot[field] = encodingProfilesSetsRoot;

		field = "response";
		contentListRoot[field] = responseRoot;

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

	return contentListRoot;
}

json MMSEngineDBFacade::getEncodingProfileList(
	int64_t workspaceKey, int64_t encodingProfileKey, bool contentTypePresent, ContentType contentType, string label
)
{
	json contentListRoot;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "getEncodingProfileList" + ", workspaceKey: " + to_string(workspaceKey) +
			", encodingProfileKey: " + to_string(encodingProfileKey) + ", contentTypePresent: " + to_string(contentTypePresent) +
			", contentType: " + (contentTypePresent ? toString(contentType) : "") + ", label: " + label
		);

		{
			json requestParametersRoot;

			if (encodingProfileKey != -1)
			{
				field = "encodingProfileKey";
				requestParametersRoot[field] = encodingProfileKey;
			}

			if (contentTypePresent)
			{
				field = "contentType";
				requestParametersRoot[field] = toString(contentType);
			}

			if (label != "")
			{
				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			contentListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where (workspaceKey = {} or workspaceKey is null) ", workspaceKey);
		if (encodingProfileKey != -1)
			sqlWhere += fmt::format("and encodingProfileKey = {} ", encodingProfileKey);
		if (contentTypePresent)
			sqlWhere += fmt::format("and contentType = {} ", trans.quote(toString(contentType)));
		if (label != "")
			sqlWhere += fmt::format("and lower(label) like lower({}) ", trans.quote("%" + label + "%"));

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_EncodingProfile {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		json encodingProfilesRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select workspaceKey, encodingProfileKey, label, contentType, deliveryTechnology, "
				"jsonProfile from MMS_EncodingProfile {}",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
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
						profileRoot = JSONUtils::toJson(jsonProfile);
					}
					catch (exception &e)
					{
						string errorMessage = string(
							"Json metadata failed during the parsing"
							", json data: " +
							jsonProfile
						);
						_logger->error(__FILEREF__ + errorMessage);

						continue;
					}

					field = "profile";
					encodingProfileRoot[field] = profileRoot;
				}

				encodingProfilesRoot.push_back(encodingProfileRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "encodingProfiles";
		responseRoot[field] = encodingProfilesRoot;

		field = "response";
		contentListRoot[field] = responseRoot;

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

	return contentListRoot;
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetKey(int64_t workspaceKey, int64_t encodingProfilesSetKey)
{
	vector<int64_t> encodingProfilesSetKeys;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		{
			string sqlStatement =
				fmt::format("select workspaceKey from MMS_EncodingProfilesSet where encodingProfilesSetKey = {}", encodingProfilesSetKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				int64_t localWorkspaceKey = res[0]["workspaceKey"].as<int64_t>();
				if (localWorkspaceKey != workspaceKey)
				{
					string errorMessage = __FILEREF__ + "WorkspaceKey does not match " +
										  ", encodingProfilesSetKey: " + to_string(encodingProfilesSetKey) +
										  ", workspaceKey: " + to_string(workspaceKey) + ", localWorkspaceKey: " + to_string(localWorkspaceKey) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage =
					__FILEREF__ + "WorkspaceKey was not found " + ", workspaceKey: " + to_string(workspaceKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = fmt::format(
				"select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = {}", encodingProfilesSetKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
				encodingProfilesSetKeys.push_back(row["encodingProfileKey"].as<int64_t>());
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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

	return encodingProfilesSetKeys;
}

vector<int64_t> MMSEngineDBFacade::getEncodingProfileKeysBySetLabel(int64_t workspaceKey, string label)
{
	vector<int64_t> encodingProfilesSetKeys;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		int64_t encodingProfilesSetKey;
		{
			string sqlStatement = fmt::format(
				"select encodingProfilesSetKey from MMS_EncodingProfilesSet where workspaceKey = {} and label = {}", workspaceKey, trans.quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				encodingProfilesSetKey = res[0]["encodingProfilesSetKey"].as<int64_t>();
			}
			else
			{
				string errorMessage = __FILEREF__ + "WorkspaceKey/encodingProfilesSetLabel was not found " +
									  ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label
					// + ", sqlStatement: " + sqlStatement    It will be added in catch
					;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = fmt::format(
				"select encodingProfileKey from MMS_EncodingProfilesSetMapping where encodingProfilesSetKey = {}", encodingProfilesSetKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
				encodingProfilesSetKeys.push_back(row["encodingProfileKey"].as<int64_t>());
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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

	return encodingProfilesSetKeys;
}

int64_t MMSEngineDBFacade::getEncodingProfileKeyByLabel(
	int64_t workspaceKey, MMSEngineDBFacade::ContentType contentType, string encodingProfileLabel, bool contentTypeToBeUsed
)
{

	int64_t encodingProfileKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		{
			string sqlStatement;
			if (contentTypeToBeUsed)
				sqlStatement = fmt::format(
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = {} or workspaceKey is null) and contentType = {} and label = {}",
					workspaceKey, trans.quote(toString(contentType)), trans.quote(encodingProfileLabel)
				);
			else
				sqlStatement = fmt::format(
					"select encodingProfileKey from MMS_EncodingProfile where "
					"(workspaceKey = {} or workspaceKey is null) and label = {}",
					workspaceKey, trans.quote(encodingProfileLabel)
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
			{
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
				if (!contentTypeToBeUsed && res.size() > 1)
				{
					string errorMessage =
						__FILEREF__ + "contentType has to be used because the label is not unique" + ", workspaceKey: " + to_string(workspaceKey) +
						", contentType: " + MMSEngineDBFacade::toString(contentType) + ", contentTypeToBeUsed: " + to_string(contentTypeToBeUsed) +
						", encodingProfileLabel: " + encodingProfileLabel + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = __FILEREF__ + "encodingProfileKey not found " + ", workspaceKey: " + to_string(workspaceKey) +
									  ", contentType: " + MMSEngineDBFacade::toString(contentType) +
									  ", contentTypeToBeUsed: " + to_string(contentTypeToBeUsed) + ", encodingProfileLabel: " + encodingProfileLabel +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
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

	return encodingProfileKey;
}

tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
MMSEngineDBFacade::getEncodingProfileDetailsByKey(int64_t workspaceKey, int64_t encodingProfileKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		string label;
		MMSEngineDBFacade::ContentType contentType;
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		string jsonProfile;
		{
			string sqlStatement = fmt::format(
				"select label, contentType, deliveryTechnology, jsonProfile from MMS_EncodingProfile where "
				"(workspaceKey = {} or workspaceKey is null) and encodingProfileKey = {}",
				workspaceKey, encodingProfileKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
				string errorMessage = __FILEREF__ + "encodingProfileKey not found " + ", workspaceKey: " + to_string(workspaceKey) +
									  ", encodingProfileKey: " + to_string(encodingProfileKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(label, contentType, deliveryTechnology, jsonProfile);
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
