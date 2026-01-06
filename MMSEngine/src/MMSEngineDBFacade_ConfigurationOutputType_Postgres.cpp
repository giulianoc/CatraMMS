
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

int64_t MMSEngineDBFacade::addRTMPChannelConf(
	int64_t workspaceKey, const string& label, const string& rtmpURL, const string& streamName, const string& userName, const string& password,
	const json& playURLDetailsRoot, const string& type
)
{
	int64_t confKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_RTMPChannel(workspaceKey, label, rtmpURL, "
				"streamName, userName, password, playURLDetails, type) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(rtmpURL),
				streamName.empty() ? "null" : trans.transaction->quote(streamName),
				userName.empty() ? "null" : trans.transaction->quote(userName),
				password.empty() ? "null" : trans.transaction->quote(password),
				playURLDetailsRoot == nullptr ? "null" : trans.transaction->quote(JSONUtils::toString(playURLDetailsRoot)),
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
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
	int64_t confKey, int64_t workspaceKey, const string& label, const string& rtmpURL, const string& streamName, const string& userName,
	const string& password, const json& playURLDetailsRoot, const string& type
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_RTMPChannel set label = {}, rtmpURL = {}, streamName = {}, "
				"userName = {}, password = {}, playURLDetails = {}, type = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(rtmpURL),
				streamName.empty() ? "null" : trans.transaction->quote(streamName),
				userName.empty() ? "null" : trans.transaction->quote(userName),
				password.empty() ? "null" : trans.transaction->quote(password),
				playURLDetailsRoot == nullptr ? "null" : trans.transaction->quote(JSONUtils::toString(playURLDetailsRoot)),
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
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
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_RTMPChannel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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
		auto const *se = dynamic_cast<sql_error const *>(&e);
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
		else if (!label.empty())
		{
			if (labelLike)
				sqlWhere += std::format("and LOWER(rc.label) like LOWER({}) ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and LOWER(rc.label) = LOWER({}) ", trans.transaction->quote(label));
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
				"rc.playURLDetails, rc.type, rc.outputIndex, rc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_RTMPChannel rc left join MMS_IngestionJob ij "
				"on rc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by rc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
			for (auto row : *sqlResultSet)
			{
				json rtmpChannelConfRoot;

				field = "confKey";
				rtmpChannelConfRoot[field] = row[0].as<int64_t>(static_cast<int64_t>(-1));

				field = "label";
				rtmpChannelConfRoot[field] = row[1].as<string>("");

				field = "rtmpURL";
				rtmpChannelConfRoot[field] = row[2].as<string>("");

				field = "streamName";
				if (row[3].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[3].as<string>("");

				field = "userName";
				if (row[4].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[4].as<string>("");

				field = "password";
				if (row[5].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[5].as<string>("");

				field = "playURLDetails";
				if (row[6].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[6].as<json>(nullptr);

				field = "type";
				rtmpChannelConfRoot[field] = row[7].as<string>("");

				field = "outputIndex";
				if (row[8].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[8].as<int16_t>(static_cast<int16_t>(-1));

				field = "reservedByIngestionJobKey";
				if (row[9].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[9].as<int64_t>(static_cast<int64_t>(-1));

				field = "configurationLabel";
				if (row[10].isNull())
					rtmpChannelConfRoot[field] = nullptr;
				else
					rtmpChannelConfRoot[field] = row[10].as<string>("");

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
		auto const *se = dynamic_cast<sql_error const *>(&e);
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

int64_t MMSEngineDBFacade::getRTMPChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
{
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
		{
			string sqlStatement = std::format(
				"select confKey "
				"from MMS_Conf_RTMPChannel "
				"where workspaceKey = {} and label = {}",
				workspaceKey, trans.transaction->quote(label)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (sqlResultSet->empty())
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

			confKey = (*sqlResultSet)[0][0].as<int64_t>(static_cast<int64_t>(-1));
		}

		return confKey;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
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

json MMSEngineDBFacade::rtmp_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex)
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

		json playURLDetailsRoot;
		{
			string sqlStatement = std::format(
				"select playURLDetails "
				"from MMS_Conf_RTMPChannel "
				"where reservedByIngestionJobKey = {} and outputIndex = {}",
				reservedIngestionJobKey, outputIndex
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			const result res = trans.transaction->exec(sqlStatement);
			const shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (sqlResultSet->empty())
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

			playURLDetailsRoot = (*sqlResultSet)[0][0].as<json>(nullptr);
		}

		return playURLDetailsRoot;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
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

tuple<string, string, string, string, string, bool, json>
MMSEngineDBFacade::reserveRTMPChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
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
				auto localIngestionJobKey = row["ingestionJobKey"].as<int64_t>();
				if (ingestionJobKeyList.empty())
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

			if (!ingestionJobKeyList.empty())
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
		json reservedPlayURLDetails = nullptr;
		int64_t reservedByIngestionJobKey = -1;

		{
			// 2023-02-16: In caso di ripartenza di mmsEngine, in caso di richiesta
			// già attiva, deve ritornare le stesse info associate a ingestionJobKey
			string sqlStatement;
			if (label.empty())
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
					"select confKey, label, rtmpURL, streamName, userName, password, playURLDetails, "
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
					"select confKey, label, rtmpURL, streamName, userName, password, playURLDetails, "
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
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
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

			reservedConfKey = (*sqlResultSet)[0][0].as<int64_t>(static_cast<int64_t>(-1));
			reservedLabel = (*sqlResultSet)[0][1].as<string>("");
			reservedRtmpURL = (*sqlResultSet)[0][2].as<string>("");
			if (!(*sqlResultSet)[0][3].isNull())
				reservedStreamName = (*sqlResultSet)[0][3].as<string>("");
			if (!(*sqlResultSet)[0][4].isNull())
				reservedUserName = (*sqlResultSet)[0][4].as<string>("");
			if (!(*sqlResultSet)[0][5].isNull())
				reservedPassword = (*sqlResultSet)[0][5].as<string>("");
			if (!(*sqlResultSet)[0][6].isNull())
				reservedPlayURLDetails = (*sqlResultSet)[0][6].as<json>(nullptr);
			if (!(*sqlResultSet)[0][7].isNull())
				reservedByIngestionJobKey = (*sqlResultSet)[0][7].as<int64_t>(static_cast<int64_t>(-1));
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

		return make_tuple(reservedLabel, reservedRtmpURL, reservedStreamName, reservedUserName, reservedPassword,
			channelAlreadyReserved, reservedPlayURLDetails
		);
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
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

json MMSEngineDBFacade::releaseRTMPChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
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
		json reservedPlayURLDetails = nullptr;

		{
			string sqlStatement = std::format(
				"select confKey, playURLDetails from MMS_Conf_RTMPChannel "
				"where workspaceKey = {} and outputIndex = {} and reservedByIngestionJobKey = {} ",
				workspaceKey, outputIndex, ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = PostgresHelper::buildResult(res);
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

			reservedConfKey = (*sqlResultSet)[0][0].as<int64_t>(static_cast<int64_t>(-1));
			reservedPlayURLDetails = (*sqlResultSet)[0][1].as<json>(nullptr);
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

		return reservedPlayURLDetails;
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
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

int64_t MMSEngineDBFacade::addSRTChannelConf(
	int64_t workspaceKey, string label, string srtURL, string mode, string streamId, string passphrase, string playURL, string type
)
{
	int64_t confKey;

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_SRTChannel(workspaceKey, label, srtURL, "
				"mode, streamId, passwphrase, playURL, type) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(srtURL), mode == "" ? "null" : trans.transaction->quote(mode),
				streamId == "" ? "null" : trans.transaction->quote(streamId), passphrase == "" ? "null" : trans.transaction->quote(passphrase),
				playURL == "" ? "null" : trans.transaction->quote(playURL), trans.transaction->quote(type)
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

void MMSEngineDBFacade::modifySRTChannelConf(
	int64_t confKey, int64_t workspaceKey, string label, string srtURL, string mode, string streamId, string passphrase, string playURL, string type
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_Conf_SRTChannel set label = {}, srtURL = {}, mode = {}, "
				"streamId = {}, passphrase = {}, playURL = {}, type = {} "
				"where confKey = {} and workspaceKey = {} ",
				trans.transaction->quote(label), trans.transaction->quote(srtURL), mode == "" ? "null" : trans.transaction->quote(mode),
				streamId == "" ? "null" : trans.transaction->quote(streamId), passphrase == "" ? "null" : trans.transaction->quote(passphrase),
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

void MMSEngineDBFacade::removeSRTChannelConf(int64_t workspaceKey, int64_t confKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format("delete from MMS_Conf_SRTChannel where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
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

json MMSEngineDBFacade::getSRTChannelConfList(
	int64_t workspaceKey, int64_t confKey, string label, bool labelLike,
	int type // 0: all, 1: SHARED, 2: DEDICATED
)
{
	json srtChannelConfListRoot;

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getSRTChannelConfList"
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
			srtChannelConfListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where rc.workspaceKey = {} ", workspaceKey);
		if (confKey != -1)
			sqlWhere += std::format("and rc.confKey = {} ", confKey);
		else if (label != "")
		{
			if (labelLike)
				sqlWhere += std::format("and LOWER(rc.label) like LOWER({}) ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and LOWER(rc.label) = LOWER({}) ", trans.transaction->quote(label));
		}
		if (type == 1)
			sqlWhere += "and rc.type = 'SHARED' ";
		else if (type == 2)
			sqlWhere += "and rc.type = 'DEDICATED' ";

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_SRTChannel rc {}", sqlWhere);
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

		json srtChannelRoot = json::array();
		{
			string sqlStatement = std::format(
				"select rc.confKey, rc.label, rc.srtURL, rc.mode, rc.streamId, rc.passphrase, "
				"rc.playURL, rc.type, rc.outputIndex, rc.reservedByIngestionJobKey, "
				"ij.metaDataContent ->> 'configurationLabel' as configurationLabel "
				"from MMS_Conf_SRTChannel rc left join MMS_IngestionJob ij "
				"on rc.reservedByIngestionJobKey = ij.ingestionJobKey {} "
				"order by rc.label ",
				sqlWhere
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json srtChannelConfRoot;

				field = "confKey";
				srtChannelConfRoot[field] = row["confKey"].as<int64_t>();

				field = "label";
				srtChannelConfRoot[field] = row["label"].as<string>();

				field = "srtURL";
				srtChannelConfRoot[field] = row["srtURL"].as<string>();

				field = "mode";
				if (row["mode"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["mode"].as<string>();

				field = "streamId";
				if (row["streamId"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["streamId"].as<string>();

				field = "passphrase";
				if (row["passphrase"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["passphrase"].as<string>();

				field = "playURL";
				if (row["playURL"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["playURL"].as<string>();

				field = "type";
				srtChannelConfRoot[field] = row["type"].as<string>();

				field = "outputIndex";
				if (row["outputIndex"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["outputIndex"].as<int>();

				field = "reservedByIngestionJobKey";
				if (row["reservedByIngestionJobKey"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["reservedByIngestionJobKey"].as<int64_t>();

				field = "configurationLabel";
				if (row["configurationLabel"].is_null())
					srtChannelConfRoot[field] = nullptr;
				else
					srtChannelConfRoot[field] = row["configurationLabel"].as<string>();

				srtChannelRoot.push_back(srtChannelConfRoot);
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

		field = "srtChannelConf";
		responseRoot[field] = srtChannelRoot;

		field = "response";
		srtChannelConfListRoot[field] = responseRoot;
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

	return srtChannelConfListRoot;
}

tuple<int64_t, string, string, string, string, string>
MMSEngineDBFacade::getSRTChannelDetails(int64_t workspaceKey, string label, bool warningIfMissing)
{
	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getSRTChannelDetails"
			", workspaceKey: {}",
			workspaceKey
		);

		int64_t confKey;
		string srtURL;
		string mode;
		string streamId;
		string passphrase;
		string playURL;
		{
			string sqlStatement = std::format(
				"select confKey, srtURL, mode, streamId, passphrase, playURL "
				"from MMS_Conf_SRTChannel "
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
			srtURL = res[0]["srtURL"].as<string>();
			if (!res[0]["mode"].is_null())
				mode = res[0]["mode"].as<string>();
			if (!res[0]["streamId"].is_null())
				streamId = res[0]["streamId"].as<string>();
			if (!res[0]["passphrase"].is_null())
				passphrase = res[0]["passphrase"].as<string>();
			if (!res[0]["playURL"].is_null())
				playURL = res[0]["playURL"].as<string>();
		}

		return make_tuple(confKey, srtURL, mode, streamId, passphrase, playURL);
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

string MMSEngineDBFacade::srt_reservationDetails(int64_t reservedIngestionJobKey, int16_t outputIndex)
{
	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"srt_reservedDetails"
			", reservedIngestionJobKey: {}"
			", outputIndex: {}",
			reservedIngestionJobKey, outputIndex
		);

		string playURL;
		{
			string sqlStatement = std::format(
				"select playURL "
				"from MMS_Conf_SRTChannel "
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
MMSEngineDBFacade::reserveSRTChannel(int64_t workspaceKey, string label, int outputIndex, int64_t ingestionJobKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		string field;

		SPDLOG_INFO(
			"reserveSRTChannel"
			", workspaceKey: {}"
			", label: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, label, outputIndex, ingestionJobKey
		);

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_SRTChannel
		//	che in realtà è in stato 'End_*'. E' uno scenario che non dovrebbe mai capitare ma,
		//	nel caso in cui dovesse capitare, eseguiamo prima questo update.
		// Aggiunto distinct perchè fissato reservedByIngestionJobKey ci potrebbero essere diversi
		// outputIndex (stesso ingestion con ad esempio 2 SRT)
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select ingestionJobKey  from MMS_IngestionJob where "
				"status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // like 'End_%' "
				"and ingestionJobKey in ("
				"select distinct reservedByIngestionJobKey from MMS_Conf_SRTChannel where "
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
					"reserveSRTChannel. "
					"The following SRT channels are reserved but the relative ingestionJobKey is finished, so they will be reset"
					", ingestionJobKeyList: {}",
					ingestionJobKeyList
				);

				{
					string sqlStatement = std::format(
						"update MMS_Conf_SRTChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
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
		string reservedSrtURL;
		string reservedMode;
		string reservedStreamId;
		string reservedPassphrase;
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
					"select confKey, label, srtURL, mode, streamId, passphrase, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_SRTChannel "
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
					"select confKey, label, srtURL, mode, streamId, passphrase, playURL, "
					"reservedByIngestionJobKey from MMS_Conf_SRTChannel "
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
					"No SRT Channel found"
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
			reservedSrtURL = res[0]["srtURL"].as<string>();
			if (!res[0]["mode"].is_null())
				reservedMode = res[0]["mode"].as<string>();
			if (!res[0]["streamId"].is_null())
				reservedStreamId = res[0]["streamId"].as<string>();
			if (!res[0]["passphrase"].is_null())
				reservedPassphrase = res[0]["passphrase"].as<string>();
			if (!res[0]["playURL"].is_null())
				reservedPlayURL = res[0]["playURL"].as<string>();
			if (!res[0]["reservedByIngestionJobKey"].is_null())
				reservedByIngestionJobKey = res[0]["reservedByIngestionJobKey"].as<int64_t>();
		}

		if (reservedByIngestionJobKey == -1)
		{
			string sqlStatement = std::format(
				"update MMS_Conf_SRTChannel set outputIndex = {}, reservedByIngestionJobKey = {} "
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

		return make_tuple(reservedLabel, reservedSrtURL, reservedMode, reservedStreamId, reservedPassphrase, reservedPlayURL, channelAlreadyReserved);
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

void MMSEngineDBFacade::releaseSRTChannel(int64_t workspaceKey, int outputIndex, int64_t ingestionJobKey)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"releaseSRTChannel"
			", workspaceKey: {}"
			", outputIndex: {}"
			", ingestionJobKey: {}",
			workspaceKey, outputIndex, ingestionJobKey
		);

		int64_t reservedConfKey;
		string reservedChannelId;

		{
			string sqlStatement = std::format(
				"select confKey from MMS_Conf_SRTChannel "
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
					"No SRT Channel found"
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
				"update MMS_Conf_SRTChannel set outputIndex = NULL, reservedByIngestionJobKey = NULL "
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
				sqlWhere += std::format("and LOWER(hc.label) like LOWER({}) ", trans.transaction->quote(std::format("%{}%", label)));
			else
				sqlWhere += std::format("and LOWER(hc.label) = LOWER({}) ", trans.transaction->quote(label));
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

		// 2023-02-01: scenario in cui è rimasto un reservedByIngestionJobKey in MMS_Conf_HLSChannel
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
