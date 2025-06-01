
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

int64_t MMSEngineDBFacade::addAWSChannelConf(int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type)
{
	int64_t confKey;

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
				"insert into MMS_Conf_AWSChannel(workspaceKey, label, channelId, "
				"rtmpURL, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(channelId), trans.transaction->quote(rtmpURL),
				trans.transaction->quote(playURL), trans.transaction->quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyAWSChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string channelId, string rtmpURL, string playURL, string type
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_AWSChannel set label = {}, channelId = {}, rtmpURL = {}, "
				"playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {}",
				trans.transaction->quote(label), trans.transaction->quote(channelId), trans.transaction->quote(rtmpURL),
				trans.transaction->quote(playURL), trans.transaction->quote(type), confKey, workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement)[0].as<int64_t>();
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
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

void MMSEngineDBFacade::removeAWSChannelConf(int64_t workspaceKey, int64_t confKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_AWSChannel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

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

json MMSEngineDBFacade::getAWSChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label, bool labelLike,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json awsChannelConfListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getAWSChannelConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;

				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}

			field = "requestParameters";
			awsChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where ac.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and ac.confKey = {} ", confKey);
		else if (label != "")
		{
			if (labelLike)
				sqlWhere += std::format("and ac.label like {} ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and ac.label = {} ", trans.transaction->quote(label));
		}
		if (type == 1)
			sqlWhere += "and ac.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and ac.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_AWSChannel ac {}", sqlWhere);
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

		json awsChannelRoot = json::array();
		{
			string sqlStatement = std::format(
				"select ac.confKey, ac.label, ac.channelId, ac.rtmpURL, ac.playURL, "
				"ac.type, ac.outputIndex, ac.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_AWSChannel ac left join MMS_IngestionJob ij "
				"on ac.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by ac.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
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

		field = "awsChannelConf";
		responseRoot[field] = awsChannelRoot;

		field = "response";
		awsChannelConfListRoot[field] = responseRoot;
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

	return awsChannelConfListRoot;
}

tuple<string, string, string, bool> MMSEngineDBFacade::reserveAWSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"reserveAWSChannel"
			", workspaceKey: {}"
			", label: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, label, outputIndex, ingestionJobKey
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_AWSChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // like 'End_%' "
				"and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_AWSChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
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

			if (ingestionJobKeyList != "")
			{
				SPDLOG_ERROR(
					"reserveAWSChannel. "
					"The following AWS channels are reserved but the relative ingestionJobKey is finished, so they will be reset"
					", ingestionJobKeyList: {}",
					ingestionJobKeyList
				);

				{
					string sqlStatement = std::format(
						"update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) ",
						ingestionJobKeyList
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
						SPDLOG_ERROR(
							"no update was done"
							", rowsUpdated: {}"
							", sqlStatement: {}",
							rowsUpdated, sqlStatement
						);

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
				sqlStatement = std::format(
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
				sqlStatement = std::format(
					"select confKey, channelId, rtmpURL, playURL, reservedByIngestionJobKey "
					"from MMS_Conf_AWSChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.transaction->quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = std::format(
					"No AWS Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}"
					", label: {}",
					ingestionJobKey, workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"update MMS_Conf_AWSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} ",
				outputIndex, ingestionJobKey, reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", sqlStatement: "
					", rowsUpdated: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedChannelId, reservedRtmpURL, reservedPlayURL, channelAlreadyReserved);
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

string MMSEngineDBFacade::cdnaws_reservationDetails(int64_t reservedIngestionJobKey, int outputIndex)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"cdnaws_reservationDetails"
			", reservedIngestionJobKey: {}"
			", outputIndex: {}",
			reservedIngestionJobKey, outputIndex
		);

		string playURL;
		{
			string sqlStatement = std::format(
				"select playURL "
				"from MMS_Conf_AWSChannel "
				"where reservedByIngestionJobKey = {} and outputIndex = {} ",
				reservedIngestionJobKey, outputIndex
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
				string errorMessage = std::format(
					"No AWS Channel found"
					", reservedIngestionJobKey: {}"
					", outputIndex: {}",
					reservedIngestionJobKey, outputIndex
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (!res[0]["playURL"].is_null())
				playURL = res[0]["playURL"].as<string>();
		}

		return playURL;
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

string MMSEngineDBFacade::releaseAWSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"releaseAWSChannel"
			", workspaceKey: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, outputIndex, ingestionJobKey
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = std::format(
				"select confKey, channelId from MMS_Conf_AWSChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = std::format(
					"No AWS Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}",
					ingestionJobKey, workspaceKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
			reservedChannelId = res[0]["channelId"].as<string>();
		}

		{
			string sqlStatement = std::format(
				"update MMS_Conf_AWSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} ",
				reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", sqlStatement: "
					", rowsUpdated: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return reservedChannelId;
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

int64_t MMSEngineDBFacade::addCDN77ChannelConf(
	int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type
)
{
	int64_t confKey;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_CDN77Channel(workspaceKey, label, rtmpURL, "
				"resourceURL, filePath, secureToken, type) values ("
				"{}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(rtmpURL), trans.transaction->quote(resourceURL),
				trans.transaction->quote(filePath), secureToken == "" ? "null" : trans.transaction->quote(secureToken), trans.transaction->quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyCDN77ChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string resourceURL, string filePath, string secureToken, string type
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_CDN77Channel set label = {}, rtmpURL = {}, resourceURL = {}, "
				"filePath = {}, secureToken = {}, type = {} "
				"where confKey = {} and workspaceKey = {}",
				trans.transaction->quote(label), trans.transaction->quote(rtmpURL), trans.transaction->quote(resourceURL),
				trans.transaction->quote(filePath), secureToken == "" ? "null" : trans.transaction->quote(secureToken),
				trans.transaction->quote(type), confKey, workspaceKey
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
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

void MMSEngineDBFacade::removeCDN77ChannelConf(int64_t workspaceKey, int64_t confKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_CDN77Channel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

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

json MMSEngineDBFacade::getCDN77ChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label, bool labelLike,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json cdn77ChannelConfListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getCDN77ChannelConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;

				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}

			field = "requestParameters";
			cdn77ChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where cc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and cc.confKey = {} ", confKey);
		else if (label != "")
		{
			if (labelLike)
				sqlWhere += std::format("and cc.label like {} ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and cc.label = {} ", trans.transaction->quote(label));
		}
		if (type == 1)
			sqlWhere += "and cc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and cc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_CDN77Channel cc {}", sqlWhere);
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

		json cdn77ChannelRoot = json::array();
		{
			string sqlStatement = std::format(
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
			result res = trans.transaction->exec(sqlStatement);
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

		field = "cdn77ChannelConf";
		responseRoot[field] = cdn77ChannelRoot;

		field = "response";
		cdn77ChannelConfListRoot[field] = responseRoot;
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

	return cdn77ChannelConfListRoot;
}

tuple<string, string, string> MMSEngineDBFacade::getCDN77ChannelDetails(int64_t workspaceKey, string label)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getCDN77ChannelDetails"
			", workspaceKey: {}",
			workspaceKey
		);

		string resourceURL;
		string filePath;
		string secureToken;
		{
			string sqlStatement = std::format(
				"select resourceURL, filePath, secureToken "
				"from MMS_Conf_CDN77Channel "
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
				string errorMessage = std::format(
					"Configuration label is not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}

			resourceURL = res[0]["resourceURL"].as<string>();
			filePath = res[0]["filePath"].as<string>();
			if (!res[0]["secureToken"].is_null())
				secureToken = res[0]["secureToken"].as<string>();
		}

		return make_tuple(resourceURL, filePath, secureToken);
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

tuple<string, string, string> MMSEngineDBFacade::cdn77_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex)
{
	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"cdn77_reservationDetails"
			", reservedIngestionJobKey: {}"
			", outputIndex: {}",
			reservedIngestionJobKey, outputIndex
		);

		string resourceURL;
		string filePath;
		string secureToken;
		{
			string sqlStatement = std::format(
				"select resourceURL, filePath, secureToken "
				"from MMS_Conf_CDN77Channel "
				"where reservedByIngestionJobKey = {} and outputIndex = {}",
				reservedIngestionJobKey, outputIndex
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
				string errorMessage = std::format(
					"reservedIngestionJob is not found"
					", reservedIngestionJobKey: {}"
					", outputIndex: {}",
					reservedIngestionJobKey, outputIndex
				);
				SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}

			resourceURL = res[0]["resourceURL"].as<string>();
			filePath = res[0]["filePath"].as<string>();
			if (!res[0]["secureToken"].is_null())
				secureToken = res[0]["secureToken"].as<string>();
		}

		return make_tuple(resourceURL, filePath, secureToken);
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

tuple<string, string, string, string, string, bool>
MMSEngineDBFacade::reserveCDN77Channel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"reserveCDN77Channel"
			", workspaceKey: {}"
			", label: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, label, outputIndex, ingestionJobKey
		);

		// 2023-02-01: se ci sono CDN77 reserved da IngestionJobs che sono in stato End*, queste devono essere "resettate".
		//	E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 CDN77)
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // like 'End_%' "
				"and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_CDN77Channel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
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

			if (ingestionJobKeyList != "")
			{
				SPDLOG_ERROR(
					"reserveCDN77Channel. "
					"The following CDN77 channels are reserved but the relative ingestionJobKey is finished, so they will be reset"
					", ingestionJobKeyList: {}",
					ingestionJobKeyList
				);

				{
					string sqlStatement = std::format(
						"update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) ",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						SPDLOG_WARN(
							"no update was done"
							", rowsUpdated: {}"
							", sqlStatement: {}",
							rowsUpdated, sqlStatement
						);

						// throw runtime_error(errorMessage);
					}
					*/
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
				sqlStatement = std::format(
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
				sqlStatement = std::format(
					"select confKey, label, rtmpURL, resourceURL, filePath, secureToken, "
					"reservedByIngestionJobKey from MMS_Conf_CDN77Channel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.transaction->quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = std::format(
					"No CDN77 Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}"
					", label: {}",
					ingestionJobKey, workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"update MMS_Conf_CDN77Channel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} ",
				outputIndex, ingestionJobKey, reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedRtmpURL, reservedResourceURL, reservedFilePath, reservedSecureToken, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseCDN77Channel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"releaseCDN77Channel"
			", workspaceKey: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, outputIndex, ingestionJobKey
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = std::format(
				"select confKey from MMS_Conf_CDN77Channel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = std::format(
					"No CDN77 Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}",
					ingestionJobKey, workspaceKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = std::format(
				"update MMS_Conf_CDN77Channel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} ",
				reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
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

int64_t MMSEngineDBFacade::addRTMPChannelConf(
	int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL, string type
)
{
	int64_t confKey;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_RTMPChannel(workspaceKey, label, rtmpURL, "
				"streamName, userName, password, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(rtmpURL),
				streamName == "" ? "null" : trans.transaction->quote(streamName), userName == "" ? "null" : trans.transaction->quote(userName),
				password == "" ? "null" : trans.transaction->quote(password), playURL == "" ? "null" : trans.transaction->quote(playURL),
				trans.transaction->quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyRTMPChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string rtmpURL, string streamName, string userName, string password, string playURL,
	string type
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_RTMPChannel set label = {}, rtmpURL = {}, streamName = {}, "
				"userName = {}, password = {}, playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(rtmpURL), streamName == "" ? "null" : trans.transaction->quote(streamName),
				userName == "" ? "null" : trans.transaction->quote(userName), password == "" ? "null" : trans.transaction->quote(password),
				playURL == "" ? "null" : trans.transaction->quote(playURL), trans.transaction->quote(type), confKey, workspaceKey
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
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

void MMSEngineDBFacade::removeRTMPChannelConf(int64_t workspaceKey, int64_t confKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_RTMPChannel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

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

json MMSEngineDBFacade::getRTMPChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label, bool labelLike,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json rtmpChannelConfListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getRTMPChannelConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;

				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}

			field = "requestParameters";
			rtmpChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where rc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and rc.confKey = {} ", confKey);
		else if (label != "")
		{
			if (labelLike)
				sqlWhere += std::format("and rc.label like {} ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and rc.label = {} ", trans.transaction->quote(label));
		}
		if (type == 1)
			sqlWhere += "and rc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and rc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_RTMPChannel rc {}", sqlWhere);
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

		json rtmpChannelRoot = json::array();
		{
			string sqlStatement = std::format(
				"select rc.confKey, rc.label, rc.rtmpURL, rc.streamName, rc.userName, rc.password, "
				"rc.playURL, rc.type, rc.outputIndex, rc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_RTMPChannel rc left join MMS_IngestionJob ij "
				"on rc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by rc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
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

		field = "rtmpChannelConf";
		responseRoot[field] = rtmpChannelRoot;

		field = "response";
		rtmpChannelConfListRoot[field] = responseRoot;
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

	return rtmpChannelConfListRoot;
}

tuple<int64_t, string, string, string, string, string>
MMSEngineDBFacade::getRTMPChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getRTMPChannelDetails"
			", workspaceKey: {}",
			workspaceKey
		);

		int64_t confKey;
		string rtmpURL;
		string streamName;
		string userName;
		string password;
		string playURL;
		{
			string sqlStatement = std::format(
				"select confKey, rtmpURL, streamName, userName, password, playURL "
				"from MMS_Conf_RTMPChannel "
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
				string errorMessage = std::format(
					"Configuration label is not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
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

		return make_tuple(confKey, rtmpURL, streamName, userName, password, playURL);
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

string MMSEngineDBFacade::rtmp_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex)
{
	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"rtmp_reservedDetails"
			", reservedIngestionJobKey: {}"
			", outputIndex: {}",
			reservedIngestionJobKey, outputIndex
		);

		string playURL;
		{
			string sqlStatement = std::format(
				"select playURL "
				"from MMS_Conf_RTMPChannel "
				"where reservedByIngestionJobKey = {} and outputIndex = {}",
				reservedIngestionJobKey, outputIndex
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
				string errorMessage = std::format(
					"Configuration label is not found"
					", reservedIngestionJobKey: {}"
					", outputIndex: {}",
					reservedIngestionJobKey, outputIndex
				);
				SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}

			if (!res[0]["playURL"].is_null())
				playURL = res[0]["playURL"].as<string>();
		}

		return playURL;
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

tuple<string, string, string, string, string, string, bool>
MMSEngineDBFacade::reserveRTMPChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"reserveRTMPChannel"
			", workspaceKey: {}"
			", label: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, label, outputIndex, ingestionJobKey
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 RTMP)
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // like 'End_%' "
				"and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_RTMPChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
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

			if (ingestionJobKeyList != "")
			{
				SPDLOG_ERROR(
					"reserveRTMPChannel. "
					"The following RTMP channels are reserved but the relative ingestionJobKey is finished, so they will be reset"
					", ingestionJobKeyList: {}",
					ingestionJobKeyList
				);

				{
					string sqlStatement = std::format(
						"update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) ",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
						SPDLOG_ERROR(errorMessage);

						// throw runtime_error(errorMessage);
					}
					*/
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
				sqlStatement = std::format(
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
				sqlStatement = std::format(
					"select confKey, label, rtmpURL, streamName, userName, password, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_RTMPChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.transaction->quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = std::format(
					"No RTMP Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}"
					", label: {}",
					ingestionJobKey, workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"update MMS_Conf_RTMPChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} ",
				outputIndex, ingestionJobKey, reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(
			reservedLabel, reservedRtmpURL, reservedStreamName, reservedUserName, reservedPassword, reservedPlayURL, channelAlreadyReserved
		);
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

void MMSEngineDBFacade::releaseRTMPChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"releaseRTMPChannel"
			", workspaceKey: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, outputIndex, ingestionJobKey
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = std::format(
				"select confKey from MMS_Conf_RTMPChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = std::format(
					"No RTMP Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}",
					ingestionJobKey, workspaceKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = std::format(
				"update MMS_Conf_RTMPChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} ",
				reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
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

int64_t MMSEngineDBFacade::addHLSChannelConf(
	int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type
)
{
	int64_t confKey;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_HLSChannel(workspaceKey, label, deliveryCode, "
				"segmentDuration, playlistEntriesNumber, type) values ("
				"{}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(deliveryCode),
				segmentDuration == -1 ? "null" : to_string(segmentDuration), playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber),
				trans.transaction->quote(type)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			confKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

	return confKey;
}

void MMSEngineDBFacade::modifyHLSChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, int64_t deliveryCode, int segmentDuration, int playlistEntriesNumber, string type
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_HLSChannel set label = {}, deliveryCode = {}, segmentDuration = {}, "
				"playlistEntriesNumber = {}, type = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), deliveryCode, segmentDuration == -1 ? "null" : to_string(segmentDuration),
				playlistEntriesNumber == -1 ? "null" : to_string(playlistEntriesNumber), trans.transaction->quote(type), confKey, workspaceKey
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
					+ ", sqlStatement: " + sqlStatement
			;
			warn(errorMessage);

			throw runtime_error(errorMessage);
		}
			*/
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

void MMSEngineDBFacade::removeHLSChannelConf(int64_t workspaceKey, int64_t confKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_HLSChannel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

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

json MMSEngineDBFacade::getHLSChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label, bool labelLike,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json hlsChannelConfListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getHLSChannelConfList"
			", workspaceKey: {}",
			workspaceKey
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;

				field = "confKey";
				requestParametersRoot[field] = confKey;

				field = "label";
				requestParametersRoot[field] = label;

				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}

			field = "requestParameters";
			hlsChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where hc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and hc.confKey = {} ", confKey);
		else if (label != "")
		{
			if (labelLike)
				sqlWhere += std::format("and hc.label like {} ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and hc.label = {} ", trans.transaction->quote(label));
		}
		if (type == 1)
			sqlWhere += "and hc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and hc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_HLSChannel hc {}", sqlWhere);
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

		json hlsChannelRoot = json::array();
		{
			string sqlStatement = std::format(
				"select hc.confKey, hc.label, hc.deliveryCode, hc.segmentDuration, hc.playlistEntriesNumber, "
				"hc.type, hc.outputIndex, hc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_HLSChannel hc left join MMS_IngestionJob ij "
				"on hc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by hc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
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

		field = "hlsChannelConf";
		responseRoot[field] = hlsChannelRoot;

		field = "response";
		hlsChannelConfListRoot[field] = responseRoot;
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

	return hlsChannelConfListRoot;
}

tuple<int64_t, int64_t, int, int> MMSEngineDBFacade::getHLSChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getHLSChannelDetails"
			", workspaceKey: {}",
			workspaceKey
		);

		int64_t confKey;
		int64_t deliveryCode;
		int segmentDuration;
		int playlistEntriesNumber;
		{
			string sqlStatement = std::format(
				"select confKey, deliveryCode, segmentDuration, playlistEntriesNumber "
				"from MMS_Conf_HLSChannel "
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
				string errorMessage = std::format(
					"Configuration label is not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}

			confKey = res[0]["confKey"].as<int64_t>();
			deliveryCode = res[0]["deliveryCode"].as<int64_t>();
			if (!res[0]["segmentDuration"].is_null())
				segmentDuration = res[0]["segmentDuration"].as<int>();
			if (!res[0]["playlistEntriesNumber"].is_null())
				playlistEntriesNumber = res[0]["playlistEntriesNumber"].as<int>();
		}

		return make_tuple(confKey, deliveryCode, segmentDuration, playlistEntriesNumber);
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

tuple<string, int64_t, int, int, bool>
MMSEngineDBFacade::reserveHLSChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"reserveHLSChannel"
			", workspaceKey: {}"
			", label: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, label, outputIndex, ingestionJobKey
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_RTMPChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 HLS)
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // like 'End_%' "
				"and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_HLSChannel where "
				"workspaceKey = {} and reservedByIngestionJobKey is not null)",
				workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			string ingestionJobKeyList;
			for (auto row : res)
			{
				int64_t localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList == "")
					ingestionJobKeyList = to_string(localIngestionJobKey);
				else
					ingestionJobKeyList += (", " + to_string(localIngestionJobKey));
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

			if (ingestionJobKeyList != "")
			{
				SPDLOG_ERROR(
					"reserveHLSChannel. "
					"The following HLS channels are reserved but the relative ingestionJobKey is finished, so they will be reset"
					", ingestionJobKeyList: {}",
					ingestionJobKeyList
				);

				{
					string sqlStatement = std::format(
						"update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
						"where reservedByIngestionJobKey in ({}) ",
						ingestionJobKeyList
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
					if (rowsUpdated == 0)
					{
						string errorMessage =
							__FILEREF__ + "no update was done" + ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement " + sqlStatement;
						SPDLOG_ERROR(errorMessage);

						// throw runtime_error(errorMessage);
					}
					*/
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
				sqlStatement = std::format(
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
				sqlStatement = std::format(
					"select confKey, label, deliveryCode, segmentDuration, playlistEntriesNumber, "
					"reservedByIngestionJobKey from MMS_Conf_HLSChannel "
					"where workspaceKey = {} " // and type = 'DEDICATED' "
					"and label = {} "
					"and (reservedByIngestionJobKey is null or reservedByIngestionJobKey = {}) "
					"for update",
					workspaceKey, trans.transaction->quote(label), ingestionJobKey
				);
			}
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
				string errorMessage = std::format(
					"No HLS Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}"
					", label: {}",
					ingestionJobKey, workspaceKey, label
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"update MMS_Conf_HLSChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
				"where confKey = {} ",
				outputIndex, ingestionJobKey, reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool channelAlreadyReserved;
		if (reservedByIngestionJobKey == -1)
			channelAlreadyReserved = false;
		else
			channelAlreadyReserved = true;

		return make_tuple(reservedLabel, reservedDeliveryCode, reservedSegmentDuration, reservedPlaylistEntriesNumber, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseHLSChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(trans.connection->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"releaseHLSChannel"
			", workspaceKey: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, outputIndex, ingestionJobKey
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = std::format(
				"select confKey from MMS_Conf_HLSChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
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
				string errorMessage = std::format(
					"No HLS Channel found"
					", ingestionJobKey: {}"
					", workspaceKey: {}",
					ingestionJobKey, workspaceKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			reservedConfKey = res[0]["confKey"].as<int64_t>();
		}

		{
			string sqlStatement = std::format(
				"update MMS_Conf_HLSChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
				"where confKey = {} ",
				reservedConfKey
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
					", ingestionJobKey: {}"
					", confKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					ingestionJobKey, reservedConfKey, rowsUpdated, sqlStatement
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
