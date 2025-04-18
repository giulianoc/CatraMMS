
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "StringUtils.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <sstream>

void MMSEngineDBFacade::createTablesIfNeeded()
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			// 2023-10-16: la partizione dovrebbe essere fatta da (workspaceKey, requestTimestamp) ma
			//	- poichè la sua gestione si complica un po
			//	- attualmente abbiamo un solo cliente che utilizza le statistiche
			// lasciamo solamente la partizione su requestTimestamp
			string sqlStatement = "create table if not exists MMS_RequestStatistic ("
								  "requestStatisticKey		bigint GENERATED ALWAYS AS IDENTITY, "
								  "workspaceKey					bigint not null, "
								  "ipAddress						text null, "
								  "geoInfoKey						bigint null, "
								  "userId								text not null, "
								  "physicalPathKey			bigint null, "
								  "confStreamKey				bigint null, "
								  "title								text not null, "
								  "requestTimestamp			timestamp without time zone not null, "
								  "upToNextRequestInSeconds	integer null, "
								  "constraint MMS_RequestStatistic_PK PRIMARY KEY (requestStatisticKey, requestTimestamp), "
								  "constraint MMS_RequestStatistic_FK foreign key (geoInfoKey) "
								  "references MMS_GEOInfo (geoInfoKey)) "
								  "partition by range (requestTimestamp)";
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
			// usato dal partitioning
			string sqlStatement = "create index if not exists MMS_RequestStatistic_idx1 on MMS_RequestStatistic ("
								  "requestTimestamp)";
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
			// 2023-03-18: aggiunto per le performance del metodo MMSEngineDBFacade::addRequestStatistic,
			//	in particolare per velocizzare
			//		select max(requestStatisticKey) from MMS_RequestStatistic
			//		where workspaceKey = ? and requestStatisticKey < ? and userId = ?
			string sqlStatement = "create index if not exists MMS_RequestStatistic_idx2 on MMS_RequestStatistic ("
								  "workspaceKey, userId, requestStatisticKey)";
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
			// usato da updateRequestStatisticGeoInfo
			string sqlStatement = "create index if not exists MMS_RequestStatistic_idx3 on MMS_RequestStatistic (ipAddress, geoInfoKey)";
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

		/*
		{
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx1 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, title, userId, upToNextRequestInSeconds)";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		{
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx2 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, userId, title, upToNextRequestInSeconds)";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		{
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx3 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, upToNextRequestInSeconds)";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}
		*/

		// per creare la partition
		retentionOfStatisticData();

		{
			string sqlStatement = "create table if not exists MMS_Workspace ("
								  "workspaceKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "creationDate					timestamp without time zone default (now() at time zone 'utc'),"
								  "name									text NOT NULL,"
								  "directoryName				text NOT NULL,"
								  "workspaceType				smallint NOT NULL,"
								  "deliveryURL					text NULL,"
								  "enabled							boolean NOT NULL,"
								  "maxEncodingPriority	text NOT NULL,"
								  "encodingPeriod				text NOT NULL,"
								  "maxIngestionsNumber	integer NOT NULL,"
								  "languageCode					text NOT NULL,"
								  "timezone							text NOT NULL default 'CET',"
								  "notes								text NULL,"
								  "preferences					jsonb,"
								  "constraint MMS_Workspace_PK PRIMARY KEY (workspaceKey)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Workspace_idx on MMS_Workspace (directoryName)";
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
			string sqlStatement = "create table if not exists MMS_WorkspaceCost ("
								  "workspaceKey  			bigint NOT NULL,"
								  "maxStorageInGB			integer NOT NULL,"
								  "currentCostForStorage	integer not null default 0,"
								  "dedicatedEncoder_power_1	integer not null default 0,"
								  "currentCostForDedicatedEncoder_power_1	integer not null default 0,"
								  "dedicatedEncoder_power_2	integer not null default 0,"
								  "currentCostForDedicatedEncoder_power_2	integer not null default 0,"
								  "dedicatedEncoder_power_3	integer not null default 0,"
								  "currentCostForDedicatedEncoder_power_3	integer not null default 0,"
								  "CDN_type_1				integer not null default 0,"
								  "currentCostForCDN_type_1	integer not null default 0,"
								  "support_type_1			boolean not null default false,"
								  "currentCostForSupport_type_1	integer not null default 0,"
								  "constraint MMS_WorkspaceCost_PK PRIMARY KEY (workspaceKey), "
								  "constraint MMS_WorkspaceCost_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_YouTube ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint not null,"
								  "label						text NOT NULL,"
								  "tokenType					text NOT NULL,"
								  "refreshToken				text NULL,"
								  "accessToken				text NULL,"
								  "constraint MMS_Conf_YouTube_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_YouTube_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_Facebook ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint not null,"
								  "label						text NOT NULL,"
								  "modificationDate			timestamp without time zone NOT NULL,"
								  "userAccessToken			text NOT NULL,"
								  "constraint MMS_Conf_Facebook_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_Facebook_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_Twitch ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "modificationDate			timestamp without time zone NOT NULL,"
								  "refreshToken				text NOT NULL,"
								  "constraint MMS_Conf_Twitch_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_Twitch_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_Tiktok ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "modificationDate			timestamp without time zone NOT NULL,"
								  "token						text NOT NULL,"
								  "constraint MMS_Conf_Tiktok_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_Tiktok_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_Stream ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  // IP_PULL, IP_PUSH, CaptureLive, TV
								  "sourceType					text NOT NULL,"
								  "encodersPoolKey			bigint NULL,"
								  "url						text NULL," // pull
								  "pushProtocol				text NULL,"
								  "pushEncoderKey				bigint NULL,"
								  "pushPublicEncoderName		boolean NULL,"
								  "pushServerPort				integer NULL,"
								  "pushUri					text NULL,"
								  "pushListenTimeout			smallint NULL," // seconds
								  "captureLiveVideoDeviceNumber	smallint NULL,"
								  "captureLiveVideoInputFormat	text NULL,"
								  "captureLiveFrameRate			smallint NULL,"
								  "captureLiveWidth				smallint NULL,"
								  "captureLiveHeight				smallint NULL,"
								  "captureLiveAudioDeviceNumber	smallint NULL,"
								  "captureLiveChannelsNumber		smallint NULL,"
								  "tvSourceTVConfKey			bigint NULL,"
								  "type						text NULL,"
								  "description				text NULL,"
								  "name						text NULL,"
								  "region						text NULL,"
								  "country					text NULL,"
								  "imageMediaItemKey			bigint NULL,"
								  "imageUniqueName			text NULL,"
								  "position					integer NULL,"
								  "userData					jsonb,"
								  "constraint MMS_Conf_Stream_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_Stream_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create index if not exists MMS_Conf_Stream_idx on MMS_Conf_Stream (workspaceKey, type)";
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
			string sqlStatement = "create table if not exists MMS_Conf_SourceTVStream ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "type						text NOT NULL,"
								  "serviceId					smallint NULL,"
								  "networkId					smallint NULL,"
								  "transportStreamId			smallint NULL,"
								  "name						text NOT NULL,"
								  "satellite					text NULL,"
								  "frequency					integer NOT NULL,"
								  "lnb						text NULL,"
								  "videoPid					smallint NULL,"
								  "audioPids					text NULL,"
								  "audioItalianPid			smallint NULL,"
								  "audioEnglishPid			smallint NULL,"
								  "teletextPid				smallint NULL,"
								  "modulation					text NULL,"
								  "polarization				text NULL,"
								  "symbolRate					integer NULL,"
								  "bandwidthInHz				integer NULL,"
								  "country					text NULL,"
								  "deliverySystem				text NULL,"
								  "constraint MMS_Conf_SourceTVStream_PK PRIMARY KEY (confKey)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Conf_SourceTVStream_idx "
								  "on MMS_Conf_SourceTVStream (type, serviceId, name, lnb, frequency, videoPid, audioPids) NULLS NOT DISTINCT";
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
			string sqlStatement = "create index if not exists MMS_Conf_SourceTVStream_idx1 on MMS_Conf_SourceTVStream (name)";
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
			// type: SHARED or DEDICATED
			string sqlStatement = "create table if not exists MMS_Conf_AWSChannel ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "channelId					text NOT NULL,"
								  "rtmpURL					text NOT NULL,"
								  "playURL					text NOT NULL,"
								  "type						text NOT NULL,"
								  "outputIndex				smallint NULL,"
								  "reservedByIngestionJobKey	bigint NULL,"
								  "constraint MMS_Conf_AWSChannel_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_AWSChannel_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Conf_AWSChannel_idx "
								  "on MMS_Conf_AWSChannel (outputIndex, reservedByIngestionJobKey) "
								  "where outputIndex is not null and reservedByIngestionJobKey is not null";
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
			// type: SHARED or DEDICATED
			string sqlStatement = "create table if not exists MMS_Conf_CDN77Channel ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "rtmpURL					text NOT NULL,"
								  "resourceURL				text NOT NULL,"
								  "filePath					text NOT NULL,"
								  "secureToken				text NULL,"
								  "type						text NOT NULL,"
								  "outputIndex				smallint NULL,"
								  "reservedByIngestionJobKey	bigint NULL,"
								  "constraint MMS_Conf_CDN77Channel_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_CDN77Channel_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Conf_CDN77Channel_idx "
								  "on MMS_Conf_CDN77Channel (outputIndex, reservedByIngestionJobKey) "
								  "where outputIndex is not null and reservedByIngestionJobKey is not null";
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
			// type: SHARED or DEDICATED
			string sqlStatement = "create table if not exists MMS_Conf_RTMPChannel ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "rtmpURL					text NOT NULL,"
								  "streamName					text NULL,"
								  "userName					text NULL,"
								  "password					text NULL,"
								  "playURL					text NULL,"
								  "type						text NOT NULL,"
								  "outputIndex				smallint NULL,"
								  "reservedByIngestionJobKey	bigint NULL,"
								  "constraint MMS_Conf_RTMPChannel_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_RTMPChannel_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Conf_RTMPChannel_idx "
								  "on MMS_Conf_RTMPChannel (outputIndex, reservedByIngestionJobKey) "
								  "where outputIndex is not null and reservedByIngestionJobKey is not null";
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
			// type: SHARED or DEDICATED
			string sqlStatement = "create table if not exists MMS_Conf_HLSChannel ("
								  "confKey					bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NOT NULL,"
								  "deliveryCode				integer NOT NULL,"
								  "segmentDuration			smallint NULL,"
								  "playlistEntriesNumber		smallint NULL,"
								  "type						text NOT NULL,"
								  "outputIndex				smallint NULL,"
								  "reservedByIngestionJobKey	bigint NULL,"
								  "constraint MMS_Conf_HLSChannel_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_HLSChannel_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label), "
								  "UNIQUE (workspaceKey, deliveryCode)) ";
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
			string sqlStatement = "create unique index if not exists MMS_Conf_HLSChannel_idx "
								  "on MMS_Conf_HLSChannel (outputIndex, reservedByIngestionJobKey) "
								  "where outputIndex is not null and reservedByIngestionJobKey is not null";
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
			string sqlStatement = "create table if not exists MMS_Conf_FTP ("
								  "confKey                    bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey               bigint NOT NULL,"
								  "label                      text NOT NULL,"
								  "server						text NOT NULL,"
								  "port						integer NOT NULL,"
								  "userName					text NOT NULL,"
								  "password					text NOT NULL,"
								  "remoteDirectory			text NOT NULL,"
								  "constraint MMS_Conf_FTP_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_FTP_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_Conf_EMail ("
								  "confKey                    bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey               bigint NOT NULL,"
								  "label                      text NOT NULL,"
								  "addresses					text NOT NULL,"
								  "subject					text NOT NULL,"
								  "message					text NOT NULL,"
								  "constraint MMS_Conf_EMail_PK PRIMARY KEY (confKey), "
								  "constraint MMS_Conf_EMail_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, label)) ";
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
			string sqlStatement = "create table if not exists MMS_OnceExecution ("
								  "type										text NOT NULL,"
								  "lastExecutionTime			timestamp without time zone NOT NULL,"
								  "constraint MMS_OnceExecution_PK PRIMARY KEY (type))";
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
			OnceType onceType = OnceType::DBDataRetention;

			string sqlStatement = std::format(
				"insert into MMS_OnceExecution (type, lastExecutionTime) "
				"select {}, NULL where not exists "
				"(select type from MMS_OnceExecution where type = {})",
				trans.transaction->quote(MMSEngineDBFacade::toString(onceType)), trans.transaction->quote(MMSEngineDBFacade::toString(onceType))
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
			OnceType onceType = OnceType::GEOInfo;

			string sqlStatement = std::format(
				"insert into MMS_OnceExecution (type, lastExecutionTime) "
				"select {}, NULL where not exists "
				"(select type from MMS_OnceExecution where type = {})",
				trans.transaction->quote(MMSEngineDBFacade::toString(onceType)), trans.transaction->quote(MMSEngineDBFacade::toString(onceType))
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
			string sqlStatement = "create table if not exists MMS_PartitionInfo ("
								  "partitionKey			bigint NOT NULL,"
								  "partitionPathName		text NOT NULL,"
								  "currentFreeSizeInBytes	bigint NOT NULL,"
								  "freeSpaceToLeaveInMB	bigint NOT NULL,"
								  "lastUpdateFreeSize		timestamp without time zone default (now() at time zone 'utc'),"
								  "awsCloudFrontHostName	text NULL,"
								  "enabled				boolean NOT NULL,"
								  "constraint MMS_PartitionInfo_PK PRIMARY KEY (partitionKey)) ";
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
			string sqlStatement = "create table if not exists MMS_User ("
								  "userKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "name					text NULL,"
								  "eMailAddress			text NULL,"
								  "password				text NOT NULL,"
								  "country				text NULL,"
								  "timezone				text NOT NULL default 'CET',"
								  "creationDate			timestamp without time zone default (now() at time zone 'utc'),"
								  "insolvent				boolean NOT NULL default false,"
								  "expirationDate			timestamp without time zone NOT NULL,"
								  "lastSuccessfulLogin	timestamp without time zone NULL,"
								  "constraint MMS_User_PK PRIMARY KEY (userKey), "
								  "UNIQUE (eMailAddress))";
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
			string sqlStatement = "create table if not exists MMS_LoginStatistic ("
								  "loginStatisticKey		bigint GENERATED ALWAYS AS IDENTITY,"
								  "userKey				bigint NOT NULL,"
								  "ip						text NULL,"
								  "geoInfoKey						bigint null, "
								  "successfulLogin		timestamp without time zone NOT NULL,"
								  "constraint MMS_LoginStatistic_PK PRIMARY KEY (loginStatisticKey), "
								  "constraint MMS_LoginStatistic_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade, "
								  "constraint MMS_LoginStatistic_FK2 foreign key (geoInfoKey) "
								  "references MMS_GEOInfo (geoInfoKey)) ";
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
			// usato da updateLoginStatisticGeoInfo
			string sqlStatement = "create index if not exists MMS_LoginStatistic_idx on MMS_LoginStatistic (ip, geoInfoKey)";
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
			string sqlStatement = "create table if not exists MMS_GEOInfo ("
								  "geoInfoKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "continent				text NULL,"
								  "continentCode		text NULL,"
								  "country					text NULL,"
								  "countryCode			text NULL,"
								  "region						text NULL,"
								  "city							text NULL,"
								  "org							text NULL,"
								  "isp							text NULL,"
								  "constraint MMS_GEOInfo_PK PRIMARY KEY (geoInfoKey)) ";
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
			string sqlStatement = "create unique index if not exists mms_geoinfo_idx on mms_geoinfo (continent, continentcode, country, countrycode, "
								  "region, city, org, isp)";
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
			string sqlStatement = "create table if not exists MMS_Invoice ("
								  "invoiceKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "userKey				bigint NOT NULL,"
								  "creationDate			timestamp without time zone default (now() at time zone 'utc'),"
								  "description			text NOT NULL,"
								  "amount					int NOT NULL,"
								  "expirationDate			timestamp without time zone NOT NULL,"
								  "paid					boolean NOT NULL,"
								  "paymentDate			timestamp without time zone NULL,"
								  "constraint MMS_Invoice_PK PRIMARY KEY (invoiceKey), "
								  "constraint MMS_Invoice_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_APIKey ("
								  "apiKey             text NOT NULL,"
								  "userKey            bigint NOT NULL,"
								  "workspaceKey       bigint NOT NULL,"
								  "isOwner            boolean NOT NULL,"
								  "isDefault			boolean NOT NULL,"
								  // same in MMS_Code
								  "permissions		jsonb NOT NULL,"
								  "creationDate		timestamp without time zone default (now() at time zone 'utc'),"
								  "expirationDate		timestamp without time zone NOT NULL,"
								  "constraint MMS_APIKey_PK PRIMARY KEY (apiKey), "
								  "constraint MMS_APIKey_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade, "
								  "constraint MMS_APIKey_FK2 foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create unique index if not exists MMS_APIKey_idx on MMS_APIKey (userKey, workspaceKey)";
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
			string sqlStatement = "create table if not exists MMS_Code ("
								  "code							text NOT NULL,"
								  "workspaceKey					bigint NOT NULL,"
								  "userKey						bigint NULL,"
								  "userEmail						text NULL,"
								  "type							text NOT NULL,"
								  // same in MMS_APIKey
								  "permissions					jsonb NOT NULL,"
								  "creationDate					timestamp without time zone default (now() at time zone 'utc'),"
								  "constraint MMS_Code_PK PRIMARY KEY (code),"
								  "constraint MMS_Code_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade, "
								  "constraint MMS_Code_FK2 foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_ResetPasswordToken ("
								  "token				text NOT NULL,"
								  "userKey			bigint NOT NULL,"
								  "creationDate		timestamp without time zone default (now() at time zone 'utc'),"
								  "constraint MMS_ResetPasswordToken_PK PRIMARY KEY (token),"
								  "constraint MMS_ResetPasswordToken_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_WorkspaceMoreInfo ("
								  "workspaceKey  			bigint NOT NULL,"
								  "currentDirLevel1		smallint NOT NULL,"
								  "currentDirLevel2		smallint NOT NULL,"
								  "currentDirLevel3		smallint NOT NULL,"
								  "startDateTime			timestamp without time zone NOT NULL,"
								  "endDateTime			timestamp without time zone NOT NULL,"
								  "currentIngestionsNumber	integer NOT NULL,"
								  "constraint MMS_WorkspaceMoreInfo_PK PRIMARY KEY (workspaceKey), "
								  "constraint MMS_WorkspaceMoreInfo_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_EncodingProfile ("
								  "encodingProfileKey  	bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey  			bigint NULL,"
								  "label					text NOT NULL,"
								  "contentType			text NOT NULL,"
								  "deliveryTechnology		text NOT NULL,"
								  "jsonProfile    		jsonb NOT NULL,"
								  "constraint MMS_EncodingProfile_PK PRIMARY KEY (encodingProfileKey), "
								  "constraint MMS_EncodingProfile_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create unique index if not exists MMS_EncodingProfile_idx "
								  "on MMS_EncodingProfile (workspaceKey, contentType, label) NULLS NOT DISTINCT";
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

		/* 2023-11-24: questo indice non lo capisco
		{
			string sqlStatement =
				"create unique index if not exists MMS_EncodingProfile_idx on MMS_EncodingProfile (workspaceKey, contentType)";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}
		*/

		{
			string predefinedProfilesDirectoryPath[3] = {
				_predefinedVideoProfilesDirectoryPath, _predefinedAudioProfilesDirectoryPath, _predefinedImageProfilesDirectoryPath
			};
			string videoSuffix("video");
			string audioSuffix("audio");
			string imageSuffix("image");

			for (string predefinedProfileDirectoryPath : predefinedProfilesDirectoryPath)
			{
				MMSEngineDBFacade::ContentType contentType;

				// if (predefinedProfileDirectoryPath.size() >= videoSuffix.size()
				//       && 0 == predefinedProfileDirectoryPath.compare(predefinedProfileDirectoryPath.size()-videoSuffix.size(),
				//         videoSuffix.size(), videoSuffix))
				if (predefinedProfileDirectoryPath.ends_with(videoSuffix))
				{
					contentType = MMSEngineDBFacade::ContentType::Video;
				}
				else if (predefinedProfileDirectoryPath.ends_with(audioSuffix))
				// else if (predefinedProfileDirectoryPath.size() >= audioSuffix.size() &&
				// 	 0 == predefinedProfileDirectoryPath.compare(
				// 		  predefinedProfileDirectoryPath.size() - audioSuffix.size(), audioSuffix.size(), audioSuffix
				//   ))
				{
					contentType = MMSEngineDBFacade::ContentType::Audio;
				}
				else if (predefinedProfileDirectoryPath.ends_with(imageSuffix))
				// else if (predefinedProfileDirectoryPath.size() >= imageSuffix.size() &&
				// 	 0 == predefinedProfileDirectoryPath.compare(
				// 				  predefinedProfileDirectoryPath.size() - imageSuffix.size(), imageSuffix.size(), imageSuffix
				// 		  ))
				{
					contentType = MMSEngineDBFacade::ContentType::Image;
				}
				else
				{
					string errorMessage = std::format(
						"Wrong predefinedProfileDirectoryPath"
						", predefinedProfileDirectoryPath: {}",
						predefinedProfileDirectoryPath
					);
					SPDLOG_ERROR(errorMessage);

					continue;
				}

				for (fs::directory_entry const &entry : fs::directory_iterator(predefinedProfileDirectoryPath))
				{
					try
					{
						if (!entry.is_regular_file())
							continue;

						if (entry.path().filename().string().find(".json") == string::npos)
						{
							string errorMessage = std::format(
								"Wrong filename (encoding profile) extention"
								", entry.path().filename(): {}",
								entry.path().filename().string()
							);
							SPDLOG_ERROR(errorMessage);

							continue;
						}

						string jsonProfile;
						{
							ifstream profileFile(entry.path());
							stringstream buffer;
							buffer << profileFile.rdbuf();

							jsonProfile = buffer.str();

							SPDLOG_INFO(
								"Reading profile"
								", profile pathname: {}"
								", profile: {}",
								entry.path().string(), jsonProfile
							);
						}

						json encodingMedatada = JSONUtils::toJson(jsonProfile);

						string label = JSONUtils::asString(encodingMedatada, "label", "");
						string fileFormat = JSONUtils::asString(encodingMedatada, "fileFormat", "");

						MMSEngineDBFacade::DeliveryTechnology deliveryTechnology = MMSEngineDBFacade::fileFormatToDeliveryTechnology(fileFormat);

						SPDLOG_INFO(
							"Encoding technology"
							", predefinedProfileDirectoryPath: {}"
							", label: {}"
							", fileFormat: {}"
							", deliveryTechnology: {}",
							predefinedProfileDirectoryPath, label, fileFormat, toString(deliveryTechnology)
						);

						{
							int64_t encodingProfileKey = -1;
							string sqlStatement = std::format(
								"select encodingProfileKey from MMS_EncodingProfile "
								"where workspaceKey is null and contentType = {} and label = {}",
								trans.transaction->quote(toString(contentType)), trans.transaction->quote(label)
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
									"DEFAULT,            NULL,         {},    {},          {},                 {}) ",
									trans.transaction->quote(label), trans.transaction->quote(MMSEngineDBFacade::toString(contentType)),
									trans.transaction->quote(toString(deliveryTechnology)), trans.transaction->quote(jsonProfile)
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
		}

		{
			string sqlStatement = "create table if not exists MMS_EncodingProfilesSet ("
								  "encodingProfilesSetKey  	bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey  				bigint NOT NULL,"
								  "contentType				text NOT NULL,"
								  "label						text NOT NULL,"
								  "constraint MMS_EncodingProfilesSet_PK PRIMARY KEY (encodingProfilesSetKey),"
								  "constraint MMS_EncodingProfilesSet_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "UNIQUE (workspaceKey, contentType, label)) ";
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
			string sqlStatement = "create table if not exists MMS_EncodingProfilesSetMapping ("
								  "encodingProfilesSetKey  	bigint NOT NULL,"
								  "encodingProfileKey			bigint NOT NULL,"
								  "constraint MMS_EncodingProfilesSetMapping_PK PRIMARY KEY (encodingProfilesSetKey, encodingProfileKey), "
								  "constraint MMS_EncodingProfilesSetMapping_FK1 foreign key (encodingProfilesSetKey) "
								  "references MMS_EncodingProfilesSet (encodingProfilesSetKey) on delete cascade, "
								  "constraint MMS_EncodingProfilesSetMapping_FK2 foreign key (encodingProfileKey) "
								  "references MMS_EncodingProfile (encodingProfileKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_Encoder ("
								  "encoderKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "label					text NOT NULL,"
								  "external				boolean NOT NULL,"
								  "enabled				boolean NOT NULL,"
								  "protocol				text NOT NULL,"
								  "publicServerName		text NOT NULL,"
								  "internalServerName		text NOT NULL,"
								  "port					integer NOT NULL,"
								  "constraint MMS_Encoder_PK PRIMARY KEY (encoderKey), "
								  "UNIQUE (label)) ";
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
			string sqlStatement = "create index if not exists MMS_Encoder_idx on MMS_Encoder (publicServerName)";
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
			string sqlStatement = "create table if not exists MMS_EncoderWorkspaceMapping ("
								  "encoderKey					bigint NOT NULL,"
								  "workspaceKey				bigint NOT NULL,"
								  "constraint MMS_EncoderWorkspaceMapping_PK PRIMARY KEY (encoderKey, workspaceKey), "
								  "constraint MMS_EncoderWorkspaceMapping_FK1 foreign key (encoderKey) "
								  "references MMS_Encoder (encoderKey) on delete cascade, "
								  "constraint MMS_EncoderWorkspaceMapping_FK2 foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_EncodersPool ("
								  "encodersPoolKey			bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey				bigint NOT NULL,"
								  "label						text NULL,"
								  "lastEncoderIndexUsed		smallint NOT NULL,"
								  "constraint MMS_EncodersPool_PK PRIMARY KEY (encodersPoolKey),"
								  "constraint MMS_EncodersPool_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			string sqlStatement = "create unique index if not exists MMS_EncodersPool_idx "
								  "on MMS_EncodersPool (workspaceKey, label) NULLS NOT DISTINCT";
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
			string sqlStatement = "create table if not exists MMS_EncoderEncodersPoolMapping ("
								  "encodersPoolKey			bigint NOT NULL,"
								  "encoderKey					bigint NOT NULL,"
								  "constraint MMS_EncoderEncodersPoolMapping_PK PRIMARY KEY (encodersPoolKey, encoderKey), "
								  "constraint MMS_EncoderEncodersPoolMapping_FK1 foreign key (encodersPoolKey) "
								  "references MMS_EncodersPool (encodersPoolKey) on delete cascade, "
								  "constraint MMS_EncoderEncodersPoolMapping_FK2 foreign key (encoderKey) "
								  "references MMS_Encoder (encoderKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_IngestionRoot ("
								  "ingestionRootKey           bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey               bigint NOT NULL,"
								  "userKey    								bigint NOT NULL,"
								  "type                       text NOT NULL,"
								  "label                      text NULL,"
								  "hidden 										boolean NOT NULL default false,"
								  "metaDataContent						jsonb NOT NULL,"
								  "processedMetaDataContent	text NULL,"
								  "ingestionDate              timestamp without time zone NOT NULL,"
								  "lastUpdate                 timestamp without time zone default (now() at time zone 'utc'),"
								  "status           					text NOT NULL,"
								  "constraint MMS_IngestionRoot_PK PRIMARY KEY (ingestionRootKey), "
								  "constraint MMS_IngestionRoot_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "constraint MMS_IngestionRoot_FK2 foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_IngestionRoot_idx on MMS_IngestionRoot (workspaceKey, label)";
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
			string sqlStatement = "create index if not exists MMS_IngestionRoot_idx2 on MMS_IngestionRoot (workspaceKey, ingestionDate, hidden)";
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
			// utile per MMSEngineDBFacade::retentionOfIngestionData
			string sqlStatement = "create index if not exists MMS_IngestionRoot_idx3 on MMS_IngestionRoot ((status like 'Completed%') desc)";
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
			// 2024-08-15: ho pensato di partizionare MMS_IngestionJob in base al campo status (partizione not_completed e completed)
			// Il vantaggio sarebbe:
			// 1. avere migliori performance in quanto la maggioranza delle query solo sul not_completed che sarebbe molto piccolo
			// 2. la view della GUI potrebbe togliere il vincolo sulla data e mostrerebbe tutti gli ingestion/encoding running
			// Il problema è che, per come postgres vuole le partizioni, bisognerebbe aggiungere status nella primary key insieme a ingestionJobKey.
			// Questo è un problema perchè abbiamo tante foreign key in altre tabelle che puntano a ingestionjobkey e non avrebbe senso aggiungere
			// anche il campo status nella foreign key.
			string sqlStatement =
				"create table if not exists MMS_IngestionJob ("
				"ingestionJobKey  			bigint GENERATED ALWAYS AS IDENTITY,"
				"ingestionRootKey           bigint NOT NULL,"
				"parentGroupOfTasksIngestionJobKey	bigint NULL,"
				"label						text NULL,"
				"metaDataContent            jsonb NOT NULL,"
				"ingestionType              text NOT NULL,"
				"processingStartingFrom		timestamp without time zone NOT NULL,"
				"priority					smallint NOT NULL,"
				"startProcessing            timestamp without time zone NULL,"
				"endProcessing              timestamp without time zone NULL,"
				"downloadingProgress        numeric(4,1) NULL,"
				"uploadingProgress          numeric(4,1) NULL,"
				"sourceBinaryTransferred    boolean NOT NULL,"
				"processorMMS               text NULL,"
				"status           			text NOT NULL,"
				"errorMessage               text NULL,"
				"scheduleStart_virtual		timestamp without time zone not NULL,"
				// added because the channels view was slow
				"configurationLabel_virtual	text generated always as (metaDataContent ->> 'configurationLabel') stored NULL,"
				"recordingCode_virtual		bigint generated always as ((metaDataContent ->> 'recordingCode')::bigint) stored NULL,"
				"broadcastIngestionJobKey_virtual	bigint generated always as ((metaDataContent -> 'internalMMS' -> 'broadcaster' ->> "
				"'broadcastIngestionJobKey')::bigint) stored NULL,"
				"toBeManaged_virtual		boolean generated always as (status = 'Start_TaskQueued' or (status in ('SourceDownloadingInProgress', "
				"'SourceMovingInProgress', 'SourceCopingInProgress', 'SourceUploadingInProgress') and sourceBinaryTransferred = true)) stored not "
				"null,"
				"constraint MMS_IngestionJob_PK PRIMARY KEY (ingestionJobKey), "
				"constraint MMS_IngestionJob_FK foreign key (ingestionRootKey) "
				"references MMS_IngestionRoot (ingestionRootKey) on delete cascade) ";
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
			string sqlStatement =
				"CREATE OR REPLACE FUNCTION MMS_IngestionJob_fillVirtual() RETURNS trigger AS $fillVirtual$ "
				"BEGIN "
				"case when NEW.metaDataContent -> 'schedule' ->> 'start' is null then "
				"NEW.scheduleStart_virtual='-infinity'; "
				"else "
				"NEW.scheduleStart_virtual=to_timestamp(NEW.metaDataContent -> 'schedule' ->> 'start', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'); "
				"end case; "
				"return NEW; "
				"END; "
				"$fillVirtual$ LANGUAGE plpgsql";
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
			string sqlStatement =
				"CREATE OR REPLACE TRIGGER MMS_IngestionJob_fillVirtual BEFORE INSERT OR UPDATE OF metaDataContent ON MMS_IngestionJob "
				"FOR EACH ROW EXECUTE PROCEDURE MMS_IngestionJob_fillVirtual()";
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
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx6 on MMS_IngestionJob (processingStartingFrom)";
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

		// 2024-01-14: usato poco
		{
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx8 on MMS_IngestionJob (recordingCode_virtual)";
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
			// viene usato dalla select di getIngestionsToBeManaged
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx9 on MMS_IngestionJob(priority, processingstartingfrom, "
								  "(toBeManaged_virtual = true), processorMMS, scheduleStart_virtual) NULLS NOT DISTINCT";
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
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx10 on MMS_IngestionJob (status)";
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

		// 2024-01-14: non sembra sia usato
		{
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx11 on MMS_IngestionJob (label)";
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
			// usato da: select status from MMS_IngestionJob where ingestionRootKey = 491421
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx12 on MMS_IngestionJob (ingestionRootKey)";
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
			// viene usato dalla select di getIngestionJobsStatus
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx13 on MMS_IngestionJob (ingestionType, configurationLabel_virtual)";
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
			// viene usato dalla select di getIngestionsToBeManaged
			string sqlStatement = "create index if not exists MMS_IngestionJob_idx14 on MMS_IngestionJob (processorMMS, status, "
								  "sourceBinaryTransferred, processingStartingFrom, scheduleStart_virtual)";
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
			string sqlStatement = "create table if not exists MMS_IngestionJobDependency ("
								  "ingestionJobDependencyKey  	bigint GENERATED ALWAYS AS IDENTITY,"
								  "ingestionJobKey				bigint NOT NULL,"
								  "dependOnSuccess				smallint NOT NULL,"
								  "dependOnIngestionJobKey		bigint NULL,"
								  "orderNumber					smallint NOT NULL,"
								  "referenceOutputDependency		smallint NOT NULL,"
								  "constraint MMS_IngestionJobDependency_PK PRIMARY KEY (ingestionJobDependencyKey), "
								  "constraint MMS_IngestionJobDependency_FK foreign key (ingestionJobKey) "
								  "references MMS_IngestionJob (ingestionJobKey) on delete cascade, "
								  "constraint MMS_IngestionJobDependency_FK2 foreign key (dependOnIngestionJobKey) "
								  "references MMS_IngestionJob (ingestionJobKey) on delete cascade) ";
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
			string sqlStatement =
				"create index if not exists MMS_IngestionJobDependency_idx1 on MMS_IngestionJobDependency (ingestionJobKey, orderNumber)";
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
			string sqlStatement =
				"create index if not exists MMS_IngestionJobDependency_idx2 on MMS_IngestionJobDependency (dependOnIngestionJobKey, dependOnSuccess)";
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
			string sqlStatement = "create table if not exists MMS_WorkflowLibrary ("
								  "workflowLibraryKey		bigint GENERATED ALWAYS AS IDENTITY,"
								  "workspaceKey  			bigint NULL,"
								  // if creatorUserKey is NULL, it means
								  //	- it was loaded by mmsEngine when it started
								  //	- belong to MMS scope (workspaceKey is NULL)
								  "creatorUserKey			bigint NULL,"
								  "lastUpdateUserKey		bigint NULL,"
								  "label					text NULL,"
								  "thumbnailMediaItemKey	bigint NULL,"
								  "jsonWorkflow			text NOT NULL,"
								  "constraint MMS_WorkflowLibrary_PK PRIMARY KEY (workflowLibraryKey), "
								  "constraint MMS_WorkflowLibrary_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "constraint MMS_WorkflowLibrary_FK2 foreign key (creatorUserKey) "
								  "references MMS_User (userKey) on delete cascade) ";
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
			string sqlStatement = "create unique index if not exists MMS_WorkflowLibrary_idx "
								  "on MMS_WorkflowLibrary (workspaceKey, label) "
								  "NULLS NOT DISTINCT";
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

		try
		{
			{
				for (fs::directory_entry const &entry : fs::directory_iterator(_predefinedWorkflowLibraryDirectoryPath))
				{
					try
					{
						if (!entry.is_regular_file())
							continue;

						if (entry.path().filename().string().find(".json") == string::npos)
						{
							string errorMessage = std::format(
								"Wrong filename (workflow) extention"
								", entry.path().filename(): {}",
								entry.path().filename().string()
							);
							SPDLOG_ERROR(errorMessage);

							continue;
						}

						string jsonWorkflow;
						{
							ifstream profileFile(entry.path());
							stringstream buffer;
							buffer << profileFile.rdbuf();

							jsonWorkflow = buffer.str();

							SPDLOG_INFO(
								"Reading workflow"
								", workflow pathname: {}"
								", workflow: {}",
								entry.path().string(), jsonWorkflow
							);
						}

						json workflowRoot = JSONUtils::toJson(jsonWorkflow);

						string label = JSONUtils::asString(workflowRoot, "label", "");

						int64_t workspaceKey = -1;
						addUpdateWorkflowAsLibrary(trans, -1, workspaceKey, label, -1, jsonWorkflow, true);
					}
					catch (runtime_error &e)
					{
						string errorMessage = std::format(
							"listing directory failed"
							", e.what(): {}",
							e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
					catch (exception &e)
					{
						string errorMessage = std::format(
							"listing directory failed"
							", e.what(): {}",
							e.what()
						);
						SPDLOG_ERROR(errorMessage);

						throw e;
					}
				}
			}
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(", e.what(): {}", e.what());

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(", e.what(): {}", e.what());

			throw e;
		}

		{
			string sqlStatement =
				"create table if not exists MMS_MediaItem ("
				"mediaItemKey           bigint GENERATED ALWAYS AS IDENTITY,"
				"workspaceKey           bigint NOT NULL,"
				"title                  text NOT NULL,"
				"ingester               text NULL,"
				"userData               jsonb,"
				"deliveryFileName       text NULL,"
				"ingestionJobKey        bigint NOT NULL,"
				"ingestionDate          timestamp without time zone NOT NULL,"
				"contentType            text NOT NULL,"
				"startPublishing        timestamp without time zone NOT NULL,"
				"endPublishing          timestamp without time zone NOT NULL,"
				"retentionInMinutes		bigint NOT NULL,"
				"tags					text[] NULL,"
				// just a metadata to hide obsolete LiveRecorderVirtualVOD
				// This is automatically set to false when overrideUniqueName is applied
				"markedAsRemoved		boolean NOT NULL,"
				"processorMMSForRetention	text NULL,"
				"liveRecordingChunk_virtual	boolean generated always as (userData -> 'mmsData' -> 'liveRecordingChunk' is not null) stored NOT NULL,"
				"recordingCode_virtual	bigint generated always as ((userData -> 'mmsData' -> 'liveRecordingChunk' ->> 'recordingCode')::bigint) "
				"stored NULL,"
				"utcStartTimeInMilliSecs_virtual	bigint generated always as ((userData -> 'mmsData' ->> 'utcStartTimeInMilliSecs')::bigint) "
				"stored NULL,"
				"utcEndTimeInMilliSecs_virtual	bigint generated always as ((userData -> 'mmsData' ->> 'utcEndTimeInMilliSecs')::bigint) stored NULL,"
				"willBeRemovedAt_virtual timestamp without time zone generated always as (ingestionDate + INTERVAL '1 minute' * retentionInMinutes) "
				"stored NOT NULL,"
				"constraint MMS_MediaItem_PK PRIMARY KEY (mediaItemKey), "
				"constraint MMS_MediaItem_FK foreign key (workspaceKey) "
				"references MMS_Workspace (workspaceKey) on delete cascade) ";
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
			// usato da getMediaItemKeyDetails
			string sqlStatement = "create index if not exists MMS_MediaItem_idx1 on MMS_MediaItem (workspaceKey, mediaItemKey)";
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
			string sqlStatement = "create index if not exists MMS_MediaItem_idx5 on MMS_MediaItem (recordingCode_virtual)";
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
			string sqlStatement = "create index if not exists MMS_MediaItem_idx6 on MMS_MediaItem (utcStartTimeInMilliSecs_virtual)";
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
			string sqlStatement = "create index if not exists MMS_MediaItem_idx7 on MMS_MediaItem (utcEndTimeInMilliSecs_virtual)";
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
			// usato da getMediaItemsList
			string sqlStatement = "create index if not exists MMS_MediaItem_idx8 on MMS_MediaItem (workspaceKey, "
								  "markedAsRemoved, contentType, liveRecordingChunk_virtual, ingestionDate)";
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
			string sqlStatement = "create index if not exists MMS_MediaItem_idx9 on MMS_MediaItem (willBeRemovedAt_virtual, "
								  "processorMMSForRetention) NULLS NOT DISTINCT";
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
			string sqlStatement = "create table if not exists MMS_ExternalUniqueName ("
								  "workspaceKey			bigint NOT NULL,"
								  "uniqueName      		text NOT NULL,"
								  "mediaItemKey  			bigint NOT NULL,"
								  "constraint MMS_ExternalUniqueName_PK PRIMARY KEY (workspaceKey, uniqueName), "
								  "constraint MMS_ExternalUniqueName_FK foreign key (workspaceKey) "
								  "references MMS_Workspace (workspaceKey) on delete cascade, "
								  "constraint MMS_ExternalUniqueName_FK2 foreign key (mediaItemKey) "
								  "references MMS_MediaItem (mediaItemKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_ExternalUniqueName_idx on MMS_ExternalUniqueName (workspaceKey, mediaItemKey)";
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
			string sqlStatement = "create table if not exists MMS_PhysicalPath ("
								  "physicalPathKey  			bigint GENERATED ALWAYS AS IDENTITY,"
								  "mediaItemKey				bigint NOT NULL,"
								  "drm	             		smallint NOT NULL,"
								  "externalReadOnlyStorage	boolean NOT NULL DEFAULT false,"
								  "fileName					text NOT NULL,"
								  "relativePath				text NOT NULL,"
								  "partitionNumber			smallint NULL,"
								  "sizeInBytes				bigint NOT NULL,"
								  "encodingProfileKey			bigint NULL,"
								  "durationInMilliSeconds		bigint NULL,"
								  "bitRate            		integer NULL,"
								  "deliveryInfo				jsonb,"
								  "metaData					jsonb,"
								  "isAlias					boolean NOT NULL DEFAULT false,"
								  "creationDate				timestamp without time zone default (now() at time zone 'utc'),"
								  "retentionInMinutes			bigint NULL,"
								  "constraint MMS_PhysicalPath_PK PRIMARY KEY (physicalPathKey), "
								  "constraint MMS_PhysicalPath_FK foreign key (mediaItemKey) "
								  "references MMS_MediaItem (mediaItemKey) on delete cascade, "
								  "constraint MMS_PhysicalPath_FK2 foreign key (encodingProfileKey) "
								  "references MMS_EncodingProfile (encodingProfileKey), "
								  "UNIQUE (mediaItemKey, relativePath, fileName, isAlias)) ";
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
			// it is not possible to have the same content using the same encoding profile key
			string sqlStatement = "create unique index if not exists MMS_PhysicalPath_idx "
								  "on MMS_PhysicalPath (mediaItemKey, encodingProfileKey) "
								  "where encodingProfileKey is not null";
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
			string sqlStatement = "create index if not exists MMS_PhysicalPath_idx2 on MMS_PhysicalPath (mediaItemKey, physicalPathKey, "
								  "encodingProfileKey, partitionNumber)";
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
			string sqlStatement = "create table if not exists MMS_IngestionJobOutput ("
								  "ingestionJobKey		bigint NOT NULL,"
								  "mediaItemKey			bigint NOT NULL,"
								  "physicalPathKey  		bigint NOT NULL,"
								  "position				int NOT NULL,"
								  "UNIQUE (ingestionJobKey, mediaItemKey, physicalPathKey), "
								  "constraint MMS_IngestionJobOutput_FK foreign key (ingestionJobKey) "
								  "references MMS_IngestionJob (ingestionJobKey) on delete cascade) ";
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
			string sqlStatement = "create table if not exists MMS_CrossReference ("
								  "sourceMediaItemKey			bigint NOT NULL,"
								  "type						text NOT NULL,"
								  "targetMediaItemKey			bigint NOT NULL,"
								  "parameters					text NULL,"
								  "constraint MMS_CrossReference_PK PRIMARY KEY (sourceMediaItemKey, type, targetMediaItemKey), "
								  "constraint MMS_CrossReference_FK1 foreign key (sourceMediaItemKey) "
								  "references MMS_MediaItem (mediaItemKey) on delete cascade, "
								  "constraint MMS_CrossReference_FK2 foreign key (targetMediaItemKey) "
								  "references MMS_MediaItem (mediaItemKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_CrossReference_idx on MMS_CrossReference (targetMediaItemKey, sourceMediaItemKey)";
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
			string sqlStatement = "create table if not exists MMS_VideoTrack ("
								  "videoTrackKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "physicalPathKey			bigint NOT NULL,"
								  "trackIndex					smallint NULL,"
								  "durationInMilliSeconds		bigint NULL,"
								  "width              		smallint NULL,"
								  "height             		smallint NULL,"
								  "avgFrameRate				text NULL,"
								  "codecName					text NULL,"
								  "bitRate					integer NULL,"
								  "profile					text NULL,"
								  "constraint MMS_VideoTrack_PK PRIMARY KEY (videoTrackKey), "
								  "constraint MMS_VideoTrack_FK foreign key (physicalPathKey) "
								  "references MMS_PhysicalPath (physicalPathKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_VideoTrack_idx on MMS_VideoTrack (physicalPathKey)";
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
			string sqlStatement = "create table if not exists MMS_AudioTrack ("
								  "audioTrackKey				bigint GENERATED ALWAYS AS IDENTITY,"
								  "physicalPathKey			bigint NOT NULL,"
								  "trackIndex					smallint NULL,"
								  "durationInMilliSeconds		bigint NULL,"
								  "codecName          		text NULL,"
								  "bitRate             		integer NULL,"
								  "sampleRate                 integer NULL,"
								  "channels             		smallint NULL,"
								  "language					text NULL,"
								  "constraint MMS_AudioTrack_PK PRIMARY KEY (audioTrackKey), "
								  "constraint MMS_AudioTrack_FK foreign key (physicalPathKey) "
								  "references MMS_PhysicalPath (physicalPathKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_AudioTrack_idx on MMS_AudioTrack (physicalPathKey)";
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
			string sqlStatement = "create table if not exists MMS_ImageItemProfile ("
								  "physicalPathKey			bigint NOT NULL,"
								  "width				smallint NOT NULL,"
								  "height				smallint NOT NULL,"
								  "format                       	text NULL,"
								  "quality				smallint NOT NULL,"
								  "constraint MMS_ImageItemProfile_PK PRIMARY KEY (physicalPathKey), "
								  "constraint MMS_ImageItemProfile_FK foreign key (physicalPathKey) "
								  "references MMS_PhysicalPath (physicalPathKey) on delete cascade) ";
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
			string sqlStatement = "create index if not exists MMS_ImageItemProfile_idx on MMS_ImageItemProfile (physicalPathKey)";
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
			string sqlStatement = "create table if not exists MMS_EncodingJob ("
								  "encodingJobKey  			bigint GENERATED ALWAYS AS IDENTITY,"
								  "ingestionJobKey			bigint NOT NULL,"
								  "type                       text NOT NULL,"
								  "typePriority				smallint NOT NULL,"
								  "parameters					jsonb NOT NULL,"
								  "encodingPriority			smallint NOT NULL,"
								  "encodingJobStart			timestamp without time zone NOT NULL DEFAULT (now() at time zone 'utc'),"
								  "encodingJobEnd             timestamp without time zone NULL,"
								  "encodingProgress			numeric(4,1) NULL,"
								  "status           			text NOT NULL,"
								  "processorMMS               text NULL,"
								  "encoderKey					bigint NULL,"
								  "encodingPid				integer NULL,"
								  "realTimeFrameRate				integer NULL,"
								  "realTimeBitRate				numeric NULL,"
								  "numberOfRestartBecauseOfFailure				integer NULL,"
								  "stagingEncodedAssetPathName text NULL,"
								  "failuresNumber           	smallint NOT NULL,"
								  "isKilled					boolean NULL,"
								  "creationDate				timestamp without time zone DEFAULT (now() at time zone 'utc'),"
								  "utcScheduleStart_virtual	bigint generated always as ((parameters ->> 'utcScheduleStart')::bigint) stored NULL,"
								  "constraint MMS_EncodingJob_PK PRIMARY KEY (encodingJobKey), "
								  "constraint MMS_EncodingJob_FK foreign key (ingestionJobKey) "
								  "references MMS_IngestionJob (ingestionJobKey) on delete cascade) ";
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
			string sqlStatement =
				"create index if not exists MMS_EncodingJob_idx1 on MMS_EncodingJob (status, processorMMS, failuresNumber, encodingJobStart)";
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
			string sqlStatement = "create index if not exists MMS_EncodingJob_idx2 on MMS_EncodingJob (utcScheduleStart_virtual)";
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
			string sqlStatement =
				"create index if not exists MMS_EncodingJob_idx3 on MMS_EncodingJob (typePriority, utcScheduleStart_virtual, encodingPriority)";
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
			string sqlStatement = "create index if not exists MMS_EncodingJob_idx4 on MMS_EncodingJob (ingestionJobKey)";
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
			string sqlStatement = "create table if not exists MMS_DeliveryAuthorization ("
								  "deliveryAuthorizationKey	bigint GENERATED ALWAYS AS IDENTITY,"
								  "userKey    				bigint NOT NULL,"
								  "clientIPAddress			text NULL,"
								  // contentType: vod or live
								  "contentType				text NOT NULL,"
								  // contentKey: physicalPathKey or liveURLConfKey
								  "contentKey					bigint NOT NULL,"
								  "deliveryURI    			text NOT NULL,"
								  "ttlInSeconds               integer NOT NULL,"
								  "currentRetriesNumber       integer NOT NULL,"
								  "maxRetries                 integer NOT NULL,"
								  "authorizationTimestamp		timestamp without time zone default (now() at time zone 'utc'),"
								  "constraint MMS_DeliveryAuthorization_PK PRIMARY KEY (deliveryAuthorizationKey), "
								  "constraint MMS_DeliveryAuthorization_FK foreign key (userKey) "
								  "references MMS_User (userKey) on delete cascade) ";
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
			string sqlStatement =
				"create index if not exists MMS_DeliveryAuthorization_idx1 on MMS_DeliveryAuthorization (contentType, contentKey, deliveryURI)";
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
