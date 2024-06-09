/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   MMSEngineDBFacade.cpp
 * Author: giuliano
 *
 * Created on January 27, 2018, 9:38 AM
 */

#include "JSONUtils.h"
#include "catralibraries/Encrypt.h"

#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include <fstream>
#include <sstream>

// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(json configuration, size_t masterDbPoolSize, size_t slaveDbPoolSize, shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
	_configuration = configuration;
	_maxRows = 1000;

	_doNotManageIngestionsOlderThanDays = JSONUtils::asInt(configuration["mms"], "doNotManageIngestionsOlderThanDays", 7);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->doNotManageIngestionsOlderThanDays: " + to_string(_doNotManageIngestionsOlderThanDays)
	);

	_ffmpegEncoderUser = JSONUtils::asString(configuration["ffmpeg"], "encoderUser", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser);
	_ffmpegEncoderPassword = JSONUtils::asString(configuration["ffmpeg"], "encoderPassword", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderPassword: " + "...");
	_ffmpegEncoderStatusURI = JSONUtils::asString(configuration["ffmpeg"], "encoderStatusURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI);
	_ffmpegEncoderInfoURI = JSONUtils::asString(configuration["ffmpeg"], "encoderInfoURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderInfoURI: " + _ffmpegEncoderInfoURI);
	_ffmpegEncoderInfoTimeout = JSONUtils::asInt(configuration["ffmpeg"], "encoderInfoTimeout", 2);
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderInfoTimeout: " + to_string(_ffmpegEncoderInfoTimeout));

	_ingestionJobsSelectPageSize = JSONUtils::asInt(configuration["mms"], "ingestionJobsSelectPageSize", 500);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->ingestionJobsSelectPageSize: " + to_string(_ingestionJobsSelectPageSize));

	_maxEncodingFailures = JSONUtils::asInt(configuration["encoding"], "maxEncodingFailures", 3);
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->maxEncodingFailures: " + to_string(_maxEncodingFailures));

	_confirmationCodeRetentionInDays = JSONUtils::asInt(configuration["mms"], "confirmationCodeRetentionInDays", 3);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays));

	_contentRetentionInMinutesDefaultValue = JSONUtils::asInt(configuration["mms"], "contentRetentionInMinutesDefaultValue", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->contentRetentionInMinutesDefaultValue: " + to_string(_contentRetentionInMinutesDefaultValue)
	);
	_contentNotTransferredRetentionInHours = JSONUtils::asInt(configuration["mms"], "contentNotTransferredRetentionInDays", 1);
	_contentNotTransferredRetentionInHours *= 24;
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->contentNotTransferredRetentionInDays*24: " + to_string(_contentNotTransferredRetentionInHours)
	);

	_maxSecondsToWaitUpdateIngestionJobLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitUpdateIngestionJobLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->maxSecondsToWaitUpdateIngestionJobLock: " + to_string(_maxSecondsToWaitUpdateIngestionJobLock)
	);
	_maxSecondsToWaitUpdateEncodingJobLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
	);
	_maxSecondsToWaitCheckIngestionLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitCheckIngestionLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->maxSecondsToWaitCheckIngestionLock: " + to_string(_maxSecondsToWaitCheckIngestionLock)
	);
	_maxSecondsToWaitCheckEncodingJobLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitCheckEncodingJobLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->maxSecondsToWaitCheckEncodingJobLock: " + to_string(_maxSecondsToWaitCheckEncodingJobLock)
	);
	_maxSecondsToWaitMainAndBackupLiveChunkLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitMainAndBackupLiveChunkLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", mms->maxSecondsToWaitMainAndBackupLiveChunkLock: " + to_string(_maxSecondsToWaitMainAndBackupLiveChunkLock)
	);
	_maxSecondsToWaitSetNotToBeExecutedLock = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitSetNotToBeExecutedLock", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->maxSecondsToWaitSetNotToBeExecutedLock,: " + to_string(_maxSecondsToWaitSetNotToBeExecutedLock)
	);

	_predefinedVideoProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "videoDir", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->predefinedProfiles->videoDir: " + _predefinedVideoProfilesDirectoryPath);
	_predefinedAudioProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "audioDir", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->predefinedProfiles->audioDir: " + _predefinedAudioProfilesDirectoryPath);
	_predefinedImageProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "imageDir", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->predefinedProfiles->imageDir: " + _predefinedImageProfilesDirectoryPath);

	_predefinedWorkflowLibraryDirectoryPath = JSONUtils::asString(configuration["mms"], "predefinedWorkflowLibraryDir", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->predefinedWorkflowLibraryDir: " + _predefinedWorkflowLibraryDirectoryPath);

	_geoServiceEnabled = JSONUtils::asBool(configuration["mms"]["geoService"], "enabled", false);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->geoService->enabled: " + to_string(_geoServiceEnabled));
	_geoServiceMaxDaysBeforeUpdate = JSONUtils::asInt(configuration["mms"]["geoService"], "maxDaysBeforeUpdate", 1);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->geoService->maxDaysBeforeUpdate: " + to_string(_geoServiceMaxDaysBeforeUpdate));
	_geoServiceURL = JSONUtils::asString(configuration["mms"]["geoService"], "url", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->geoService->url: " + _geoServiceURL);
	_geoServiceKey = JSONUtils::asString(configuration["mms"]["geoService"], "key", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->geoService->key: " + _geoServiceKey);
	_geoServiceTimeoutInSeconds = JSONUtils::asInt(configuration["mms"]["geoService"], "timeoutInSeconds", 10);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->geoService->timeoutInSeconds: " + to_string(_geoServiceTimeoutInSeconds));

	_getIngestionJobsCurrentIndex = 0;
	_getEncodingJobsCurrentIndex = 0;

	_logger->info(__FILEREF__ + "Looking for adminEmailAddresses");
	for (int adminEmailAddressesIndex = 0; adminEmailAddressesIndex < configuration["api"]["adminEmailAddresses"].size(); adminEmailAddressesIndex++)
	{
		string adminEmailAddress = JSONUtils::asString(configuration["api"]["adminEmailAddresses"][adminEmailAddressesIndex]);
		_adminEmailAddresses.push_back(adminEmailAddress);
		_logger->info(__FILEREF__ + "Configuration item" + ", mms->adminEmailAddresses[adminEmailAddressesIndex]: " + adminEmailAddress);
	}

	_dbConnectionPoolStatsReportPeriodInSeconds = JSONUtils::asInt(configuration["postgres"], "dbConnectionPoolStatsReportPeriodInSeconds", 5);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", postgres->dbConnectionPoolStatsReportPeriodInSeconds: " + to_string(_dbConnectionPoolStatsReportPeriodInSeconds)
	);
	_ingestionWorkflowRetentionInDays = JSONUtils::asInt(configuration["postgres"], "ingestionWorkflowRetentionInDays", 30);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", postgres->ingestionWorkflowRetentionInDays: " + to_string(_ingestionWorkflowRetentionInDays)
	);
	_statisticRetentionInMonths = JSONUtils::asInt(configuration["postgres"], "statisticRetentionInMonths", 12);
	_logger->info(__FILEREF__ + "Configuration item" + ", postgres->statisticRetentionInMonths: " + to_string(_statisticRetentionInMonths));
	_statisticsEnabled = JSONUtils::asBool(configuration["postgres"], "statisticsEnabled", true);
	_logger->info(__FILEREF__ + "Configuration item" + ", postgres->statisticsEnabled: " + to_string(_statisticsEnabled));

	{
		string masterDbServer = JSONUtils::asString(configuration["postgres"]["master"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->master->server: " + masterDbServer);
		string slaveDbServer = JSONUtils::asString(configuration["postgres"]["slave"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->slave->server: " + slaveDbServer);
		string masterDbUsername = JSONUtils::asString(configuration["postgres"]["master"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->master->userName: " + masterDbUsername);
		string slaveDbUsername = JSONUtils::asString(configuration["postgres"]["slave"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->slave->userName: " + slaveDbUsername);
		string dbPassword;
		{
			string encryptedPassword = JSONUtils::asString(configuration["postgres"], "password", "");
			dbPassword = Encrypt::opensslDecrypt(encryptedPassword);
			// dbPassword = encryptedPassword;
		}
		string dbName = JSONUtils::asString(configuration["postgres"], "dbName", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->dbName: " + dbName);
		string selectTestingConnection = JSONUtils::asString(configuration["postgres"], "selectTestingConnection", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->selectTestingConnection: " + selectTestingConnection);

		_logger->info(__FILEREF__ + "Creating PostgresConnectionFactory...");
		bool reconnect = true;
		_postgresMasterConnectionFactory =
			make_shared<PostgresConnectionFactory>(masterDbServer, masterDbUsername, dbPassword, dbName, selectTestingConnection);
		_postgresSlaveConnectionFactory =
			make_shared<PostgresConnectionFactory>(slaveDbServer, slaveDbUsername, dbPassword, dbName, selectTestingConnection);

		// 2018-04-05: without an open stream the first connection fails
		// 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
		//      Stdout of the spdlog is now false and I commented the ofstream statement
		// ofstream aaa("/tmp/a.txt");
		_logger->info(__FILEREF__ + "Creating MasterDBConnectionPool...");
		_masterPostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(masterDbPoolSize, _postgresMasterConnectionFactory);

		_logger->info(__FILEREF__ + "Creating SlaveDBConnectionPool...");
		_slavePostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(slaveDbPoolSize, _postgresSlaveConnectionFactory);
	}

	_lastConnectionStatsReport = chrono::system_clock::now();

	_logger->info(__FILEREF__ + "createTablesIfNeeded...");
	createTablesIfNeeded();

	_logger->info(__FILEREF__ + "loadSqlColumnsSchema...");
	loadSqlColumnsSchema();
}

MMSEngineDBFacade::~MMSEngineDBFacade() {}

void MMSEngineDBFacade::loadSqlColumnsSchema()
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
		_postgresHelper.loadSqlColumnsSchema(conn, &trans);
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

void MMSEngineDBFacade::resetProcessingJobsIfNeeded(string processorMMS)
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
		/*
		  2019-05-04: Found some contents expired but not removed.
			Investigating I find out that the reason the contents were not removed is because they depend on
			ingestion jobs not finished.
			Still investigating those ingestion jobs were in the EncodingQueue status but I find out the
				relative encoding jobs were already finished.
				So for some reasons we have a scenario where ingestion job was not finished but the encoding job was finished.
				Processor fields were null.
		   So next select and update retrieves and "solve" this scenario:

		   select ij.startProcessing, ij.ingestionJobKey, ij.ingestionRootKey, ij.ingestionType, ij.processorMMS,
			ij.status, ej.encodingJobKey, ej.type, ej.status, ej.encodingJobEnd
			from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej
			where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey
			and ij.status not like 'End_%' and ej.status like 'End_%';

			update MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej
			 set ir.lastUpdate = NOW(), ir.status = 'CompletedWithFailures', ij.status = 'End_IngestionFailure',
			 ij.errorMessage = 'Found Encoding finished but Ingestion not finished'
			 where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey
			 and ij.status not like 'End_%' and ej.status like 'End_%';
		 */

		// 2022-09-27: next procedure should be already covered by retentionOfIngestionData,
		//		anyway, we will leave it here
		{
			_logger->info(
				__FILEREF__ + "resetProcessingJobsIfNeeded. Downloading of IngestionJobs not completed" + ", processorMMS: " + processorMMS
			);
			// 2021-07-17: Scenario:
			//	1. added the new MMS_0003 partition
			//	2. restarted the engine during the upload of a content
			//	Found the following combination:
			//		processorMMS is NULL, status is SourceDownloadingInProgress, sourceBinaryTransferred is 0
			//	Since we cannot have the above combination (processorMMS is NULL, status is SourceDownloadingInProgress)
			//	then next update was changed to consider also processorMMS as null
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set status = {} "
				"where (processorMMS is NULL or processorMMS = {}) and "
				"status in ({}, {}, {}) and sourceBinaryTransferred = false "
				"returning 1) select count(*) from rows",
				trans.quote(toString(IngestionStatus::Start_TaskQueued)), trans.quote(processorMMS),
				trans.quote(toString(IngestionStatus::SourceDownloadingInProgress)), trans.quote(toString(IngestionStatus::SourceMovingInProgress)),
				trans.quote(toString(IngestionStatus::SourceCopingInProgress))
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
		}

		// 2022-09-27: moved to retentionOfIngestionData.
		//		Scenario: we had a lot of Start_TaskQueued too old that were not 'clean' because the engine
		//			was not restarted. All these 'Start_TaskQueued too old' were blocking all the other tasks.
		//			For this reason we moved this clean procedure in retentionOfIngestionData that is call
		//			once a day
		/*
		{
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. IngestionJobs not completed "
				+ "(state Start_TaskQueued) and too old to be considered "
				+ "by MMSEngineDBFacade::getIngestionsToBeManaged"
				+ ", processorMMS: " + processorMMS
			);
			// 2021-07-17: In this scenario the IngestionJobs would remain infinite time:
			lastSQLCommand =
				"select ingestionJobKey from MMS_IngestionJob "
				"where (processorMMS is NULL or processorMMS = ?) "
				"and status = ? and NOW() > DATE_ADD(processingStartingFrom, INTERVAL ? DAY)";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, processorMMS);
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
			preparedStatement->setInt(queryParameterIndex++, _doNotManageIngestionsOlderThanDays);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. IngestionJobs not completed)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMS: " + processorMMS
				+ ", IngestionStatus::Start_TaskQueued: " + MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued)
				+ ", _doNotManageIngestionsOlderThanDays: " + to_string(_doNotManageIngestionsOlderThanDays)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			while (resultSet->next())
			{
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

				string errorMessage = "Canceled by MMS because not completed and too old";
				try
				{
					_logger->info(__FILEREF__ + "Update IngestionJob"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + "End_CanceledByMMS"
						+ ", errorMessage: " + errorMessage
					);
					updateIngestionJob (ingestionJobKey,
						MMSEngineDBFacade::IngestionStatus::End_CanceledByMMS,
						errorMessage);
				}
				catch(runtime_error& e)
				{
					_logger->error(__FILEREF__ + "reset updateIngestionJob failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + "End_CanceledByMMS"
						+ ", errorMessage: " + errorMessage
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception& e)
				{
					_logger->error(__FILEREF__ + "reset updateIngestionJob failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + "End_CanceledByMMS"
						+ ", errorMessage: " + errorMessage
						+ ", e.what(): " + e.what()
					);
				}
			}
		}
		*/

		{
			_logger->info(
				__FILEREF__ + "resetProcessingJobsIfNeeded. IngestionJobs assigned without final state" + ", processorMMS: " + processorMMS
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set processorMMS = NULL where processorMMS = {} "
				"and status not like 'End_%' returning 1) select count(*) from rows",
				trans.quote(processorMMS)
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
		}

		{
			_logger->info(
				__FILEREF__ + "resetProcessingJobsIfNeeded. EncodingJobs assigned with state Processing" + ", processorMMS: " + processorMMS
			);
			// transcoder does not have to be reset because the Engine, in case of restart,
			// is still able to attach to it (encoder)
			// lastSQLCommand =
			//   "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ?";
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = null "
				"where processorMMS = {} and status = {} returning 1) select count(*) from rows",
				trans.quote(toString(EncodingStatus::ToBeProcessed)), trans.quote(processorMMS), trans.quote(toString(EncodingStatus::Processing))
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
		}

		{
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. MediaItems retention assigned" + ", processorMMS: " + processorMMS);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_MediaItem set processorMMSForRetention = NULL "
				"where processorMMSForRetention = {} returning 1) select count(*) from rows",
				trans.quote(processorMMS)
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
		}

		// 2022-09-27: non sono sicuro che la prossima procedura sia corretta:
		//	- nel caso di LiveRecorder, LiveProxy, ..., questi task potrebbero durare tanto tempo
		//	- anche nel caso di un encoding, nella ripartenza abbiamo le chiamate a encodingStatus che
		//		dovrebbero gestire questo scenario
		//	Provo a commentarlo
		/*
		{
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. Old EncodingJobs retention Processing"
					+ ", processorMMS: " + processorMMS
					);
			int retentionDaysToReset = 7;

			lastSQLCommand =
				"update MMS_EncodingJob set status = ?, processorMMS = null where processorMMS = ? and status = ? "
				"and DATE_ADD(encodingJobStart, INTERVAL ? DAY) <= NOW()";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
			preparedStatement->setString(queryParameterIndex++, processorMMS);
			preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
			preparedStatement->setInt(queryParameterIndex++, retentionDaysToReset);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. Old EncodingJobs retention Processing)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", processorMMS: " + processorMMS
				+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
				+ ", retentionDaysToReset: " + to_string(retentionDaysToReset)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
		}
		*/

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

string MMSEngineDBFacade::nextRelativePathToBeUsed(int64_t workspaceKey)
{
	string relativePathToBeUsed;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		int currentDirLevel1;
		int currentDirLevel2;
		int currentDirLevel3;

		{
			string sqlStatement = fmt::format(
				"select currentDirLevel1, currentDirLevel2, currentDirLevel3 from MMS_WorkspaceMoreInfo where workspaceKey = {}", workspaceKey
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
				currentDirLevel1 = res[0]["currentDirLevel1"].as<int>();
				currentDirLevel2 = res[0]["currentDirLevel2"].as<int>();
				currentDirLevel3 = res[0]["currentDirLevel3"].as<int>();
			}
			else
			{
				string errorMessage = __FILEREF__ + "Workspace is not present/configured" + ", workspaceKey: " + to_string(workspaceKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			char pCurrentRelativePath[64];

			sprintf(pCurrentRelativePath, "/%03d/%03d/%03d/", currentDirLevel1, currentDirLevel2, currentDirLevel3);

			relativePathToBeUsed = pCurrentRelativePath;
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

	return relativePathToBeUsed;
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
MMSEngineDBFacade::getStorageDetails(int64_t physicalPathKey, bool fromMaster)
{
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

	try
	{
		int64_t workspaceKey;
		bool externalReadOnlyStorage;
		int64_t encodingProfileKey;
		int mmsPartitionNumber;
		string relativePath;
		string fileName;
		uint64_t sizeInBytes;
		string deliveryFileName;
		string title;
		ContentType contentType;

		{
			string sqlStatement = fmt::format(
				"select mi.workspaceKey, mi.contentType, mi.title, mi.deliveryFileName, pp.externalReadOnlyStorage, "
				"pp.encodingProfileKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and pp.physicalPathKey = {} ",
				physicalPathKey
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
				workspaceKey = res[0]["workspaceKey"].as<int64_t>();
				contentType = toContentType(res[0]["contentType"].as<string>());
				title = res[0]["title"].as<string>();
				if (!res[0]["deliveryFileName"].is_null())
					deliveryFileName = res[0]["deliveryFileName"].as<string>();
				externalReadOnlyStorage = res[0]["externalReadOnlyStorage"].as<bool>();
				if (res[0]["encodingProfileKey"].is_null())
					encodingProfileKey = -1;
				else
					encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
				mmsPartitionNumber = res[0]["partitionNumber"].as<int>();
				relativePath = res[0]["relativePath"].as<string>();
				fileName = res[0]["fileName"].as<string>();
				sizeInBytes = res[0]["sizeInBytes"].as<uint64_t>();
			}
			else
			{
				string errorMessage = __FILEREF__ + "physicalPathKey is not present" + ", physicalPathKey: " + to_string(physicalPathKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// default
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
		if (contentType == ContentType::Video || contentType == ContentType::Audio)
			deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
		else
			deliveryTechnology = DeliveryTechnology::Download;

		if (encodingProfileKey != -1)
		{
			string sqlStatement = fmt::format(
				"select deliveryTechnology from MMS_EncodingProfile "
				"where encodingProfileKey = {} ",
				encodingProfileKey
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
				deliveryTechnology = toDeliveryTechnology(res[0]["deliveryTechnology"].as<string>());
			else
			{
				string errorMessage = __FILEREF__ + "encodingProfileKey is not present" + ", encodingProfileKey: " + to_string(encodingProfileKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "getStorageDetails" + ", deliveryTechnology: " + toString(deliveryTechnology) + ", physicalPathKey: " +
			to_string(physicalPathKey) + ", workspaceKey: " + to_string(workspaceKey) + ", mmsPartitionNumber: " + to_string(mmsPartitionNumber) +
			", relativePath: " + relativePath + ", fileName: " + fileName + ", deliveryFileName: " + deliveryFileName + ", title: " + title +
			", sizeInBytes: " + to_string(sizeInBytes) + ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
		);

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

		return make_tuple(
			physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes,
			externalReadOnlyStorage
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

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
MMSEngineDBFacade::getStorageDetails(
	int64_t mediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having the bigger size in case there are more than one)
	int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster
)
{
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

	try
	{
		MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;

		int64_t workspaceKey = -1;
		int64_t physicalPathKey = -1;
		int mmsPartitionNumber;
		bool externalReadOnlyStorage;
		string relativePath;
		string fileName;
		uint64_t sizeInBytes;
		string deliveryFileName;
		string title;
		ContentType contentType;
		if (encodingProfileKey != -1)
		{
			string sqlStatement = fmt::format(
				"select mi.workspaceKey, mi.title, mi.contentType, mi.deliveryFileName, "
				"pp.externalReadOnlyStorage, pp.physicalPathKey, pp.partitionNumber, "
				"pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = {} "
				"and pp.encodingProfileKey = {}",
				mediaItemKey, encodingProfileKey
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
				workspaceKey = res[0]["workspaceKey"].as<int64_t>();
				title = res[0]["title"].as<string>();
				contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
				if (!res[0]["deliveryFileName"].is_null())
					deliveryFileName = res[0]["deliveryFileName"].as<string>();
				externalReadOnlyStorage = res[0]["externalReadOnlyStorage"].as<bool>();
				physicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
				mmsPartitionNumber = res[0]["partitionNumber"].as<int>();
				relativePath = res[0]["relativePath"].as<string>();
				fileName = res[0]["fileName"].as<string>();
				sizeInBytes = res[0]["sizeInBytes"].as<uint64_t>();
			}

			if (physicalPathKey == -1)
			{
				// warn perchè già loggato come errore nel catch sotto
				string errorMessage = fmt::format(
					"MediaItemKey/EncodingProfileKey are not present"
					", mediaItemKey: {}"
					", encodingProfileKey: {}"
					", sqlStatement: {}",
					mediaItemKey, encodingProfileKey, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}

			// default
			if (contentType == ContentType::Video || contentType == ContentType::Audio)
				deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
			else
				deliveryTechnology = DeliveryTechnology::Download;

			{
				string sqlStatement = fmt::format(
					"select deliveryTechnology from MMS_EncodingProfile "
					"where encodingProfileKey = {} ",
					encodingProfileKey
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
					deliveryTechnology = toDeliveryTechnology(res[0]["deliveryTechnology"].as<string>());
				else
				{
					string errorMessage = __FILEREF__ + "encodingProfileKey is not present" +
										  ", encodingProfileKey: " + to_string(encodingProfileKey) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else
		{
			tuple<int64_t, int, string, string, int64_t, bool, int64_t> sourcePhysicalPathDetails =
				getSourcePhysicalPath(mediaItemKey, warningIfMissing, fromMaster);
			tie(physicalPathKey, mmsPartitionNumber, relativePath, fileName, sizeInBytes, externalReadOnlyStorage, ignore) =
				sourcePhysicalPathDetails;

			string sqlStatement = fmt::format(
				"select workspaceKey, contentType, title, deliveryFileName "
				"from MMS_MediaItem where mediaItemKey = {}",
				mediaItemKey
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
				workspaceKey = res[0]["workspaceKey"].as<int64_t>();
				title = res[0]["title"].as<string>();
				contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
				if (!res[0]["deliveryFileName"].is_null())
					deliveryFileName = res[0]["deliveryFileName"].as<string>();

				// default
				MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
				if (contentType == ContentType::Video || contentType == ContentType::Audio)
					deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
				else
					deliveryTechnology = DeliveryTechnology::Download;
			}

			if (workspaceKey == -1)
			{
				string errorMessage = __FILEREF__ + "MediaItemKey/EncodingProfileKey are not present" + ", mediaItemKey: " + to_string(mediaItemKey) +
									  ", sqlStatement: " + sqlStatement;
				if (warningIfMissing)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

		return make_tuple(
			physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes,
			externalReadOnlyStorage
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
	catch (MediaItemKeyNotFound &e)
	{
		if (warningIfMissing)
			SPDLOG_WARN(
				"MediaItemKeyNotFound, SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
		else
			SPDLOG_ERROR(
				"MediaItemKeyNotFound, SQL exception"
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

void MMSEngineDBFacade::getAllStorageDetails(
	int64_t mediaItemKey, bool fromMaster,
	vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>> &allStorageDetails
)
{
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

	try
	{
		int64_t workspaceKey;
		int64_t encodingProfileKey;
		int mmsPartitionNumber;
		int64_t sizeInBytes;
		int externalReadOnlyStorage;
		string relativePath;
		string fileName;
		ContentType contentType;
		{
			string sqlStatement = fmt::format(
				"select mi.workspaceKey, mi.contentType, pp.externalReadOnlyStorage, pp.encodingProfileKey, "
				"pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = {} ",
				mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				workspaceKey = row["workspaceKey"].as<int64_t>();
				contentType = toContentType(row["contentType"].as<string>());
				externalReadOnlyStorage = row["externalReadOnlyStorage"].as<bool>();
				if (row["encodingProfileKey"].is_null())
					encodingProfileKey = -1;
				else
					encodingProfileKey = row["encodingProfileKey"].as<int64_t>();
				mmsPartitionNumber = row["partitionNumber"].as<int>();
				relativePath = row["relativePath"].as<string>();
				fileName = row["fileName"].as<string>();
				sizeInBytes = row["sizeInBytes"].as<int64_t>();

				shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

				// default
				MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
				if (contentType == ContentType::Video || contentType == ContentType::Audio)
					deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
				else
					deliveryTechnology = DeliveryTechnology::Download;

				if (encodingProfileKey != -1)
				{
					string sqlStatement = fmt::format(
						"select deliveryTechnology from MMS_EncodingProfile "
						"where encodingProfileKey = {} ",
						encodingProfileKey
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
						deliveryTechnology = toDeliveryTechnology(res[0]["deliveryTechnology"].as<string>());
					else
					{
						string errorMessage = __FILEREF__ + "encodingProfileKey is not present" +
											  ", encodingProfileKey: " + to_string(encodingProfileKey) + ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool> storageDetails = make_tuple(
					deliveryTechnology, mmsPartitionNumber, workspace->_directoryName, relativePath, fileName, sizeInBytes, externalReadOnlyStorage
				);

				allStorageDetails.push_back(storageDetails);
			}
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
}

// at least one between physicalPathKey and liveDeliveryKey has to be -1
int64_t MMSEngineDBFacade::createDeliveryAuthorization(
	int64_t userKey, string clientIPAddress,
	int64_t physicalPathKey, // vod key
	int64_t liveDeliveryKey, // live key
	string deliveryURI, int ttlInSeconds, int maxRetries
)
{
	int64_t deliveryAuthorizationKey;
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
			string contentType;
			int64_t contentKey;
			if (physicalPathKey == -1)
			{
				contentType = "live";
				contentKey = liveDeliveryKey;
			}
			else
			{
				contentType = "vod";
				contentKey = physicalPathKey;
			}
			string sqlStatement = fmt::format(
				"insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, "
				"contentType, contentKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
				"                                      DEFAULT,                  {},       {}, "
				"{},           {},          {},           {},            0,                    {}) "
				"returning deliveryAuthorizationKey",
				userKey, clientIPAddress == "" ? "null" : trans.quote(clientIPAddress), trans.quote(contentType), contentKey,
				trans.quote(deliveryURI), ttlInSeconds, maxRetries
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			deliveryAuthorizationKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return deliveryAuthorizationKey;
}

bool MMSEngineDBFacade::checkDeliveryAuthorization(int64_t deliveryAuthorizationKey, string contentURI)
{
	bool authorizationOK = false;
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
				"select deliveryURI, currentRetriesNumber, maxRetries, "
				"extract(epoch from (((authorizationTimestamp + INTERVAL '1 second' * ttlInSeconds) - NOW() at time zone 'utc'))) as "
				"timeToLiveAvailable "
				"from MMS_DeliveryAuthorization "
				"where deliveryAuthorizationKey = {}",
				deliveryAuthorizationKey
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
				string deliveryURI = res[0]["deliveryURI"].as<string>();
				int currentRetriesNumber = res[0]["currentRetriesNumber"].as<int>();
				int maxRetries = res[0]["maxRetries"].as<int>();
				int timeToLiveAvailable = res[0]["timeToLiveAvailable"].as<float>();

				if (contentURI != deliveryURI)
				{
					string errorMessage = __FILEREF__ + "contentURI and deliveryURI are different" + ", contentURI: " + contentURI +
										  ", deliveryURI: " + deliveryURI + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (currentRetriesNumber >= maxRetries)
				{
					string errorMessage = __FILEREF__ + "maxRetries is already reached" +
										  ", currentRetriesNumber: " + to_string(currentRetriesNumber) + ", maxRetries: " + to_string(maxRetries) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (timeToLiveAvailable < 0)
				{
					string errorMessage =
						__FILEREF__ + "TTL expired" + ", timeToLiveAvailable: " + to_string(timeToLiveAvailable) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = __FILEREF__ + "deliveryAuthorizationKey not found" +
									  ", deliveryAuthorizationKey: " + to_string(deliveryAuthorizationKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			authorizationOK = true;
		}

		if (authorizationOK)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_DeliveryAuthorization set currentRetriesNumber = currentRetriesNumber + 1 "
				"where deliveryAuthorizationKey = {} returning 1) select count(*) from rows",
				deliveryAuthorizationKey
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

	return authorizationOK;
}

void MMSEngineDBFacade::retentionOfDeliveryAuthorization()
{
	_logger->info(__FILEREF__ + "retentionOfDeliveryAuthorization");
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
			// Once authorization is expired, we will still take it for 1 day
			int retention = 3600 * 24;

			// we will remove by steps to avoid error because of transaction log overflow
			int maxToBeRemoved = 100;
			int totalRowsRemoved = 0;
			bool moreRowsToBeRemoved = true;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeRemoved)
			{
				try
				{
					string sqlStatement = fmt::format(
						"WITH rows AS (delete from MMS_DeliveryAuthorization where deliveryAuthorizationKey in "
						"(select deliveryAuthorizationKey from MMS_DeliveryAuthorization "
						"where (authorizationTimestamp + INTERVAL '1 second' * (ttlInSeconds + {})) < NOW() at time zone 'utc' limit {}) "
						"returning 1) select count(*) from rows",
						retention, maxToBeRemoved
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
					totalRowsRemoved += rowsUpdated;
					if (rowsUpdated == 0)
						moreRowsToBeRemoved = false;

					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					currentRetriesOnError++;
					if (currentRetriesOnError >= maxRetriesOnError)
						throw e;

					SPDLOG_ERROR(
						"SQL exception, Deadlock!!!"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
					);

					int secondsBetweenRetries = 15;
					_logger->info(
						__FILEREF__ + "retentionOfDeliveryAuthorization failed, " + "waiting before to try again" +
						", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
						", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}

			_logger->info(__FILEREF__ + "Deletion obsolete DeliveryAuthorization" + ", totalRowsRemoved: " + to_string(totalRowsRemoved));
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

bool MMSEngineDBFacade::oncePerDayExecution(OncePerDayType oncePerDayType)
{
	_logger->info(__FILEREF__ + "oncePerDayExecution");
	bool alreadyExecuted;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		/*
			this method is called when a procedure, identified by 'oncePerDayKey'
			parameter, has to be executed once a day
			It returns:
				- true if the procedure was already executed for today
				- false if the procedure was NOT already executed for today
					and has to be executed
		*/

		string today_yyyy_mm_dd;
		{
			tm tmDateTime;
			char strDateTime[64];
			time_t utcTime = chrono::system_clock::to_time_t(chrono::system_clock::now());

			localtime_r(&utcTime, &tmDateTime);

			sprintf(strDateTime, "%04d-%02d-%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday);
			today_yyyy_mm_dd = strDateTime;
		}

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_OncePerDayExecution set lastExecutionTime = {} "
				"where type = {} returning 1) select count(*) from rows",
				trans.quote(today_yyyy_mm_dd), trans.quote(toString(oncePerDayType))
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

			alreadyExecuted = (rowsUpdated == 0 ? true : false);
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return alreadyExecuted;
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

MMSEngineDBFacade::DeliveryTechnology MMSEngineDBFacade::fileFormatToDeliveryTechnology(string fileFormat)
{
	MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
	string fileFormatLowerCase;

	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(), [](unsigned char c) { return tolower(c); });

	if (fileFormatLowerCase == "mp4" || fileFormatLowerCase == "mkv" || fileFormatLowerCase == "mov" || fileFormatLowerCase == "webm")
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::DownloadAndStreaming;
	else if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "m3u8" || fileFormatLowerCase == "dash")
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming;
	else if (fileFormatLowerCase == "ts" || fileFormatLowerCase == "mts" || fileFormatLowerCase == "avi")
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::Download;
	else
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::Download;

	return deliveryTechnology;
}

string MMSEngineDBFacade::getPostgresArray(vector<string> &arrayElements, bool emptyElementToBeRemoved, transaction_base *trans)
{
	string postgresArray;
	for (string element : arrayElements)
	{
		if (emptyElementToBeRemoved && element == "")
			continue;
		if (postgresArray == "")
			postgresArray = trans->quote(element);
		else
			postgresArray += "," + trans->quote(element);
	}
	if (postgresArray == "")
		postgresArray = "ARRAY []::text[]";
	else
		postgresArray = "ARRAY [" + postgresArray + "]";

	return postgresArray;
}

string MMSEngineDBFacade::getPostgresArray(json arrayRoot, bool emptyElementToBeRemoved, transaction_base *trans)
{
	string postgresArray;
	for (int index = 0; index < arrayRoot.size(); index++)
	{
		string element = JSONUtils::asString(arrayRoot[index]);

		if (emptyElementToBeRemoved && element == "")
			continue;

		if (postgresArray == "")
			postgresArray = trans->quote(element);
		else
			postgresArray += "," + trans->quote(element);
	}
	if (postgresArray == "")
		postgresArray = "ARRAY []::text[]";
	else
		postgresArray = "ARRAY [" + postgresArray + "]";

	return postgresArray;
}
