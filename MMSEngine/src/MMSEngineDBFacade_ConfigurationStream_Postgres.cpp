
#include "FFMpegWrapper.h"
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <FFMpegWrapper.h>
#include <cstdint>

json MMSEngineDBFacade::addStream(
	int64_t workspaceKey, string label, string sourceType, int64_t encodersPoolKey, string url, string pushProtocol, int64_t pushEncoderKey,
	bool pushPublicEncoderName, // indica se deve essere usato l'encoder pubblico (true) o quello privato (false)
	int pushServerPort, string pushUri, int pushListenTimeout, int captureLiveVideoDeviceNumber, string captureLiveVideoInputFormat,
	int captureLiveFrameRate, int captureLiveWidth, int captureLiveHeight, int captureLiveAudioDeviceNumber, int captureLiveChannelsNumber,
	int64_t tvSourceTVConfKey, string type, string description, string name, string region, string country, int64_t imageMediaItemKey,
	string imageUniqueName, int position, json userData
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
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_Stream(workspaceKey, label, sourceType, "
				"encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushPublicEncoderName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, captureLiveVideoInputFormat, "
				"captureLiveFrameRate, captureLiveWidth, captureLiveHeight, "
				"captureLiveAudioDeviceNumber, captureLiveChannelsNumber, "
				"tvSourceTVConfKey, "
				"type, description, name, region, country, imageMediaItemKey, imageUniqueName, "
				"position, userData) values ("
				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				workspaceKey, trans.transaction->quote(label), trans.transaction->quote(sourceType),
				encodersPoolKey == -1 ? "null" : to_string(encodersPoolKey), url == "" ? "null" : trans.transaction->quote(url),
				pushProtocol == "" ? "null" : trans.transaction->quote(pushProtocol), pushEncoderKey == -1 ? "null" : to_string(pushEncoderKey),
				pushEncoderKey == -1 ? "null" : to_string(pushPublicEncoderName), pushServerPort == -1 ? "null" : to_string(pushServerPort),
				pushUri == "" ? "null" : trans.transaction->quote(pushUri), pushListenTimeout == -1 ? "null" : to_string(pushListenTimeout),
				captureLiveVideoDeviceNumber == -1 ? "null" : to_string(captureLiveVideoDeviceNumber),
				captureLiveVideoInputFormat == "" ? "null" : trans.transaction->quote(captureLiveVideoInputFormat),
				captureLiveFrameRate == -1 ? "null" : to_string(captureLiveFrameRate), captureLiveWidth == -1 ? "null" : to_string(captureLiveWidth),
				captureLiveHeight == -1 ? "null" : to_string(captureLiveHeight),
				captureLiveAudioDeviceNumber == -1 ? "null" : to_string(captureLiveAudioDeviceNumber),
				captureLiveChannelsNumber == -1 ? "null" : to_string(captureLiveChannelsNumber),
				tvSourceTVConfKey == -1 ? "null" : to_string(tvSourceTVConfKey), type == "" ? "null" : trans.transaction->quote(type),
				description == "" ? "null" : trans.transaction->quote(description), name == "" ? "null" : trans.transaction->quote(name),
				region == "" ? "null" : trans.transaction->quote(region), country == "" ? "null" : trans.transaction->quote(country),
				imageMediaItemKey == -1 ? "null" : to_string(imageMediaItemKey),
				imageUniqueName == "" ? "null" : trans.transaction->quote(imageUniqueName), position == -1 ? "null" : to_string(position),
				userData == nullptr ? "null" : trans.transaction->quote(JSONUtils::toString(userData))
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

		json streamsRoot;
		{
			int start = 0;
			int rows = 1;
			string label;
			bool labelLike = true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			json streamListRoot =
				getStreamList(trans, workspaceKey, confKey, start, rows, label, labelLike, url, sourceType, type, name, region, country, labelOrder);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = "Wrong streams";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return streamsRoot[0];
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

json MMSEngineDBFacade::modifyStream(
	int64_t confKey, string labelKey, int64_t workspaceKey, bool labelToBeModified, string label,

	bool sourceTypeToBeModified, string sourceType, bool encodersPoolKeyToBeModified, int64_t encodersPoolKey, bool urlToBeModified, string url,
	bool pushProtocolToBeModified, string pushProtocol, bool pushEncoderKeyToBeModified, int64_t pushEncoderKey,
	bool pushPublicEncoderNameToBeModified, bool pushPublicEncoderName, bool pushServerPortToBeModified, int pushServerPort, bool pushUriToBeModified,
	string pushUri, bool pushListenTimeoutToBeModified, int pushListenTimeout, bool captureLiveVideoDeviceNumberToBeModified,
	int captureLiveVideoDeviceNumber, bool captureLiveVideoInputFormatToBeModified, string captureLiveVideoInputFormat,
	bool captureLiveFrameRateToBeModified, int captureLiveFrameRate, bool captureLiveWidthToBeModified, int captureLiveWidth,
	bool captureLiveHeightToBeModified, int captureLiveHeight, bool captureLiveAudioDeviceNumberToBeModified, int captureLiveAudioDeviceNumber,
	bool captureLiveChannelsNumberToBeModified, int captureLiveChannelsNumber, bool tvSourceTVConfKeyToBeModified, int64_t tvSourceTVConfKey,

	bool typeToBeModified, string type, bool descriptionToBeModified, string description, bool nameToBeModified, string name, bool regionToBeModified,
	string region, bool countryToBeModified, string country, bool imageToBeModified, int64_t imageMediaItemKey, string imageUniqueName,
	bool positionToBeModified, int position, bool userDataToBeModified, json userData
)
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
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = " + trans.transaction->quote(label));
				oneParameterPresent = true;
			}

			if (sourceTypeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("sourceType = " + trans.transaction->quote(sourceType));
				oneParameterPresent = true;
			}

			if (encodersPoolKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("encodersPoolKey = " + (encodersPoolKey == -1 ? "null" : to_string(encodersPoolKey)));
				oneParameterPresent = true;
			}

			if (urlToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("url = " + (url == "" ? "null" : trans.transaction->quote(url)));
				oneParameterPresent = true;
			}

			if (pushProtocolToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushProtocol = " + (pushProtocol == "" ? "null" : trans.transaction->quote(pushProtocol)));
				oneParameterPresent = true;
			}

			if (pushEncoderKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushEncoderKey = " + (pushEncoderKey == -1 ? "null" : to_string(pushEncoderKey)));
				oneParameterPresent = true;
			}

			if (pushPublicEncoderNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushPublicEncoderName = " + to_string(pushPublicEncoderName));
				oneParameterPresent = true;
			}

			if (pushServerPortToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushServerPort = " + (pushServerPort == -1 ? "null" : to_string(pushServerPort)));
				oneParameterPresent = true;
			}

			if (pushUriToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushUri = " + (pushUri == "" ? "null" : trans.transaction->quote(pushUri)));
				oneParameterPresent = true;
			}

			if (pushListenTimeoutToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("pushListenTimeout = " + (pushListenTimeout == -1 ? "null" : to_string(pushListenTimeout)));
				oneParameterPresent = true;
			}

			if (captureLiveVideoDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL +=
					("captureLiveVideoDeviceNumber = " + (captureLiveVideoDeviceNumber == -1 ? "null" : to_string(captureLiveVideoDeviceNumber)));
				oneParameterPresent = true;
			}

			if (captureLiveVideoInputFormatToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL +=
					("captureLiveVideoInputFormat = " +
					 (captureLiveVideoInputFormat == "" ? "null" : trans.transaction->quote(captureLiveVideoInputFormat)));
				oneParameterPresent = true;
			}

			if (captureLiveFrameRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveFrameRate = " + (captureLiveFrameRate == -1 ? "null" : to_string(captureLiveFrameRate)));
				oneParameterPresent = true;
			}

			if (captureLiveWidthToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveWidth = " + (captureLiveWidth == -1 ? "null" : to_string(captureLiveWidth)));
				oneParameterPresent = true;
			}

			if (captureLiveHeightToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveHeight = " + (captureLiveHeight == -1 ? "null" : to_string(captureLiveHeight)));
				oneParameterPresent = true;
			}

			if (captureLiveAudioDeviceNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL +=
					("captureLiveAudioDeviceNumber = " + (captureLiveAudioDeviceNumber == -1 ? "null" : to_string(captureLiveAudioDeviceNumber)));
				oneParameterPresent = true;
			}

			if (captureLiveChannelsNumberToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("captureLiveChannelsNumber = " + (captureLiveChannelsNumber == -1 ? "null" : to_string(captureLiveChannelsNumber)));
				oneParameterPresent = true;
			}

			if (tvSourceTVConfKeyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("tvSourceTVConfKey = " + (tvSourceTVConfKey == -1 ? "null" : to_string(tvSourceTVConfKey)));
				oneParameterPresent = true;
			}

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = " + (type == "" ? "null" : trans.transaction->quote(type)));
				oneParameterPresent = true;
			}

			if (descriptionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("description = " + (description == "" ? "null" : trans.transaction->quote(description)));
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = " + (name == "" ? "null" : trans.transaction->quote(name)));
				oneParameterPresent = true;
			}

			if (regionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("region = " + (region == "" ? "null" : trans.transaction->quote(region)));
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = " + (country == "" ? "null" : trans.transaction->quote(country)));
				oneParameterPresent = true;
			}

			if (imageToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				if (imageMediaItemKey == -1)
					setSQL +=
						("imageMediaItemKey = null, imageUniqueName = " + (imageUniqueName == "" ? "null" : trans.transaction->quote(imageUniqueName))
						);
				else
					setSQL += ("imageMediaItemKey = " + to_string(imageMediaItemKey) + ", imageUniqueName = null");
				oneParameterPresent = true;
			}

			if (positionToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("position = " + (position == -1 ? "null" : to_string(position)));
				oneParameterPresent = true;
			}

			if (userDataToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("userData = " + (userData == nullptr ? "null" : trans.transaction->quote(JSONUtils::toString(userData))));
				oneParameterPresent = true;
			}

			if (!oneParameterPresent)
			{
				string errorMessage = std::format(
					"Wrong input, no parameters to be updated"
					", confKey: {}"
					", oneParameterPresent: {}",
					confKey, oneParameterPresent
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sqlStatement = std::format(
				"update MMS_Conf_Stream {} "
				"where {} = {} and workspaceKey = {} ",
				setSQL, confKey != -1 ? "confKey" : "label", confKey != -1 ? to_string(confKey) : trans.transaction->quote(label), workspaceKey
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

		json streamsRoot;
		{
			int start = 0;
			int rows = 1;
			// string label;
			// se viene usata la labelKey come chiave per la modifica, labelLike sarà false
			bool labelLike = confKey == -1 ? false : true;
			string url;
			string sourceType;
			string type;
			string name;
			string region;
			string country;
			string labelOrder;
			json streamListRoot = getStreamList(
				trans, workspaceKey, confKey, start, rows, labelKey, labelLike, url, sourceType, type, name, region, country, labelOrder
			);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(streamListRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json responseRoot = streamListRoot[field];

			field = "streams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			streamsRoot = responseRoot[field];

			if (streamsRoot.size() != 1)
			{
				string errorMessage = "Wrong streams";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return streamsRoot[0];
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

void MMSEngineDBFacade::removeStream(int64_t workspaceKey, int64_t confKey, string label)
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
		if (confKey == -1 && label == "")
		{
			string errorMessage = std::format(
				"Wrong input"
				", confKey: {}"
				", label: {}",
				confKey, label
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			string sqlStatement;
			if (confKey != -1)
				sqlStatement = std::format("delete from MMS_Conf_Stream where confKey = {} and workspaceKey = {} ", confKey, workspaceKey);
			else
				sqlStatement =
					std::format("delete from MMS_Conf_Stream where label = {} and workspaceKey = {} ", trans.transaction->quote(label), workspaceKey);
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
					", label: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					confKey, label, rowsUpdated, sqlStatement
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

json MMSEngineDBFacade::getStreamList(
	int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label, bool labelLike, string url, string sourceType, string type,
	string name, string region, string country,
	string labelOrder, // "" or "asc" or "desc"
	bool fromMaster
)
{
	json streamListRoot;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		streamListRoot = MMSEngineDBFacade::getStreamList(
			trans, workspaceKey, liveURLKey, start, rows, label, labelLike, url, sourceType, type, name, region, country, labelOrder
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

	return streamListRoot;
}

json MMSEngineDBFacade::getStreamList(
	PostgresConnTrans &trans, int64_t workspaceKey, int64_t liveURLKey, int start, int rows, string label, bool labelLike, string url,
	string sourceType, string type, string name, string region, string country,
	string labelOrder, // "" or "asc" or "desc"
	bool fromMaster

)
{
	json streamListRoot;

	try
	{
		string field;

		SPDLOG_INFO(
			"getStreamList"
			", workspaceKey: {}"
			", liveURLKey: {}"
			", start: {}"
			", rows: {}"
			", label: {}"
			", labelLike: {}"
			", url: {}"
			", sourceType: {}"
			", type: {}"
			", name: {}"
			", region: {}"
			", country: {}"
			", labelOrder: {}",
			workspaceKey, liveURLKey, start, rows, label, labelLike, url, sourceType, type, name, region, country, labelOrder
		);

		{
			json requestParametersRoot;

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}

			if (liveURLKey != -1)
			{
				field = "liveURLKey";
				requestParametersRoot[field] = liveURLKey;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			if (!label.empty())
			{
				field = "label";
				requestParametersRoot[field] = label;
			}

			{
				field = "labelLike";
				requestParametersRoot[field] = labelLike;
			}

			if (!url.empty())
			{
				field = "url";
				requestParametersRoot[field] = url;
			}

			if (!sourceType.empty())
			{
				field = "sourceType";
				requestParametersRoot[field] = sourceType;
			}

			if (!type.empty())
			{
				field = "type";
				requestParametersRoot[field] = type;
			}

			if (!name.empty())
			{
				field = "name";
				requestParametersRoot[field] = name;
			}

			if (!region.empty())
			{
				field = "region";
				requestParametersRoot[field] = region;
			}

			if (!country.empty())
			{
				field = "country";
				requestParametersRoot[field] = country;
			}

			if (!labelOrder.empty())
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

			field = "requestParameters";
			streamListRoot[field] = requestParametersRoot;
		}

		string sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (liveURLKey != -1)
			sqlWhere += std::format("and confKey = {} ", liveURLKey);
		if (!label.empty())
		{
			if (labelLike)
				sqlWhere += std::format("and LOWER(label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
			else
				sqlWhere += std::format("and LOWER(label) = LOWER({}) ", trans.transaction->quote(label));
		}
		if (!url.empty())
			sqlWhere += std::format("and url like {} ", trans.transaction->quote("%" + url + "%"));
		if (!sourceType.empty())
			sqlWhere += std::format("and sourceType = {} ", trans.transaction->quote(sourceType));
		if (!type.empty())
			sqlWhere += std::format("and type = {} ", trans.transaction->quote(type));
		if (!name.empty())
			sqlWhere += std::format("and LOWER(name) like LOWER({}) ", trans.transaction->quote("%" + name + "%"));
		if (!region.empty())
			sqlWhere += std::format("and region like {} ", trans.transaction->quote("%" + region + "%"));
		if (!country.empty())
			sqlWhere += std::format("and LOWER(country) like LOWER({}) ", trans.transaction->quote("%" + country + "%"));

		SPDLOG_ERROR("sqlWhere: {}", sqlWhere);
		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_Stream {}", sqlWhere);
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

		json streamsRoot = json::array();
		{
			string orderByCondition;
			if (!labelOrder.empty())
				orderByCondition = std::format("order by label {}", labelOrder);

			string sqlStatement = std::format(
				"select confKey, label, sourceType, encodersPoolKey, url, "
				"pushProtocol, pushEncoderKey, pushPublicEncoderName, pushServerPort, pushUri, "
				"pushListenTimeout, captureLiveVideoDeviceNumber, "
				"captureLiveVideoInputFormat, captureLiveFrameRate, captureLiveWidth, "
				"captureLiveHeight, captureLiveAudioDeviceNumber, "
				"captureLiveChannelsNumber, tvSourceTVConfKey, "
				"type, description, name, "
				"region, country, "
				"imageMediaItemKey, imageUniqueName, position, userData "
				"from MMS_Conf_Stream {} {} "
				"limit {} offset {}",
				sqlWhere, orderByCondition, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			chrono::milliseconds internalSqlDuration(0);
			for (auto row : res)
			{
				json streamRoot;

				auto confKey = row["confKey"].as<int64_t>();
				field = "confKey";
				streamRoot[field] = confKey;

				field = "label";
				streamRoot[field] = row["label"].as<string>();

				auto sourceType = row["sourceType"].as<string>();
				field = "sourceType";
				streamRoot[field] = sourceType;

				field = "encodersPoolKey";
				if (row["encodersPoolKey"].is_null())
					streamRoot[field] = nullptr;
				else
				{
					auto encodersPoolKey = row["encodersPoolKey"].as<int64_t>();
					streamRoot[field] = encodersPoolKey;

					if (encodersPoolKey >= 0)
					{
						try
						{
							chrono::milliseconds sqlDuration(0);
							string encodersPoolLabel = getEncodersPoolDetails(encodersPoolKey, &sqlDuration);
							internalSqlDuration += sqlDuration;

							field = "encodersPoolLabel";
							streamRoot[field] = encodersPoolLabel;
						}
						catch (exception &e)
						{
							SPDLOG_ERROR(
								"getStreamList. getEncodersPoolDetails failed"
								", confKey: {}"
								", encodersPoolKey: {}",
								confKey, encodersPoolKey
							);
						}
					}
				}

				// if (sourceType == "IP_PULL")
				{
					field = "url";
					if (row["url"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["url"].as<string>();
				}
				// else if (sourceType == "IP_PUSH")
				{
					field = "pushProtocol";
					if (row["pushProtocol"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["pushProtocol"].as<string>();

					bool pushPublicEncoderName = false;
					if (!row["pushPublicEncoderName"].is_null())
						pushPublicEncoderName = row["pushPublicEncoderName"].as<bool>();

					field = "pushEncoderKey";
					if (row["pushEncoderKey"].is_null())
					{
						streamRoot[field] = nullptr;

						field = "pushPublicEncoderName";
						streamRoot[field] = nullptr;

						field = "pushEncoderLabel";
						streamRoot[field] = nullptr;

						field = "pushEncoderName";
						streamRoot[field] = nullptr;
					}
					else
					{
						auto pushEncoderKey = row["pushEncoderKey"].as<int64_t>();
						streamRoot[field] = pushEncoderKey;

						if (pushEncoderKey >= 0)
						{
							try
							{
								field = "pushPublicEncoderName";
								streamRoot[field] = pushPublicEncoderName;

								chrono::milliseconds sqlDuration(0);
								auto [pushEncoderLabel, publicServerName, internalServerName] = // getEncoderDetails(pushEncoderKey);
									encoder_LabelPublicServerNameInternalServerName(pushEncoderKey, fromMaster, &sqlDuration);
								internalSqlDuration += sqlDuration;

								field = "pushEncoderLabel";
								streamRoot[field] = pushEncoderLabel;

								field = "pushEncoderName";
								if (pushPublicEncoderName)
									streamRoot[field] = publicServerName;
								else
									streamRoot[field] = internalServerName;
							}
							catch (exception &e)
							{
								SPDLOG_ERROR(
									"getStreamList. getEncoderDetails failed"
									", pushEncoderKey: {}"
									", exception: {}",
									pushEncoderKey, e.what()
								);
							}
						}
					}

					field = "pushServerPort";
					if (row["pushServerPort"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["pushServerPort"].as<int>();

					field = "pushUri";
					if (row["pushUri"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["pushUri"].as<string>();

					field = "pushListenTimeout";
					if (row["pushListenTimeout"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["pushListenTimeout"].as<int>();
				}
				// else if (sourceType == "CaptureLive")
				{
					field = "captureLiveVideoDeviceNumber";
					if (row["captureLiveVideoDeviceNumber"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveVideoDeviceNumber"].as<int>();

					field = "captureLiveVideoInputFormat";
					if (row["captureLiveVideoInputFormat"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveVideoInputFormat"].as<string>();

					field = "captureLiveFrameRate";
					if (row["captureLiveFrameRate"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveFrameRate"].as<int>();

					field = "captureLiveWidth";
					if (row["captureLiveWidth"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveWidth"].as<int>();

					field = "captureLiveHeight";
					if (row["captureLiveHeight"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveHeight"].as<int>();

					field = "captureLiveAudioDeviceNumber";
					if (row["captureLiveAudioDeviceNumber"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveAudioDeviceNumber"].as<int>();

					field = "captureLiveChannelsNumber";
					if (row["captureLiveChannelsNumber"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["captureLiveChannelsNumber"].as<int>();
				}
				// else if (sourceType == "TV")
				{
					field = "tvSourceTVConfKey";
					if (row["tvSourceTVConfKey"].is_null())
						streamRoot[field] = nullptr;
					else
						streamRoot[field] = row["tvSourceTVConfKey"].as<int64_t>();
				}

				field = "type";
				if (row["type"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["type"].as<string>();

				field = "description";
				if (row["description"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["description"].as<string>();

				field = "name";
				if (row["name"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["name"].as<string>();

				field = "region";
				if (row["region"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["region"].as<string>();

				field = "country";
				if (row["country"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["country"].as<string>();

				field = "imageMediaItemKey";
				if (row["imageMediaItemKey"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["imageMediaItemKey"].as<int64_t>();

				field = "imageUniqueName";
				if (row["imageUniqueName"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["imageUniqueName"].as<string>();

				field = "position";
				if (row["position"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["position"].as<int>();

				field = "userData";
				if (row["userData"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["userData"].as<string>();

				streamsRoot.push_back(streamRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count();
			// per questa query abbiamo l'indice ma recupera tanti dati e prende un po piu di tempo
			SQLQUERYLOG(
				"getStreamList", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@getStreamList@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		field = "streams";
		responseRoot[field] = streamsRoot;

		field = "response";
		streamListRoot[field] = responseRoot;
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

	return streamListRoot;
}

json MMSEngineDBFacade::getStreamFreePushEncoderPort(int64_t encoderKey, bool fromMaster)
{
	json streamFreePushEncoderPortRoot;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		int freePushEncoderPort = 30000;
		{
			// si vuole recuperare la prima pushserverport di un encoder non configurata tra tutti gli streams di tutti i workspace
			// Per questo motivo non bisogna aggiungere la condizione del workspace key
			string sqlStatement = std::format(
				"select max(pushServerPort) as pushServerPort "
				"from MMS_Conf_Stream "
				"where sourceType = {} and pushEncoderKey = {}",
				trans.transaction->quote("IP_PUSH"), encoderKey
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
				freePushEncoderPort = res[0]["pushServerPort"].as<int>();
				freePushEncoderPort++;
			}
		}

		streamFreePushEncoderPortRoot["freePushEncoderPort"] = freePushEncoderPort;
		return streamFreePushEncoderPortRoot;
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

tuple<int64_t, string, string, string, int64_t, bool, int, int, string, int, int, int, int, int, int64_t>
MMSEngineDBFacade::stream_aLot(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_conf_stream:.confkey",						 // 0
			"mms_conf_stream:.sourcetype",					 // 1
			"mms_conf_stream:.encoderspoolkey",				 // 2
			"mms_conf_stream:.url",							 // 3
			"mms_conf_stream:.pushEncoderKey",				 // 4
			"mms_conf_stream:.pushPublicEncoderName",		 // 5
			"mms_conf_stream:.pushListenTimeout",			 // 6
			"mms_conf_stream:.captureLiveVideoDeviceNumber", // 7
			"mms_conf_stream:.captureLiveVideoInputFormat",	 // 8
			"mms_conf_stream:.captureLiveFrameRate",		 // 9
			"mms_conf_stream:.captureLiveWidth",			 // 10
			"mms_conf_stream:.captureLiveHeight",			 // 11
			"mms_conf_stream:.captureLiveAudioDeviceNumber", // 12
			"mms_conf_stream:.captureLiveChannelsNumber",	 // 13
			"mms_conf_stream:.tvSourceTVConfKey"			 // 14
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		string encodersPoolLabel;
		{
			int64_t encodersPoolKey = (*sqlResultSet)[0][2].as<int64_t>(-1);
			if (encodersPoolKey != -1)
			{
				try
				{
					encodersPoolLabel = getEncodersPoolDetails(encodersPoolKey);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"getEncodersPoolDetails failed"
						", encodersPoolKey: {}",
						encodersPoolKey
					);
				}
			}
		}
		return make_tuple(
			(*sqlResultSet)[0][0].as<int64_t>(-1), (*sqlResultSet)[0][1].as<string>(""), encodersPoolLabel, (*sqlResultSet)[0][3].as<string>(""),
			(*sqlResultSet)[0][4].as<int64_t>(-1), (*sqlResultSet)[0][5].as<bool>(false), (*sqlResultSet)[0][6].as<int>(-1),
			(*sqlResultSet)[0][7].as<int>(-1), (*sqlResultSet)[0][8].as<string>(""), (*sqlResultSet)[0][9].as<int>(-1),
			(*sqlResultSet)[0][10].as<int>(-1), (*sqlResultSet)[0][11].as<int>(-1), (*sqlResultSet)[0][12].as<int>(-1),
			(*sqlResultSet)[0][13].as<int>(-1), (*sqlResultSet)[0][14].as<int64_t>(-1)
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}

tuple<string, string, int64_t, bool, int, string> MMSEngineDBFacade::stream_pushInfo(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_conf_stream:.sourceType",			  // 0
			"mms_conf_stream:.pushProtocol",		  // 1
			"mms_conf_stream:.pushEncoderKey",		  // 2
			"mms_conf_stream:.pushPublicEncoderName", // 3
			"mms_conf_stream:.pushServerPort",		  // 4
			"mms_conf_stream:.pushUri"				  // 5
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		return make_tuple(
			(*sqlResultSet)[0][0].as<string>(""), (*sqlResultSet)[0][1].as<string>(""), (*sqlResultSet)[0][2].as<int64_t>(-1),
			(*sqlResultSet)[0][3].as<bool>(false), (*sqlResultSet)[0][4].as<int>(-1), (*sqlResultSet)[0][5].as<string>("")
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}

string MMSEngineDBFacade::stream_columnAsString(int64_t workspaceKey, string columnName, int64_t confKey, string label)
{
	try
	{
		string requestedColumn = std::format("mms_conf_stream:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, confKey, label);

		return (*sqlResultSet)[0][0].as<string>(string());
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", confKey: {}",
			workspaceKey, confKey
		);

		throw e;
	}
}

/*
string MMSEngineDBFacade::stream_pushProtocol(int64_t workspaceKey, int64_t confKey)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_conf_stream:.pushProtocol" // 0
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, confKey);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", confKey: {}",
			workspaceKey, confKey
		);

		throw e;
	}
}
*/

int64_t MMSEngineDBFacade::stream_columnAsInt64(int64_t workspaceKey, string columnName, int64_t confKey, string label)
{
	try
	{
		string requestedColumn = std::format("mms_conf_stream:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, confKey, label);

		return (*sqlResultSet)[0][0].as<int64_t>(static_cast<int64_t>(-1));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", confKey: {}",
			workspaceKey, confKey
		);

		throw e;
	}
}

/*
int64_t MMSEngineDBFacade::stream_confKey(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {"mms_conf_stream:.confkey"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		return (*sqlResultSet)[0][0].as<int64_t>(-1);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}
*/

/*
string MMSEngineDBFacade::stream_sourceType(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {"mms_conf_stream:.sourcetype"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		return (*sqlResultSet)[0][0].as<string>("null source type!!!");
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}
*/

tuple<string, string, int64_t, bool>
MMSEngineDBFacade::stream_sourceTypeEncodersPoolPushEncoderKeyPushPublicEncoderName(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_conf_stream:.sourceType",			  // 0
			"mms_conf_stream:.pushEncoderKey",		  // 1
			"mms_conf_stream:.pushPublicEncoderName", // 2
			"mms_conf_stream:.encoderspoolkey"		  // 3
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		string encodersPoolLabel;
		{
			int64_t encodersPoolKey = (*sqlResultSet)[0][3].as<int64_t>(-1);
			if (encodersPoolKey != -1)
			{
				try
				{
					encodersPoolLabel = getEncodersPoolDetails(encodersPoolKey);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"getEncodersPoolDetails failed"
						", encodersPoolKey: {}",
						encodersPoolKey
					);
				}
			}
		}

		return make_tuple(
			(*sqlResultSet)[0][0].as<string>(""), encodersPoolLabel, (*sqlResultSet)[0][1].as<int64_t>(-1), (*sqlResultSet)[0][2].as<bool>(false)
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}

pair<string, string> MMSEngineDBFacade::stream_sourceTypeUrl(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {"mms_conf_stream:.sourceType", "mms_conf_stream:.url"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		return make_pair((*sqlResultSet)[0][0].as<string>("null source type!!!"), (*sqlResultSet)[0][1].as<string>("null url!!!"));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}

tuple<int64_t, string, string> MMSEngineDBFacade::stream_confKeySourceTypeUrl(int64_t workspaceKey, string label)
{
	try
	{
		vector<string> requestedColumns = {"mms_conf_stream:.confKey", "mms_conf_stream:.sourceType", "mms_conf_stream:.url"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, -1, label);

		return make_tuple(
			(*sqlResultSet)[0][0].as<int64_t>(-1), (*sqlResultSet)[0][1].as<string>("null source type!!!"),
			(*sqlResultSet)[0][2].as<string>("null url!!!")
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", exceptionMessage: {}",
			workspaceKey, label, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}",
			workspaceKey, label
		);

		throw e;
	}
}

json MMSEngineDBFacade::stream_columnAsJson(int64_t workspaceKey, string columnName, int64_t confKey, string label)
{
	try
	{
		string requestedColumn = std::format("mms_conf_stream:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, confKey, label);

		return sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<json>(json()) : json();
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", confKey: {}",
			workspaceKey, confKey
		);

		throw e;
	}
}

/*
json MMSEngineDBFacade::stream_userData(int64_t workspaceKey, int64_t confKey)
{
	try
	{
		vector<string> requestedColumns = {"mms_conf_stream:.userData"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = streamQuery(requestedColumns, workspaceKey, confKey);

		return JSONUtils::toJson((*sqlResultSet)[0][0].as<string>("null source type!!!"));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", confKey: {}"
			", exceptionMessage: {}",
			workspaceKey, confKey, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", confKey: {}",
			workspaceKey, confKey
		);

		throw e;
	}
}
*/

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::streamQuery(
	vector<string> &requestedColumns, int64_t workspaceKey, int64_t confKey, string label, bool fromMaster, int startIndex, int rows, string orderBy,
	bool notFoundAsException
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/
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
			string errorMessage = std::format(
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
			where += std::format("{} workspacekey = {} ", where.size() > 0 ? "and" : "", workspaceKey);
			if (confKey != -1)
				where += std::format("{} confkey = {} ", where.size() > 0 ? "and" : "", confKey);
			if (label != "")
				where += std::format("{} label = {} ", where.size() > 0 ? "and" : "", trans.transaction->quote(label));

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_Conf_Stream "
				"where "
				"{} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (notFoundAsException && empty(res) && (confKey != -1 || label != ""))
			{
				string errorMessage = std::format(
					"Stream not found"
					", workspaceKey: {}"
					", label: {}",
					workspaceKey, label
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
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

json MMSEngineDBFacade::addSourceTVStream(
	string type, int64_t serviceId, int64_t networkId, int64_t transportStreamId, string name, string satellite, int64_t frequency, string lnb,
	int videoPid, string audioPids, int audioItalianPid, int audioEnglishPid, int teletextPid, string modulation, string polarization,
	int64_t symbolRate, int64_t bandwidthInHz, string country, string deliverySystem
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
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"insert into MMS_Conf_SourceTVStream( "
				"type, serviceId, networkId, transportStreamId, "
				"name, satellite, frequency, lnb, "
				"videoPid, audioPids, audioItalianPid, audioEnglishPid, teletextPid, "
				"modulation, polarization, symbolRate, bandwidthInHz, "
				"country, deliverySystem "
				") values ({}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
				"{}, {}, {}, {}, {}, {}, {}, {}, {}) returning confKey",
				trans.transaction->quote(type), serviceId == -1 ? "null" : to_string(serviceId), networkId == -1 ? "null" : to_string(networkId),
				transportStreamId == -1 ? "null" : to_string(transportStreamId), trans.transaction->quote(name),
				satellite == "" ? "null" : trans.transaction->quote(satellite), frequency, lnb == "" ? "null" : trans.transaction->quote(lnb),
				videoPid == -1 ? "null" : to_string(videoPid), audioPids == "" ? "null" : trans.transaction->quote(audioPids),
				audioItalianPid == -1 ? "null" : to_string(audioItalianPid), audioEnglishPid == -1 ? "null" : to_string(audioEnglishPid),
				teletextPid == -1 ? "null" : to_string(teletextPid), modulation == "" ? "null" : trans.transaction->quote(modulation),
				polarization == "" ? "null" : trans.transaction->quote(polarization), symbolRate == -1 ? "null" : to_string(symbolRate),
				bandwidthInHz == -1 ? "null" : to_string(bandwidthInHz), country == "" ? "null" : trans.transaction->quote(country),
				deliverySystem == "" ? "null" : trans.transaction->quote(deliverySystem)
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

		json sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId = -1;
			string name;
			int64_t frequency = -1;
			string lnb;
			int videoPid = -1;
			string audioPids;
			string nameOrder;
			json sourceTVStreamRoot =
				getSourceTVStreamList(confKey, start, rows, type, serviceId, name, frequency, lnb, videoPid, audioPids, nameOrder, true);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = std::format(
					"Wrong channelConf"
					", confKey: {}"
					", sourceTVStreamsRoot.size: {}",
					confKey, sourceTVStreamsRoot.size()
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return sourceTVStreamsRoot[0];
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

json MMSEngineDBFacade::modifySourceTVStream(
	int64_t confKey,

	bool typeToBeModified, string type, bool serviceIdToBeModified, int64_t serviceId, bool networkIdToBeModified, int64_t networkId,
	bool transportStreamIdToBeModified, int64_t transportStreamId, bool nameToBeModified, string name, bool satelliteToBeModified, string satellite,
	bool frequencyToBeModified, int64_t frequency, bool lnbToBeModified, string lnb, bool videoPidToBeModified, int videoPid,
	bool audioPidsToBeModified, string audioPids, bool audioItalianPidToBeModified, int audioItalianPid, bool audioEnglishPidToBeModified,
	int audioEnglishPid, bool teletextPidToBeModified, int teletextPid, bool modulationToBeModified, string modulation, bool polarizationToBeModified,
	string polarization, bool symbolRateToBeModified, int64_t symbolRate, bool bandwidthInHzToBeModified, int64_t bandwidthInHz,
	bool countryToBeModified, string country, bool deliverySystemToBeModified, string deliverySystem
)
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
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (typeToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("type = " + (type == "" ? "null" : trans.transaction->quote(type)));
				oneParameterPresent = true;
			}

			if (serviceIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("serviceId = " + (serviceId == -1 ? "null" : to_string(serviceId)));
				oneParameterPresent = true;
			}

			if (networkIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("networkId = " + (networkId == -1 ? "null" : to_string(networkId)));
				oneParameterPresent = true;
			}

			if (transportStreamIdToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("transportStreamId = " + (transportStreamId == -1 ? "null" : to_string(transportStreamId)));
				oneParameterPresent = true;
			}

			if (nameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("name = " + (name == "" ? "null" : trans.transaction->quote(name)));
				oneParameterPresent = true;
			}

			if (satelliteToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("satellite = " + (satellite == "" ? "null" : trans.transaction->quote(satellite)));
				oneParameterPresent = true;
			}

			if (frequencyToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("frequency = " + (frequency == -1 ? "null" : to_string(frequency)));
				oneParameterPresent = true;
			}

			if (lnbToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("lnb = " + (lnb == "" ? "null" : trans.transaction->quote(lnb)));
				oneParameterPresent = true;
			}

			if (videoPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("videoPid = " + (videoPid == -1 ? "null" : to_string(videoPid)));
				oneParameterPresent = true;
			}

			if (audioPidsToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioPids = " + (audioPids == "" ? "null" : trans.transaction->quote(audioPids)));
				oneParameterPresent = true;
			}

			if (audioItalianPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioItalianPid = " + (audioItalianPid == -1 ? "null" : to_string(audioItalianPid)));
				oneParameterPresent = true;
			}

			if (audioEnglishPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("audioEnglishPid = " + (audioEnglishPid == -1 ? "null" : to_string(audioEnglishPid)));
				oneParameterPresent = true;
			}

			if (teletextPidToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("teletextPid = " + (teletextPid == -1 ? "null" : to_string(teletextPid)));
				oneParameterPresent = true;
			}

			if (modulationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("modulation = " + (modulation == "" ? "null" : trans.transaction->quote(modulation)));
				oneParameterPresent = true;
			}

			if (polarizationToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("polarization = " + (polarization == "" ? "null" : trans.transaction->quote(polarization)));
				oneParameterPresent = true;
			}

			if (symbolRateToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("symbolRate = " + (symbolRate == -1 ? "null" : to_string(symbolRate)));
				oneParameterPresent = true;
			}

			if (bandwidthInHzToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("bandwidthInHz = " + (bandwidthInHz == -1 ? "null" : to_string(bandwidthInHz)));
				oneParameterPresent = true;
			}

			if (countryToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("country = " + (country == "" ? "null" : trans.transaction->quote(country)));
				oneParameterPresent = true;
			}

			if (deliverySystemToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("deliverySystem = " + (deliverySystem == "" ? "null" : trans.transaction->quote(deliverySystem)));
				oneParameterPresent = true;
			}

			if (!oneParameterPresent)
			{
				string errorMessage = std::format(
					"Wrong input, no parameters to be updated"
					", confKey: {}"
					", oneParameterPresent: {}",
					confKey, oneParameterPresent
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			string sqlStatement = std::format(
				"update MMS_Conf_SourceTVStream {} "
				"where confKey = {} ",
				setSQL, confKey
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

		json sourceTVStreamsRoot;
		{
			int start = 0;
			int rows = 1;
			string type;
			int64_t serviceId = -1;
			string name;
			int64_t frequency = -1;
			string lnb;
			int videoPid = -1;
			string audioPids;
			string nameOrder;
			json sourceTVStreamRoot =
				getSourceTVStreamList(confKey, start, rows, type, serviceId, name, frequency, lnb, videoPid, audioPids, nameOrder, true);

			string field = "response";
			if (!JSONUtils::isMetadataPresent(sourceTVStreamRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			json responseRoot = sourceTVStreamRoot[field];

			field = "sourceTVStreams";
			if (!JSONUtils::isMetadataPresent(responseRoot, field))
			{
				string errorMessage = std::format(
					"Field is not present or it is null"
					", Field: {}",
					field
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
			sourceTVStreamsRoot = responseRoot[field];

			if (sourceTVStreamsRoot.size() != 1)
			{
				string errorMessage = "Wrong streams";
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		return sourceTVStreamsRoot[0];
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

void MMSEngineDBFacade::removeSourceTVStream(int64_t confKey)
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
			string sqlStatement = std::format("delete from MMS_Conf_SourceTVStream where confKey = {} ", confKey);
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

json MMSEngineDBFacade::getSourceTVStreamList(
	int64_t confKey, int start, int rows, string type, int64_t serviceId, string name, int64_t frequency, string lnb, int videoPid, string audioPids,
	string nameOrder, bool fromMaster
)
{
	json streamListRoot;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getSourceTVStreamList"
			", confKey: {}"
			", start: {}"
			", rows: {}"
			", type: {}"
			", frequency: {}"
			", lnb: {}"
			", serviceId: {}"
			", name: {}"
			", videoPid: {}"
			", audioPids: {}"
			", nameOrder: {}",
			confKey, start, rows, type, frequency, lnb, serviceId, name, videoPid, audioPids, nameOrder
		);

		{
			json requestParametersRoot;

			if (confKey != -1)
			{
				field = "confKey";
				requestParametersRoot[field] = confKey;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			if (type != "")
			{
				field = "type";
				requestParametersRoot[field] = type;
			}

			if (serviceId != -1)
			{
				field = "serviceId";
				requestParametersRoot[field] = serviceId;
			}

			if (name != "")
			{
				field = "name";
				requestParametersRoot[field] = name;
			}

			if (frequency != -1)
			{
				field = "frequency";
				requestParametersRoot[field] = frequency;
			}

			if (lnb != "")
			{
				field = "lnb";
				requestParametersRoot[field] = lnb;
			}

			if (videoPid != -1)
			{
				field = "videoPid";
				requestParametersRoot[field] = videoPid;
			}

			if (audioPids != "")
			{
				field = "audioPids";
				requestParametersRoot[field] = audioPids;
			}

			if (nameOrder != "")
			{
				field = "nameOrder";
				requestParametersRoot[field] = nameOrder;
			}

			field = "requestParameters";
			streamListRoot[field] = requestParametersRoot;
		}

		string sqlWhere;
		if (confKey != -1)
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.confKey = {} ", confKey);
			else
				sqlWhere += std::format("and sc.confKey = {} ", confKey);
		}
		if (type != "")
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.type = {} ", trans.transaction->quote(type));
			else
				sqlWhere += std::format("and sc.type = {} ", trans.transaction->quote(type));
		}
		if (serviceId != -1)
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.serviceId = {} ", serviceId);
			else
				sqlWhere += std::format("and sc.serviceId = {} ", serviceId);
		}
		if (name != "")
		{
			if (sqlWhere == "")
				sqlWhere += std::format("LOWER(sc.name) like LOWER({}) ", trans.transaction->quote("%" + name + "%"));
			else
				sqlWhere += std::format("and LOWER(sc.name) like LOWER({}) ", trans.transaction->quote("%" + name + "%"));
		}
		if (frequency != -1)
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.frequency = {} ", frequency);
			else
				sqlWhere += std::format("and sc.frequency = {} ", frequency);
		}
		if (lnb != "")
		{
			if (sqlWhere == "")
				sqlWhere += std::format("LOWER(sc.lnb) like LOWER({}) ", trans.transaction->quote("%" + lnb + "%"));
			else
				sqlWhere += std::format("and LOWER(sc.lnb) like LOWER({}) ", trans.transaction->quote("%" + lnb + "%"));
		}
		if (videoPid != -1)
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.videoPid = {} ", videoPid);
			else
				sqlWhere += std::format("and sc.videoPid = {} ", videoPid);
		}
		if (audioPids != "")
		{
			if (sqlWhere == "")
				sqlWhere += std::format("sc.audioPids = {} ", trans.transaction->quote(audioPids));
			else
				sqlWhere += std::format("and sc.audioPids = {} ", trans.transaction->quote(audioPids));
		}
		if (sqlWhere != "")
			sqlWhere = std::format("where {}", sqlWhere);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Conf_SourceTVStream sc {}", sqlWhere);
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

		json streamsRoot = json::array();
		{
			string orderByCondition;
			orderByCondition = std::format("order by sc.name {}", nameOrder);

			string sqlStatement = std::format(
				"select sc.confKey, sc.type, sc.serviceId, "
				"sc.networkId, sc.transportStreamId, sc.name, sc.satellite, "
				"sc.frequency, sc.lnb, sc.videoPid, sc.audioPids, "
				"sc.audioItalianPid, sc.audioEnglishPid, sc.teletextPid, "
				"sc.modulation, sc.polarization, sc.symbolRate, sc.bandwidthInHz, "
				"sc.country, sc.deliverySystem "
				"from MMS_Conf_SourceTVStream sc {} {} "
				"limit {} offset {}",
				sqlWhere, orderByCondition, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json streamRoot;

				field = "confKey";
				streamRoot[field] = row["confKey"].as<int64_t>();

				field = "type";
				streamRoot[field] = row["type"].as<string>();

				field = "serviceId";
				if (row["serviceId"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["serviceId"].as<int>();

				field = "networkId";
				if (row["networkId"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["networkId"].as<int>();

				field = "transportStreamId";
				if (row["transportStreamId"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["transportStreamId"].as<int>();

				field = "name";
				streamRoot[field] = row["name"].as<string>();

				field = "satellite";
				if (row["satellite"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["satellite"].as<string>();

				field = "frequency";
				streamRoot[field] = row["frequency"].as<int64_t>();

				field = "lnb";
				if (row["lnb"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["lnb"].as<string>();

				field = "videoPid";
				if (row["videoPid"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["videoPid"].as<int>();

				field = "audioPids";
				if (row["audioPids"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["audioPids"].as<string>();

				field = "audioItalianPid";
				if (row["audioItalianPid"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["audioItalianPid"].as<int>();

				field = "audioEnglishPid";
				if (row["audioEnglishPid"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["audioEnglishPid"].as<int>();

				field = "teletextPid";
				if (row["teletextPid"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["teletextPid"].as<int>();

				field = "modulation";
				if (row["modulation"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["modulation"].as<string>();

				field = "polarization";
				if (row["polarization"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["polarization"].as<string>();

				field = "symbolRate";
				if (row["symbolRate"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["symbolRate"].as<int>();

				field = "bandwidthInHz";
				if (row["bandwidthInHz"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["bandwidthInHz"].as<int>();

				field = "country";
				if (row["country"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["country"].as<string>();

				field = "deliverySystem";
				if (row["deliverySystem"].is_null())
					streamRoot[field] = nullptr;
				else
					streamRoot[field] = row["deliverySystem"].as<string>();

				streamsRoot.push_back(streamRoot);
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

		field = "sourceTVStreams";
		responseRoot[field] = streamsRoot;

		field = "response";
		streamListRoot[field] = responseRoot;
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

	return streamListRoot;
}

tuple<string, int64_t, int64_t, int64_t, int64_t, string, int, int>
MMSEngineDBFacade::getSourceTVStreamDetails(int64_t confKey, bool warningIfMissing)
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
		SPDLOG_INFO(
			"getTVStreamDetails"
			", confKey: {}",
			confKey
		);

		string type;
		int64_t serviceId;
		int64_t frequency;
		int64_t symbolRate;
		int64_t bandwidthInHz;
		string modulation;
		int videoPid;
		int audioItalianPid;
		{
			string sqlStatement = std::format(
				"select type, serviceId, frequency, symbolRate, "
				"bandwidthInHz, modulation, videoPid, audioItalianPid "
				"from MMS_Conf_SourceTVStream "
				"where confKey = {}",
				confKey
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
					"Configuration is not found"
					", confKey: {}",
					confKey
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}

			type = res[0]["type"].as<string>();
			serviceId = res[0]["serviceId"].as<int>();
			frequency = res[0]["frequency"].as<int64_t>();
			if (res[0]["symbolRate"].is_null())
				symbolRate = -1;
			else
				symbolRate = res[0]["symbolRate"].as<int>();
			if (res[0]["bandwidthInHz"].is_null())
				bandwidthInHz = -1;
			else
				bandwidthInHz = res[0]["bandwidthInHz"].as<int>();
			if (!res[0]["modulation"].is_null())
				modulation = res[0]["modulation"].as<string>();
			videoPid = res[0]["videoPid"].as<int>();
			audioItalianPid = res[0]["audioItalianPid"].as<int>();
		}

		return make_tuple(type, serviceId, frequency, symbolRate, bandwidthInHz, modulation, videoPid, audioItalianPid);
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

string MMSEngineDBFacade::getStreamingYouTubeLiveURL(shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, string liveURL)
{

	string streamingYouTubeLiveURL;

	long hoursFromLastCalculatedURL;
	pair<long, string> lastYouTubeURLDetails;
	try
	{
		lastYouTubeURLDetails = getLastYouTubeURLDetails(workspace, ingestionJobKey, confKey);

		string lastCalculatedURL;

		tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

		long retrieveStreamingYouTubeURLPeriodInHours = 5; // 5 hours

		SPDLOG_INFO(
			"check youTubeURLCalculate"
			", ingestionJobKey: {}"
			", confKey: {}"
			", hoursFromLastCalculatedURL: {}"
			", retrieveStreamingYouTubeURLPeriodInHours: {}",
			ingestionJobKey, confKey, hoursFromLastCalculatedURL, retrieveStreamingYouTubeURLPeriodInHours
		);
		if (hoursFromLastCalculatedURL < retrieveStreamingYouTubeURLPeriodInHours)
			streamingYouTubeLiveURL = lastCalculatedURL;
	}
	catch (runtime_error &e)
	{
		string errorMessage = std::format(
			"youTubeURLCalculate. getLastYouTubeURLDetails failed"
			", ingestionJobKey: {}"
			", confKey: {}"
			", YouTube URL: {}",
			ingestionJobKey, confKey, streamingYouTubeLiveURL
		);
		SPDLOG_ERROR(errorMessage);
	}

	if (streamingYouTubeLiveURL == "")
	{
		try
		{
			FFMpegWrapper ffmpeg(_configuration);
			pair<string, string> streamingLiveURLDetails = ffmpeg.retrieveStreamingYouTubeURL(ingestionJobKey, liveURL);

			tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

			SPDLOG_INFO(
				"youTubeURLCalculate. Retrieve streaming YouTube URL"
				", ingestionJobKey: {}"
				", confKey: {}"
				", initial YouTube URL: {}"
				", streaming YouTube Live URL: {}"
				", hoursFromLastCalculatedURL: {}",
				ingestionJobKey, confKey, liveURL, streamingYouTubeLiveURL, hoursFromLastCalculatedURL
			);
		}
		catch (runtime_error &e)
		{
			// in case ffmpeg.retrieveStreamingYouTubeURL fails
			// we will use the last saved URL
			tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

			string errorMessage = std::format(
				"youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
				", ingestionJobKey: {}"
				", confKey: {}"
				", YouTube URL: {}",
				ingestionJobKey, confKey, streamingYouTubeLiveURL
			);
			SPDLOG_ERROR(errorMessage);

			try
			{
				string firstLineOfErrorMessage;
				{
					string firstLine;
					stringstream ss(errorMessage);
					if (getline(ss, firstLine))
						firstLineOfErrorMessage = firstLine;
					else
						firstLineOfErrorMessage = errorMessage;
				}

				appendIngestionJobErrorMessage(ingestionJobKey, firstLineOfErrorMessage);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					"youTubeURLCalculate. appendIngestionJobErrorMessage failed"
					", ingestionJobKey: {}"
					", e.what(): {}",
					ingestionJobKey, e.what()
				);
			}

			if (streamingYouTubeLiveURL == "")
			{
				// 2020-04-21: let's go ahead because it would be managed
				// the killing of the encodingJob
				// 2020-09-17: it does not have sense to continue
				//	if we do not have the right URL (m3u8)
				throw YouTubeURLNotRetrieved();
			}
		}

		if (streamingYouTubeLiveURL != "")
		{
			try
			{
				updateChannelDataWithNewYouTubeURL(workspace, ingestionJobKey, confKey, streamingYouTubeLiveURL);
			}
			catch (runtime_error &e)
			{
				string errorMessage = std::format(
					"youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
					", ingestionJobKey: {}"
					", confKey: {}"
					", YouTube URL: {}",
					ingestionJobKey, confKey, streamingYouTubeLiveURL
				);
				SPDLOG_ERROR(errorMessage);
			}
		}
	}
	else
	{
		SPDLOG_INFO(
			"youTubeURLCalculate. Reuse a previous streaming YouTube URL"
			", ingestionJobKey: {}"
			", confKey: {}"
			", initial YouTube URL: {}"
			", streaming YouTube Live URL: {}"
			", hoursFromLastCalculatedURL: {}",
			ingestionJobKey, confKey, liveURL, streamingYouTubeLiveURL, hoursFromLastCalculatedURL
		);
	}

	return streamingYouTubeLiveURL;
}

pair<long, string> MMSEngineDBFacade::getLastYouTubeURLDetails(shared_ptr<Workspace> workspace, int64_t ingestionKey, int64_t confKey)
{
	long hoursFromLastCalculatedURL = -1;
	string lastCalculatedURL;

	try
	{
		json channelDataRoot = stream_columnAsJson(workspace->_workspaceKey, "userData", confKey);
		/*
		tuple<string, string, string> channelDetails = getStreamDetails(workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		json channelDataRoot = JSONUtils::toJson(channelData);
		*/

		string field;

		json mmsDataRoot;
		{
			field = "mmsData";
			if (!JSONUtils::isMetadataPresent(channelDataRoot, field))
			{
				SPDLOG_INFO(
					"no mmsData present"
					", ingestionKey: {}"
					", workspaceKey: {}"
					", confKey: {}",
					ingestionKey, workspace->_workspaceKey, confKey
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			mmsDataRoot = channelDataRoot[field];
		}

		json youTubeURLsRoot;
		{
			field = "youTubeURLs";
			if (!JSONUtils::isMetadataPresent(mmsDataRoot, field))
			{
				SPDLOG_INFO(
					"no youTubeURLs present"
					", ingestionKey: {}"
					", workspaceKey: {}"
					", confKey: {}",
					ingestionKey, workspace->_workspaceKey, confKey
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			youTubeURLsRoot = mmsDataRoot[field];
		}

		if (youTubeURLsRoot.size() == 0)
		{
			SPDLOG_INFO(
				"no youTubeURL present"
				", ingestionKey: {}"
				", workspaceKey: {}"
				", confKey: {}",
				ingestionKey, workspace->_workspaceKey, confKey
			);

			return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
		}

		{
			json youTubeLiveURLRoot = youTubeURLsRoot[youTubeURLsRoot.size() - 1];

			time_t tNow;
			{
				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmNow;

				localtime_r(&utcNow, &tmNow);
				tNow = mktime(&tmNow);
			}

			time_t tLastCalculatedURLTime;
			{
				unsigned long ulYear;
				unsigned long ulMonth;
				unsigned long ulDay;
				unsigned long ulHour;
				unsigned long ulMinutes;
				unsigned long ulSeconds;
				int sscanfReturn;

				field = "timestamp";
				string timestamp = JSONUtils::asString(youTubeLiveURLRoot, field, "");

				if ((sscanfReturn =
						 sscanf(timestamp.c_str(), "%4lu-%2lu-%2lu %2lu:%2lu:%2lu", &ulYear, &ulMonth, &ulDay, &ulHour, &ulMinutes, &ulSeconds)) != 6)
				{
					string errorMessage = std::format(
						"timestamp has a wrong format (sscanf failed)"
						", ingestionKey: {}"
						", workspaceKey: {}"
						", confKey: {}"
						", sscanfReturn: {}",
						ingestionKey, workspace->_workspaceKey, confKey, sscanfReturn
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmLastCalculatedURL;

				localtime_r(&utcNow, &tmLastCalculatedURL);

				tmLastCalculatedURL.tm_year = ulYear - 1900;
				tmLastCalculatedURL.tm_mon = ulMonth - 1;
				tmLastCalculatedURL.tm_mday = ulDay;
				tmLastCalculatedURL.tm_hour = ulHour;
				tmLastCalculatedURL.tm_min = ulMinutes;
				tmLastCalculatedURL.tm_sec = ulSeconds;

				tLastCalculatedURLTime = mktime(&tmLastCalculatedURL);
			}

			hoursFromLastCalculatedURL = (tNow - tLastCalculatedURLTime) / 3600;

			field = "youTubeURL";
			lastCalculatedURL = JSONUtils::asString(youTubeLiveURLRoot, field, "");
		}

		return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
	}
	catch (DBRecordNotFound &e)
	{
		string errorMessage = std::format(
			"getLastYouTubeURLDetails failed"
			", ingestionKey: {}"
			", workspaceKey: {}"
			", confKey: {}",
			ingestionKey, workspace->_workspaceKey, confKey
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (...)
	{
		string errorMessage = std::format(
			"getLastYouTubeURLDetails failed"
			", ingestionKey: {}"
			", workspaceKey: {}"
			", confKey: {}",
			ingestionKey, workspace->_workspaceKey, confKey
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

void MMSEngineDBFacade::updateChannelDataWithNewYouTubeURL(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t confKey, string streamingYouTubeLiveURL
)
{
	try
	{
		json channelDataRoot = stream_columnAsJson(workspace->_workspaceKey, "userData", confKey);
		/*
		tuple<string, string, string> channelDetails = getStreamDetails(workspace->_workspaceKey, confKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		json channelDataRoot = JSONUtils::toJson(channelData);
		*/

		// add streamingYouTubeLiveURL info to the channelData
		{
			string field;

			json youTubeLiveURLRoot;
			{
				// char strNow[64];
				string strNow;
				{
					time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

					tm tmNow;

					localtime_r(&utcNow, &tmNow);
					/*
					sprintf(
						strNow, "%04d-%02d-%02d %02d:%02d:%02d", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min,
						tmNow.tm_sec
					);
					*/
					strNow = std::format(
						"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday, tmNow.tm_hour,
						tmNow.tm_min, tmNow.tm_sec
					);
				}
				field = "timestamp";
				youTubeLiveURLRoot[field] = strNow;

				field = "youTubeURL";
				youTubeLiveURLRoot[field] = streamingYouTubeLiveURL;
			}

			json mmsDataRoot;
			{
				field = "mmsData";
				if (JSONUtils::isMetadataPresent(channelDataRoot, field))
					mmsDataRoot = channelDataRoot[field];
			}

			json previousYouTubeURLsRoot;
			{
				field = "youTubeURLs";
				if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
					previousYouTubeURLsRoot = mmsDataRoot[field];
			}

			json youTubeURLsRoot = json::array();

			// maintain the last 10 URLs
			int youTubeURLIndex;
			if (previousYouTubeURLsRoot.size() > 10)
				youTubeURLIndex = 10;
			else
				youTubeURLIndex = previousYouTubeURLsRoot.size();
			for (; youTubeURLIndex >= 0; youTubeURLIndex--)
				youTubeURLsRoot.push_back(previousYouTubeURLsRoot[youTubeURLIndex]);
			youTubeURLsRoot.push_back(youTubeLiveURLRoot);

			field = "youTubeURLs";
			mmsDataRoot[field] = youTubeURLsRoot;

			field = "mmsData";
			channelDataRoot[field] = mmsDataRoot;
		}

		bool labelToBeModified = false;
		string label;
		bool sourceTypeToBeModified = false;
		string sourceType;
		bool encodersPoolToBeModified = false;
		int64_t encodersPoolKey;
		bool urlToBeModified = false;
		string url;
		bool pushProtocolToBeModified = false;
		string pushProtocol;
		bool pushEncoderKeyToBeModified = false;
		int64_t pushEncoderKey = -1;
		bool pushPublicEncoderNameToBeModified = false;
		bool pushPublicEncoderName;
		bool pushServerPortToBeModified = false;
		int pushServerPort = -1;
		bool pushUriToBeModified = false;
		string pushUri;
		bool pushListenTimeoutToBeModified = false;
		int pushListenTimeout = -1;
		bool captureVideoDeviceNumberToBeModified = false;
		int captureVideoDeviceNumber = -1;
		bool captureVideoInputFormatToBeModified = false;
		string captureVideoInputFormat;
		bool captureFrameRateToBeModified = false;
		int captureFrameRate = -1;
		bool captureWidthToBeModified = false;
		int captureWidth = -1;
		bool captureHeightToBeModified = false;
		int captureHeight = -1;
		bool captureAudioDeviceNumberToBeModified = false;
		int captureAudioDeviceNumber = -1;
		bool captureChannelsNumberToBeModified = false;
		int captureChannelsNumber = -1;
		bool tvSourceTVConfKeyToBeModified = false;
		int64_t tvSourceTVConfKey = -1;
		bool typeToBeModified = false;
		string type;
		bool descriptionToBeModified = false;
		string description;
		bool nameToBeModified = false;
		string name;
		bool regionToBeModified = false;
		string region;
		bool countryToBeModified = false;
		string country;
		bool imageToBeModified = false;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		bool positionToBeModified = false;
		int position = -1;
		bool channelDataToBeModified = true;

		modifyStream(
			confKey, "", workspace->_workspaceKey, labelToBeModified, label, sourceTypeToBeModified, sourceType, encodersPoolToBeModified,
			encodersPoolKey, urlToBeModified, url, pushProtocolToBeModified, pushProtocol, pushEncoderKeyToBeModified, pushEncoderKey,
			pushPublicEncoderNameToBeModified, pushPublicEncoderName, pushServerPortToBeModified, pushServerPort, pushUriToBeModified, pushUri,
			pushListenTimeoutToBeModified, pushListenTimeout, captureVideoDeviceNumberToBeModified, captureVideoDeviceNumber,
			captureVideoInputFormatToBeModified, captureVideoInputFormat, captureFrameRateToBeModified, captureFrameRate, captureWidthToBeModified,
			captureWidth, captureHeightToBeModified, captureHeight, captureAudioDeviceNumberToBeModified, captureAudioDeviceNumber,
			captureChannelsNumberToBeModified, captureChannelsNumber, tvSourceTVConfKeyToBeModified, tvSourceTVConfKey, typeToBeModified, type,
			descriptionToBeModified, description, nameToBeModified, name, regionToBeModified, region, countryToBeModified, country, imageToBeModified,
			imageMediaItemKey, imageUniqueName, positionToBeModified, position, channelDataToBeModified, channelDataRoot
		);
	}
	catch (DBRecordNotFound &e)
	{
		string errorMessage = std::format(
			"updateChannelDataWithNewYouTubeURL failed"
			", ingestionKey: {}"
			", workspaceKey: {}"
			", confKey: {}"
			", streamingYouTubeLiveURL: {}",
			ingestionJobKey, workspace->_workspaceKey, confKey, streamingYouTubeLiveURL
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	catch (...)
	{
		string errorMessage = std::format(
			"updateChannelDataWithNewYouTubeURL failed"
			", ingestionKey: {}"
			", workspaceKey: {}"
			", confKey: {}"
			", streamingYouTubeLiveURL: {}",
			ingestionJobKey, workspace->_workspaceKey, confKey, streamingYouTubeLiveURL
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}
