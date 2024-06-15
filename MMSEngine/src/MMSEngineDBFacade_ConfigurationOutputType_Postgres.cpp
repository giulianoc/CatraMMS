
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

int64_t MMSEngineDBFacade::addAWSChannelConf(int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type)
{
	int64_t confKey;

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
				"insert into MMS_Conf_AWSChannel(workspaceKey, label, channelId, "
				"rtmpURL, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(channelId), trans.quote(rtmpURL), trans.quote(playURL), trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyAWSChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type
)
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
				"WITH rows AS (update MMS_Conf_AWSChannel set label = {}, channelId = {}, rtmpURL = {}, "
				"playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(channelId), trans.quote(rtmpURL), trans.quote(playURL), trans.quote(type), confKey, workspaceKey
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
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", confKey: " + to_string(confKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
				*/
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

void MMSEngineDBFacade::removeAWSChannelConf(int64_t workspaceKey, int64_t confKey)
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
				"WITH rows AS (delete from MMS_Conf_AWSChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey
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
				string errorMessage = __FILEREF__ + "no delete was done" + ", confKey: " + to_string(confKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

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

json MMSEngineDBFacade::getAWSChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json awsChannelConfListRoot;

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

		_logger->info(__FILEREF__ + "getAWSChannelConfList" + ", workspaceKey: " + to_string(workspaceKey));

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			awsChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where ac.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and ac.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and ac.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and ac.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and ac.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_Conf_AWSChannel ac {}", sqlWhere);
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

		json awsChannelRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select ac.confKey, ac.label, ac.channelId, ac.rtmpURL, ac.playURL, "
				"ac.type, ac.outputIndex, ac.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_AWSChannel ac left join MMS_IngestionJob ij "
				"on ac.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by ac.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json awsChannelConfRoot;

				field = "confKey";
				awsChannelConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				awsChannelConfRoot[field] = row["label"].as<string>();

				field = "channelId";
				awsChannelConfRoot[field] = row["channelId"].as<string>();

				field = "rtmpURL";
				awsChannelConfRoot[field] = row["rtmpURL"].as<string>();

				field = "playURL";
				awsChannelConfRoot[field] = row["playURL"].as<string>();

				field = "type";
				awsChannelConfRoot[field] = row["type"].as<string>();

				field = "outputIndex";
				if (row["outputIndex"].is_null())
					awsChannelConfRoot[field] = nullptr;
				else
					awsChannelConfRoot[field] = row["outputIndex"].as<int>();

				field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					awsChannelConfRoot[field] = nullptr;
				else
					awsChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

				field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					awsChannelConfRoot[field] = nullptr;
				else
					awsChannelConfRoot[field] = row["configurationLabel"].as<string>();

				awsChannelRoot.push_back(awsChannelConfRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "awsChannelConf";
		responseRoot[field] = awsChannelRoot;

		field = "response";
		awsChannelConfListRoot[field] = responseRoot;

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

	return awsChannelConfListRoot;
}

tuple<string, string, string, bool> MMSEngineDBFacade::reserveAWSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "reserveAWSChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label +
			", outputIndex: " + to_string(outputIndex) + ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_AWSChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		{
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_AWSChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveAWSChannel. " +
										  "The following AWS channels are reserved but the relative ingestionJobKey is finished," +
										  "so they will be reset" + ", ingestionJobKeyList: " + ingestionJobKeyList;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedChannelId;
		string reservedRtmpURL;
		string reservedPlayURL;
		int64_t reservedByIngestionJobKey = -1;

		{

			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				// 2023-11-22: perchè ritorni la riga con ingestionJobKey inizializzato bisogna usare asc e non desc
				sqlStatement = fmt::format(
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel "
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {})) "
					"order by reservedByIngestionJobKey asc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey
				);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = __FILEREF__ + "No AWS Channel found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedChannelId = res[0]["channelId"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			reservedPlayURL = res[0]["playURL"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", confKey: " + to_string(reservedConfKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedChannelId, reservedRtmpURL, reservedPlayURL, channelAlreadyReserved);
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

string MMSEngineDBFacade::releaseAWSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
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
		string field;

		_logger->info(
			__FILEREF__ + "releaseAWSChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", outputIndex: " + to_string(outputIndex) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = fmt::format(
				"select confKey, channelId from MMS_Conf_AWSChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = string("No AWS Channel found") + ", workspaceKey: " + to_string(workspaceKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedChannelId = res[0]["channelId"].as<string>();
		}

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", confKey: " + to_string(reservedConfKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return reservedChannelId;
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

int64_t MMSEngineDBFacade::addCDN77ChannelConf(
	int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type
)
{
	int64_t confKey;

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
				"insert into MMS_Conf_CDN77Channel(workspaceKey, label, rtmpURL, "
				"resourceURL, filePath, secureToken, type) values ("
				"{}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(rtmpURL), trans.quote(resourceURL), trans.quote(filePath),
				secureToken == "" ? "null" : trans.quote(secureToken), trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyCDN77ChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type
)
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
				"WITH rows AS (update MMS_Conf_CDN77Channel set label = {}, rtmpURL = {}, resourceURL = {}, "
				"filePath = {}, secureToken = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(rtmpURL), trans.quote(resourceURL), trans.quote(filePath),
				secureToken == "" ? "null" : trans.quote(secureToken), trans.quote(type), confKey, workspaceKey
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
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", confKey: " + to_string(confKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
				*/
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

void MMSEngineDBFacade::removeCDN77ChannelConf(int64_t workspaceKey, int64_t confKey)
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
				"WITH rows AS (delete from MMS_Conf_CDN77Channel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey
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
				string errorMessage = __FILEREF__ + "no delete was done" + ", confKey: " + to_string(confKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

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

json MMSEngineDBFacade::getCDN77ChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json cdn77ChannelConfListRoot;

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

		_logger->info(__FILEREF__ + "getCDN77ChannelConfList" + ", workspaceKey: " + to_string(workspaceKey));

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			cdn77ChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where cc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and cc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and cc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and cc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and cc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_Conf_CDN77Channel cc {}", sqlWhere);
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

		json cdn77ChannelRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select cc.confKey, cc.label, cc.rtmpURL, cc.resourceURL, cc.filePath, "
				"cc.secureToken, cc.type, cc.outputIndex, cc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_CDN77Channel cc left join MMS_IngestionJob ij "
				"on cc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by cc.label ",
				sqlWhere
			);
			;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json cdn77ChannelConfRoot;

				field = "confKey";
				cdn77ChannelConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				cdn77ChannelConfRoot[field] = row["label"].as<string>();

				field = "rtmpURL";
				cdn77ChannelConfRoot[field] = row["rtmpURL"].as<string>();

				field = "resourceURL";
				cdn77ChannelConfRoot[field] = row["resourceURL"].as<string>();

				field = "filePath";
				cdn77ChannelConfRoot[field] = row["filePath"].as<string>();

				field = "secureToken";
				if (row["secureToken"].is_null())
					cdn77ChannelConfRoot[field] = nullptr;
				else
					cdn77ChannelConfRoot[field] = row["secureToken"].as<string>();

				field = "type";
				cdn77ChannelConfRoot[field] = row["type"].as<string>();

				field = "outputIndex";
				if (row["outputIndex"].is_null())
					cdn77ChannelConfRoot[field] = nullptr;
				else
					cdn77ChannelConfRoot[field] = row["outputIndex"].as<int>();

				field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					cdn77ChannelConfRoot[field] = nullptr;
				else
					cdn77ChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

				field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					cdn77ChannelConfRoot[field] = nullptr;
				else
					cdn77ChannelConfRoot[field] = row["configurationLabel"].as<string>();

				cdn77ChannelRoot.push_back(cdn77ChannelConfRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "cdn77ChannelConf";
		responseRoot[field] = cdn77ChannelRoot;

		field = "response";
		cdn77ChannelConfListRoot[field] = responseRoot;

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

	return cdn77ChannelConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getCDN77ChannelDetails(int64_t workspaceKey, string label)
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
		string field;

		_logger->info(__FILEREF__ + "getCDN77ChannelDetails" + ", workspaceKey: " + to_string(workspaceKey));

		string resourceURL;
		string filePath;
		string secureToken;
		{
			string sqlStatement = fmt::format(
				"select resourceURL, filePath, secureToken "
				"from MMS_Conf_CDN77Channel "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label)
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
				string errorMessage =
					__FILEREF__ + "Configuration label is not found" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				_logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);
			}

			resourceURL = res[0]["resourceURL"].as<string>();
			filePath = res[0]["filePath"].as<string>();
			if (!res[0]["secureToken"].is_null())
				secureToken = res[0]["secureToken"].as<string>();
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(resourceURL, filePath, secureToken);
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
	catch (ConfKeyNotFound &e)
	{
		SPDLOG_ERROR(
			"ConfKeyNotFound SQL exception"
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

tuple<string, string, string, string, string, bool>
MMSEngineDBFacade::reserveCDN77Channel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "reserveCDN77Channel" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label +
			", outputIndex: " + to_string(outputIndex) + ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_CDN77Channel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 CDN77)
		{
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_CDN77Channel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveCDN77Channel. " +
										  "The following CDN77 channels are reserved but the relative ingestionJobKey is finished," +
										  "so they will be reset" + ", ingestionJobKeyList: " + ingestionJobKeyList;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		string reservedRtmpURL;
		string reservedResourceURL;
		string reservedFilePath;
		string reservedSecureToken;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				// 2023-11-22: perchè ritorni la riga con ingestionJobKey inizializzato bisogna usare asc e non desc
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, resourceURL, filePath, secureToken, "
					"reservedByIngestionJobKey from MMS_Conf_CDN77Channel "
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {})) "
					"order by reservedByIngestionJobKey asc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey
				);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, resourceURL, filePath, secureToken, "
					"reservedByIngestionJobKey from MMS_Conf_CDN77Channel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = __FILEREF__ + "No CDN77 Channel found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			reservedResourceURL = res[0]["resourceURL"].as<string>();
			reservedFilePath = res[0]["filePath"].as<string>();
			if (!res[0]["secureToken"].is_null())
				reservedSecureToken = res[0]["secureToken"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", confKey: " + to_string(reservedConfKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedRtmpURL, reservedResourceURL, reservedFilePath, reservedSecureToken, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseCDN77Channel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
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
		string field;

		_logger->info(
			__FILEREF__ + "releaseCDN77Channel" + ", workspaceKey: " + to_string(workspaceKey) + ", outputIndex: " + to_string(outputIndex) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_CDN77Channel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = string("No CDN77 Channel found") + ", workspaceKey: " + to_string(workspaceKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", confKey: " + to_string(reservedConfKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
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

int64_t MMSEngineDBFacade::addRTMPChannelConf(
	int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL, string type
)
{
	int64_t confKey;

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
				"insert into MMS_Conf_RTMPChannel(workspaceKey, label, rtmpURL, "
				"streamName, userName, password, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(rtmpURL), streamName == "" ? "null" : trans.quote(streamName),
				userName == "" ? "null" : trans.quote(userName), password == "" ? "null" : trans.quote(password),
				playURL == "" ? "null" : trans.quote(playURL), trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyRTMPChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL,
	string type
)
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
				"WITH rows AS (update MMS_Conf_RTMPChannel set label = {}, rtmpURL = {}, streamName = {}, "
				"userName = {}, password = {}, playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), trans.quote(rtmpURL), streamName == "" ? "null" : trans.quote(streamName),
				userName == "" ? "null" : trans.quote(userName), password == "" ? "null" : trans.quote(password),
				playURL == "" ? "null" : trans.quote(playURL), trans.quote(type), confKey, workspaceKey
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
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", confKey: " + to_string(confKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
				*/
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

void MMSEngineDBFacade::removeRTMPChannelConf(int64_t workspaceKey, int64_t confKey)
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
				"WITH rows AS (delete from MMS_Conf_RTMPChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey
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
				string errorMessage = __FILEREF__ + "no delete was done" + ", confKey: " + to_string(confKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

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

json MMSEngineDBFacade::getRTMPChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json rtmpChannelConfListRoot;

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

		_logger->info(__FILEREF__ + "getRTMPChannelConfList" + ", workspaceKey: " + to_string(workspaceKey));

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			rtmpChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where rc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and rc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and rc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and rc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and rc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_Conf_RTMPChannel rc {}", sqlWhere);
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

		json rtmpChannelRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select rc.confKey, rc.label, rc.rtmpURL, rc.streamName, rc.userName, rc.password, "
				"rc.playURL, rc.type, rc.outputIndex, rc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_RTMPChannel rc left join MMS_IngestionJob ij "
				"on rc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by rc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json rtmpChannelConfRoot;

				field = "confKey";
				rtmpChannelConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				rtmpChannelConfRoot[field] = row["label"].as<string>();

				field = "rtmpURL";
				rtmpChannelConfRoot[field] = row["rtmpURL"].as<string>();

				field = "streamName";
				if (row["streamName"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["streamName"].as<string>();

				field = "userName";
				if (row["userName"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["userName"].as<string>();

				field = "password";
				if (row["password"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["password"].as<string>();

				field = "playURL";
				if (row["playURL"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["playURL"].as<string>();

				field = "type";
				rtmpChannelConfRoot[field] = row["type"].as<string>();

				field = "outputIndex";
				if (row["outputIndex"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["outputIndex"].as<int>();

				field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

				field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row["configurationLabel"].as<string>();

				rtmpChannelRoot.push_back(rtmpChannelConfRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "rtmpChannelConf";
		responseRoot[field] = rtmpChannelRoot;

		field = "response";
		rtmpChannelConfListRoot[field] = responseRoot;

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

	return rtmpChannelConfListRoot;
}

tuple<int64_t, string, string, string, string, string>
MMSEngineDBFacade::getRTMPChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
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
		string field;

		_logger->info(__FILEREF__ + "getRTMPChannelDetails" + ", workspaceKey: " + to_string(workspaceKey));

		int64_t confKey;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
		{
			string sqlStatement = fmt::format(
				"select confKey, rtmpURL, streamName, userName, password, playURL "
				"from MMS_Conf_RTMPChannel "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label)
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
				string errorMessage =
					__FILEREF__ + "Configuration label is not found" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				if (warningIfMissing)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);
			}

			confKey = res[0]["confKey"].as<int64_t>();
			rtmpURL = res[0]["rtmpURL"].as<string>();
			if (!res[0]["streamName"].is_null())
				streamName = res[0]["streamName"].as<string>();
			if (!res[0]["userName"].is_null())
				userName = res[0]["userName"].as<string>();
			if (!res[0]["password"].is_null())
				password = res[0]["password"].as<string>();
			if (!res[0]["playURL"].is_null())
				playURL = res[0]["playURL"].as<string>();
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, rtmpURL, streamName, userName, password, playURL);
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
	catch (ConfKeyNotFound &e)
	{
		SPDLOG_ERROR(
			"ConfKeyNotFound SQL exception"
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

tuple<string, string, string, string, string, string, bool>
MMSEngineDBFacade::reserveRTMPChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "reserveRTMPChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label +
			", outputIndex: " + to_string(outputIndex) + ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 RTMP)
		{
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_RTMPChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveRTMPChannel. " +
										  "The following RTMP channels are reserved but the relative ingestionJobKey is finished," +
										  "so they will be reset" + ", ingestionJobKeyList: " + ingestionJobKeyList;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		string reservedRtmpURL;
		string reservedStreamName;
		string reservedUserName;
		string reservedPassword;
		string reservedPlayURL;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "")
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				// 2023-11-22: perchè ritorni la riga con ingestionJobKey inizializzato bisogna usare asc e non desc
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, streamName, userName, password, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_RTMPChannel "
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {})) "
					"order by reservedByIngestionJobKey asc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey
				);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, rtmpURL, streamName, userName, password, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_RTMPChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = __FILEREF__ + "No RTMP Channel found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedRtmpURL = res[0]["rtmpURL"].as<string>();
			if (!res[0]["streamName"].is_null())
				reservedStreamName = res[0]["streamName"].as<string>();
			if (!res[0]["userName"].is_null())
				reservedUserName = res[0]["userName"].as<string>();
			if (!res[0]["password"].is_null())
				reservedPassword = res[0]["password"].as<string>();
			if (!res[0]["playURL"].is_null())
				reservedPlayURL = res[0]["playURL"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", confKey: " + to_string(reservedConfKey) + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " +
									  sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(
			reservedLabel, reservedRtmpURL, reservedStreamName, reservedUserName, reservedPassword, reservedPlayURL, channelAlreadyReserved
		);
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

void MMSEngineDBFacade::releaseRTMPChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
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
		string field;

		_logger->info(
			__FILEREF__ + "releaseRTMPChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", outputIndex: " + to_string(outputIndex) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_RTMPChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = string("No RTMP Channel found") + ", workspaceKey: " + to_string(workspaceKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", confKey: " + to_string(reservedConfKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
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

int64_t MMSEngineDBFacade::addHLSChannelConf(
	int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type
)
{
	int64_t confKey;

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
				"insert into MMS_Conf_HLSChannel(workspaceKey, label, deliveryCode, "
				"segmentDuration, playlistEntriesNumber, type) values ("
				"{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.quote(label), trans.quote(deliveryCode), segmentDuration == -1 ? "null" : to_string(segmentDuration),
				playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber), trans.quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyHLSChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type
)
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
				"WITH rows AS (update MMS_Conf_HLSChannel set label = {}, deliveryCode = {}, segmentDuration = {}, "
				"playlistEntriesNumber = {}, type = {} "
				"where confKey = {} and workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(label), deliveryCode, segmentDuration == -1 ? "null" : to_string(segmentDuration),
				playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber), trans.quote(type), confKey, workspaceKey
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
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", confKey: " + to_string(confKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
				*/
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

void MMSEngineDBFacade::removeHLSChannelConf(int64_t workspaceKey, int64_t confKey)
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
				"WITH rows AS (delete from MMS_Conf_HLSChannel where confKey = {} and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				confKey, workspaceKey
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
				string errorMessage = __FILEREF__ + "no delete was done" + ", confKey: " + to_string(confKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

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

json MMSEngineDBFacade::getHLSChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json hlsChannelConfListRoot;

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

		_logger->info(__FILEREF__ + "getHLSChannelConfList" + ", workspaceKey: " + to_string(workspaceKey));

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;
			}

			field = "requestParameters";
			hlsChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = fmt::format("where hc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += fmt::format("and hc.confKey = {} ", confKey);
		else if (label != "")
			sqlWhere += fmt::format("and hc.label = {} ", trans.quote(label));
		if (type == 1)
			sqlWhere += "and hc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and hc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_Conf_HLSChannel hc {}", sqlWhere);
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

		json hlsChannelRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select hc.confKey, hc.label, hc.deliveryCode, hc.segmentDuration, hc.playlistEntriesNumber, "
				"hc.type, hc.outputIndex, hc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_HLSChannel hc left join MMS_IngestionJob ij "
				"on hc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by hc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json hlsChannelConfRoot;

				field = "confKey";
				hlsChannelConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				hlsChannelConfRoot[field] = row["label"].as<string>();

				field = "deliveryCode";
				hlsChannelConfRoot[field] = row["deliveryCode"].as<int64_t>();

				field = "segmentDuration";
				if (row["segmentDuration"].is_null())
					hlsChannelConfRoot[field] = nullptr;
				else
					hlsChannelConfRoot[field] = row["segmentDuration"].as<int>();

				field = "playlistEntriesNumber";
				if (row["playlistEntriesNumber"].is_null())
					hlsChannelConfRoot[field] = nullptr;
				else
					hlsChannelConfRoot[field] = row["playlistEntriesNumber"].as<int>();

				field = "type";
				hlsChannelConfRoot[field] = row["type"].as<string>();

				field = "outputIndex";
				if (row["outputIndex"].is_null())
					hlsChannelConfRoot[field] = nullptr;
				else
					hlsChannelConfRoot[field] = row["outputIndex"].as<int>();

				field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					hlsChannelConfRoot[field] = nullptr;
				else
					hlsChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

				field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					hlsChannelConfRoot[field] = nullptr;
				else
					hlsChannelConfRoot[field] = row["configurationLabel"].as<string>();

				hlsChannelRoot.push_back(hlsChannelConfRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "hlsChannelConf";
		responseRoot[field] = hlsChannelRoot;

		field = "response";
		hlsChannelConfListRoot[field] = responseRoot;

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

	return hlsChannelConfListRoot;
}

tuple<int64_t, int64_t, int, int> MMSEngineDBFacade::getHLSChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
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
		string field;

		_logger->info(__FILEREF__ + "getHLSChannelDetails" + ", workspaceKey: " + to_string(workspaceKey));

		int64_t confKey;
		int64_t deliveryCode;
		int segmentDuration;
		int playlistEntriesNumber;
		{
			string sqlStatement = fmt::format(
				"select confKey, deliveryCode, segmentDuration, playlistEntriesNumber "
				"from MMS_Conf_HLSChannel "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.quote(label)
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
				string errorMessage =
					__FILEREF__ + "Configuration label is not found" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				if (warningIfMissing)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

				throw ConfKeyNotFound(errorMessage);
			}

			confKey = res[0]["confKey"].as<int64_t>();
			deliveryCode = res[0]["deliveryCode"].as<int64_t>();
			if (!res[0]["segmentDuration"].is_null())
				segmentDuration = res[0]["segmentDuration"].as<int>();
			if (!res[0]["playlistEntriesNumber"].is_null())
				playlistEntriesNumber = res[0]["playlistEntriesNumber"].as<int>();
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(confKey, deliveryCode, segmentDuration, playlistEntriesNumber);
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
	catch (ConfKeyNotFound &e)
	{
		SPDLOG_ERROR(
			"ConfKeyNotFound SQL exception"
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

tuple<string, int64_t, int, int, bool>
MMSEngineDBFacade::reserveHLSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		string field;

		_logger->info(
			__FILEREF__ + "reserveHLSChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label +
			", outputIndex: " + to_string(outputIndex) + ", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 HLS)
		{
			string sqlStatement = fmt::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status like 'End_%' and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_HLSChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (ingestionJobKeyList != "")
			{
				{
					string errorMessage = __FILEREF__ + "reserveHLSChannel. " +
										  "The following HLS channels are reserved but the relative ingestionJobKey is finished," +
										  "so they will be reset" + ", ingestionJobKeyList: " + ingestionJobKeyList;
					_logger->error(errorMessage);
				}

				{
					string sqlStatement = fmt::format(
						"WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) returning 1) select count(*) from rows",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
						_logger->error(errorMessage);

						// throw runtime_error(errorMessage);
					}
				}
			}
		}

		int64_t reservedConfKey;
		string reservedLabel;
		int64_t reservedDeliveryCode = -1;
		int reservedSegmentDuration = -1;
		int reservedPlaylistEntriesNumber = -1;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label == "") // type is SHARED
			{
				// In caso di ripartenza di mmsEngine, nella tabella avremo già la riga con
				// l'ingestionJobKey e, questo metodo, deve ritornare le info di quella riga.
				// Poichè solo workspaceKey NON è chiave unica, la select, puo' ritornare piu righe:
				// quella con ingestionJobKey inizializzato e quelle con ingestionJobKey NULL.
				// In questo scenario è importante che questo metodo ritorni le informazioni
				// della riga con ingestionJobKey inizializzato.
				// Per questo motivo ho aggiunto: order by reservedByIngestionJobKey desc limit 1
				// 2023-11-22: perchè ritorni la riga con ingestionJobKey inizializzato bisogna usare asc e non desc
				sqlStatement = fmt::format(
					"select confKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, "
					"reservedByIngestionJobKey from MMS_Conf_HLSChannel "
					"where workspaceKey = {} and type = 'SHARED' "
					"and ((outputIndex is null and reservedByIngestionJobKey is null) or (outputIndex = {} and reservedByIngestionJobKey = {})) "
					"order by reservedByIngestionJobKey asc limit 1 for update",
					workspaceKey, outputIndex, ingestionJobKey
				);
			}
			else
			{
				// workspaceKey, label sono chiave unica, quindi la select ritorna una solo riga
				// 2023-09-29: eliminata la condizione 'DEDICATED' in modo che è possibile riservare
				//	anche uno SHARED con la label (i.e.: viene selezionato dalla GUI)
				sqlStatement = fmt::format(
					"select confKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, "
					"reservedByIngestionJobKey from MMS_Conf_HLSChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = __FILEREF__ + "No HLS Channel found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", workspaceKey: " + to_string(workspaceKey) + ", label: " + label;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedLabel = res[0]["label"].as<string>();
			reservedDeliveryCode = res[0]["deliveryCode"].as<int64_t>();
			if (!res[0]["segmentDuration"].is_null())
				reservedSegmentDuration = res[0]["segmentDuration"].as<int>();
			if (!res[0]["playlistEntriesNumber"].is_null())
				reservedPlaylistEntriesNumber = res[0]["playlistEntriesNumber"].as<int>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} returning 1) select count(*) from rows",
				outputIndex, ingestionJobKey, reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", confKey: " + to_string(reservedConfKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedDeliveryCode, reservedSegmentDuration, reservedPlaylistEntriesNumber, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseHLSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
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
		string field;

		_logger->info(
			__FILEREF__ + "releaseHLSChannel" + ", workspaceKey: " + to_string(workspaceKey) + ", outputIndex: " + to_string(outputIndex) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = fmt::format(
				"select confKey from MMS_Conf_HLSChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = string("No HLS Channel found") + ", workspaceKey: " + to_string(workspaceKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} returning 1) select count(*) from rows",
				reservedConfKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", confKey: " + to_string(reservedConfKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
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

