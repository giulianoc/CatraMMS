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

#include "Encrypt.h"
#include "JSONUtils.h"

#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <fstream>
#include <sstream>

// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
	json configuration, json slowQueryConfigurationRoot, size_t masterDbPoolSize, size_t slaveDbPoolSize, shared_ptr<spdlog::logger> logger
)
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

	_confirmationCodeExpirationInDays = JSONUtils::asInt(configuration["mms"], "confirmationCodeExpirationInDays", 3);
	_logger->info(__FILEREF__ + "Configuration item" + ", mms->confirmationCodeExpirationInDays: " + to_string(_confirmationCodeExpirationInDays));

	_contentRetentionInMinutesDefaultValue = JSONUtils::asInt(configuration["mms"], "contentRetentionInMinutesDefaultValue", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", mms->contentRetentionInMinutesDefaultValue: " + to_string(_contentRetentionInMinutesDefaultValue)
	);
	/*
	_addContentIngestionJobsNotCompletedRetentionInDays =
		JSONUtils::asInt(configuration["mms"], "addContentIngestionJobsNotCompletedRetentionInDays", 1);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", mms->addContentIngestionJobsNotCompletedRetentionInDays: " + to_string(_addContentIngestionJobsNotCompletedRetentionInDays)
	);
	*/

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
	_ingestionWorkflowCompletedRetentionInDays = JSONUtils::asInt(configuration["postgres"], "ingestionWorkflowCompletedRetentionInDays", 30);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", postgres->ingestionWorkflowCompletedRetentionInDays: " + to_string(_ingestionWorkflowCompletedRetentionInDays)
	);
	_statisticRetentionInMonths = JSONUtils::asInt(configuration["postgres"], "statisticRetentionInMonths", 12);
	_logger->info(__FILEREF__ + "Configuration item" + ", postgres->statisticRetentionInMonths: " + to_string(_statisticRetentionInMonths));
	_statisticsEnabled = JSONUtils::asBool(configuration["postgres"], "statisticsEnabled", true);
	_logger->info(__FILEREF__ + "Configuration item" + ", postgres->statisticsEnabled: " + to_string(_statisticsEnabled));

	{
		string masterDbServer = JSONUtils::asString(configuration["postgres"]["master"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->master->server: " + masterDbServer);
		int masterDbPort = JSONUtils::asInt(configuration["postgres"]["master"], "port", 5432);
		_logger->info(__FILEREF__ + "Configuration item" + ", database->master->port: " + to_string(masterDbPort));
		string slaveDbServer = JSONUtils::asString(configuration["postgres"]["slave"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item" + ", database->slave->server: " + slaveDbServer);
		int slaveDbPort = JSONUtils::asInt(configuration["postgres"]["slave"], "port", 5432);
		_logger->info(__FILEREF__ + "Configuration item" + ", database->slave->port: " + to_string(slaveDbPort));
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
			make_shared<PostgresConnectionFactory>(masterDbServer, masterDbUsername, masterDbPort, dbPassword, dbName, selectTestingConnection);
		_postgresSlaveConnectionFactory =
			make_shared<PostgresConnectionFactory>(slaveDbServer, slaveDbUsername, slaveDbPort, dbPassword, dbName, selectTestingConnection);

		// 2018-04-05: without an open stream the first connection fails
		// 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
		//      Stdout of the spdlog is now false and I commented the ofstream statement
		// ofstream aaa("/tmp/a.txt");
		_logger->info(__FILEREF__ + "Creating MasterDBConnectionPool...");
		_masterPostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(masterDbPoolSize, _postgresMasterConnectionFactory);

		_logger->info(__FILEREF__ + "Creating SlaveDBConnectionPool...");
		_slavePostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(slaveDbPoolSize, _postgresSlaveConnectionFactory);
	}

	// slow query
	loadMaxQueryElapsedConfiguration(slowQueryConfigurationRoot);

	_lastConnectionStatsReport = chrono::system_clock::now();

	if (masterDbPoolSize > 0)
	{
		_logger->info(__FILEREF__ + "createTablesIfNeeded...");
		createTablesIfNeeded();

		_logger->info(__FILEREF__ + "loadSqlColumnsSchema...");
		loadSqlColumnsSchema();
	}
	else
		_logger->warn(__FILEREF__ + "createTablesIfNeeded not done because no master connections");
}

MMSEngineDBFacade::~MMSEngineDBFacade() = default;

void MMSEngineDBFacade::loadMaxQueryElapsedConfiguration(json slowQueryConfigurationRoot)
{
	_defaultMaxQueryElapsed = JSONUtils::asInt(slowQueryConfigurationRoot, "defaultMaxQueryElapsed", 100);
	SPDLOG_DEBUG(
		"Configuration item"
		", defaultMaxQueryElapsed: {}",
		_defaultMaxQueryElapsed
	);

	_maxQueryElapsed.clear();
	if (slowQueryConfigurationRoot != nullptr)
	{
		json maxQueryElapsedRoot = slowQueryConfigurationRoot["maxQueryElapsed"];
		if (maxQueryElapsedRoot != nullptr)
		{
			for (auto &[keyRoot, valRoot] : maxQueryElapsedRoot.items())
			{
				string queryLabel = JSONUtils::asString(json(keyRoot), "", "");
				long maxQueryElapsed = JSONUtils::asInt(valRoot, "", 100);
				_maxQueryElapsed.insert(make_pair(queryLabel, maxQueryElapsed));
			}
		}
	}
}

long MMSEngineDBFacade::maxQueryElapsed(const string queryLabel)
{
	if (queryLabel == "default" || queryLabel.empty())
		return _defaultMaxQueryElapsed;
	else
	{
		auto it = _maxQueryElapsed.find(queryLabel);
		if (it != _maxQueryElapsed.end())
			return it->second;
		else
			return _defaultMaxQueryElapsed;
	}
}

void MMSEngineDBFacade::loadSqlColumnsSchema()
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
		_postgresHelper.loadSqlColumnsSchema(trans);
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

void MMSEngineDBFacade::resetProcessingJobsIfNeeded(string processorMMS)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	/*
	 2024-12-16: Questo metodo viene chiamato quando l'engine service viene fatto partire.
			Serve per ripristinare scenari che altrimenti rimarrebbero in uno stato non corretto.
	 */
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
			SPDLOG_INFO(
				"resetProcessingJobsIfNeeded. Downloading of content (pull mode) not completed"
				", processorMMS: {}",
				processorMMS
			);
			//	In caso di trasferimenti incompleti, resettiamo (Start_TaskQueued e processorMMS null) in modo
			//	che _mmsEngineDBFacade->getIngestionsToBeManaged li possa riprocessare
			string sqlStatement = std::format(
				"update MMS_IngestionJob set status = {}, processorMMS = null "
				"where processorMMS = {} and "
				"status in ({}, {}, {}) and sourceBinaryTransferred = false ",
				trans.transaction->quote(toString(IngestionStatus::Start_TaskQueued)), trans.transaction->quote(processorMMS),
				trans.transaction->quote(toString(IngestionStatus::SourceDownloadingInProgress)),
				trans.transaction->quote(toString(IngestionStatus::SourceMovingInProgress)),
				trans.transaction->quote(toString(IngestionStatus::SourceCopingInProgress))
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

		// 2025-05-21: Scenario: il task Concat-Demuxer è rimasto bloccato sulla move di un file da IngestionRepository a MMSRepository.
		// 	In generale se un task gestito dall'Engine e non da un Encoding rimane incompleto, rimane in stato Start_TaskQueued
		// 	con processorMMS inizializzato. In questo caso, in caso di ripartenza dell'Engine, è necessario resettare processorMMS
		// 	altrimenti, anche il restart dell'Engine non recupera questo task che rimarrebbe incompleto e bloccato da quel processor.
		// 	E' importante che questo reset considera SOLAMENTE i task non gestiti da un EncodingJob, infatti per quelli l'engine
		// 	si ricollega automaticamente
		{
			SPDLOG_INFO(
				"resetProcessingJobsIfNeeded. Tasks managed only by Engine (not by Encoding) not completed"
				", processorMMS: {}",
				processorMMS
			);
			string sqlStatement = std::format(
				"UPDATE MMS_IngestionJob SET processorMMS = null "
				"WHERE processorMMS = {} AND status = {} "
				"AND NOT EXISTS (SELECT 1 FROM MMS_EncodingJob WHERE MMS_IngestionJob.ingestionJobKey = MMS_EncodingJob.ingestionJobKey)",
				trans.transaction->quote(processorMMS), trans.transaction->quote(toString(IngestionStatus::Start_TaskQueued))
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
			/* 2024-12-15:
			 * commentato perchè non la capisco
			 *
			SPDLOG_INFO(
				"resetProcessingJobsIfNeeded. IngestionJobs assigned without final state"
				", processorMMS: {}",
				processorMMS
			);
			// non uso "not like 'End_%'" per motivi di performance
			string sqlStatement = std::format(
				"update MMS_IngestionJob set processorMMS = NULL where processorMMS = {} "
				"and status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress') " // , 'EncodingQueued') ",
				trans.quote(processorMMS)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			*/
		}

		{
			/*
			SPDLOG_INFO(
				"resetProcessingJobsIfNeeded. EncodingJobs assigned with state Processing"
				", processorMMS: {}",
				processorMMS
			);
			// transcoder does not have to be reset because the Engine, in case of restart,
			// is still able to attach to it (encoder). Commento per cui non capisco l'update sotto
			// lastSQLCommand =
			//   "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ?";
			string sqlStatement = std::format(
				"update MMS_EncodingJob set status = {}, processorMMS = null "
				"where processorMMS = {} and status = {} ",
				trans.quote(toString(EncodingStatus::ToBeProcessed)), trans.quote(processorMMS), trans.quote(toString(EncodingStatus::Processing))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			*/
		}

		{
			SPDLOG_INFO(
				"resetProcessingJobsIfNeeded. MediaItems retention assigned"
				", processorMMS: {}",
				processorMMS
			);
			string sqlStatement = std::format(
				"update MMS_MediaItem set processorMMSForRetention = NULL "
				"where processorMMSForRetention = {} ",
				trans.transaction->quote(processorMMS)
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

string MMSEngineDBFacade::nextRelativePathToBeUsed(int64_t workspaceKey)
{
	string relativePathToBeUsed;
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
		int currentDirLevel1;
		int currentDirLevel2;
		int currentDirLevel3;

		{
			string sqlStatement = std::format(
				"select currentDirLevel1, currentDirLevel2, currentDirLevel3 from MMS_WorkspaceMoreInfo where workspaceKey = {}", workspaceKey
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
			/*
			char pCurrentRelativePath[64];

			sprintf(pCurrentRelativePath, "/%03d/%03d/%03d/", currentDirLevel1, currentDirLevel2, currentDirLevel3);

			relativePathToBeUsed = pCurrentRelativePath;
			*/
			relativePathToBeUsed = std::format("/{:0>3}/{:0>3}/{:0>3}/", currentDirLevel1, currentDirLevel2, currentDirLevel3);
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

	return relativePathToBeUsed;
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
MMSEngineDBFacade::getStorageDetails(int64_t physicalPathKey, bool fromMaster)
{
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
			string sqlStatement = std::format(
				"select mi.workspaceKey, mi.contentType, mi.title, mi.deliveryFileName, pp.externalReadOnlyStorage, "
				"pp.encodingProfileKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and pp.physicalPathKey = {} ",
				physicalPathKey
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
			string sqlStatement = std::format(
				"select deliveryTechnology from MMS_EncodingProfile "
				"where encodingProfileKey = {} ",
				encodingProfileKey
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

		shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

		return make_tuple(
			physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes,
			externalReadOnlyStorage
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

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>, string, string, string, string, uint64_t, bool>
MMSEngineDBFacade::getStorageDetails(
	int64_t mediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having the bigger size in case there are more than one)
	int64_t encodingProfileKey, bool fromMaster
)
{
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
			string sqlStatement = std::format(
				"select mi.workspaceKey, mi.title, mi.contentType, mi.deliveryFileName, "
				"pp.externalReadOnlyStorage, pp.physicalPathKey, pp.partitionNumber, "
				"pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = {} "
				"and pp.encodingProfileKey = {}",
				mediaItemKey, encodingProfileKey
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
				string errorMessage = std::format(
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
				string sqlStatement = std::format(
					"select deliveryTechnology from MMS_EncodingProfile "
					"where encodingProfileKey = {} ",
					encodingProfileKey
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
			tuple<int64_t, int, string, string, int64_t, bool, int64_t> sourcePhysicalPathDetails = getSourcePhysicalPath(mediaItemKey, fromMaster);
			tie(physicalPathKey, mmsPartitionNumber, relativePath, fileName, sizeInBytes, externalReadOnlyStorage, ignore) =
				sourcePhysicalPathDetails;

			string sqlStatement = std::format(
				"select workspaceKey, contentType, title, deliveryFileName "
				"from MMS_MediaItem where mediaItemKey = {}",
				mediaItemKey
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
				// 2024-08-17: warn, sara' il chiamante che deciderà se loggare o no l'errore
				// if (warningIfMissing)
				_logger->warn(errorMessage);
				// else
				// 	_logger->error(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

		return make_tuple(
			physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes,
			externalReadOnlyStorage
		);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				me->what(), trans.connection->getConnectionId()
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

void MMSEngineDBFacade::getAllStorageDetails(
	int64_t mediaItemKey, bool fromMaster,
	vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>> &allStorageDetails
)
{
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
		int64_t workspaceKey;
		int64_t encodingProfileKey;
		int mmsPartitionNumber;
		int64_t sizeInBytes;
		int externalReadOnlyStorage;
		string relativePath;
		string fileName;
		ContentType contentType;
		{
			string sqlStatement = std::format(
				"select mi.workspaceKey, mi.contentType, pp.externalReadOnlyStorage, pp.encodingProfileKey, "
				"pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = {} ",
				mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
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
					string sqlStatement = std::format(
						"select deliveryTechnology from MMS_EncodingProfile "
						"where encodingProfileKey = {} ",
						encodingProfileKey
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

// at least one between physicalPathKey and liveDeliveryKey has to be -1
int64_t MMSEngineDBFacade::createDeliveryAuthorization(
	int64_t userKey, string clientIPAddress,
	int64_t physicalPathKey, // vod key
	int64_t liveDeliveryKey, // live key
	string deliveryURI, int ttlInSeconds, int maxRetries, bool reuseAuthIfPresent
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

		int64_t deliveryAuthorizationKey = -1;
		if (reuseAuthIfPresent)
		{
			vector<string> requestedColumns = {"mms_deliveryauthorization:.deliveryauthorizationkey"};
			shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet =
				deliveryAuthorizationQuery(requestedColumns, -1, contentType, contentKey, deliveryURI, true, false, -1, -1, "", false);

			if (!(*sqlResultSet).empty())
				deliveryAuthorizationKey = (*sqlResultSet)[0][0].as<int64_t>(-1);
		}
		if (deliveryAuthorizationKey == -1)
		{
			string sqlStatement = std::format(
				"insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, "
				"contentType, contentKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
				"                                      DEFAULT,                  {},       {}, "
				"{},           {},          {},           {},            0,                    {}) "
				"returning deliveryAuthorizationKey",
				userKey, clientIPAddress == "" ? "null" : trans.transaction->quote(clientIPAddress), trans.transaction->quote(contentType),
				contentKey, trans.transaction->quote(deliveryURI), ttlInSeconds, maxRetries
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			deliveryAuthorizationKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		return deliveryAuthorizationKey;
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

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::deliveryAuthorizationQuery(
	vector<string> &requestedColumns, int64_t deliveryAuthorizationKey, string contentType, int64_t contentKey, string deliveryURI,
	bool notExpiredCheck, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
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
			if (deliveryAuthorizationKey != -1)
				where += std::format("{} deliveryAuthorizationKey = {} ", where.size() > 0 ? "and" : "", deliveryAuthorizationKey);
			if (contentType != "")
				where += std::format("{} contentType = {} ", where.size() > 0 ? "and" : "", trans.transaction->quote(contentType));
			if (contentKey != -1)
				where += std::format("{} contentKey = {} ", where.size() > 0 ? "and" : "", contentKey);
			if (deliveryURI != "")
				where += std::format("{} deliveryURI = {} ", where.size() > 0 ? "and" : "", trans.transaction->quote(deliveryURI));
			if (notExpiredCheck)
			{
				long minTimeToLeaveInSeconds = 3 * 3600; // 3h (non servono 3h in caso di un film?)
				where += std::format(
					"{} extract(epoch from (((authorizationTimestamp + INTERVAL '1 second' * ttlInSeconds) - NOW() at time zone 'utc'))) > {} ",
					where.size() > 0 ? "and" : "", minTimeToLeaveInSeconds
				);
			}

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
				"from MMS_DeliveryAuthorization "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
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
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && deliveryAuthorizationKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"deliveryAuthorization not found"
					", deliveryAuthorizationKey: {}",
					deliveryAuthorizationKey
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
		DBRecordNotFound const *de = dynamic_cast<DBRecordNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (de != nullptr)
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
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

bool MMSEngineDBFacade::checkDeliveryAuthorization(int64_t deliveryAuthorizationKey, string contentURI)
{
	bool authorizationOK = false;
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
				"select deliveryURI, currentRetriesNumber, maxRetries, "
				"extract(epoch from (((authorizationTimestamp + INTERVAL '1 second' * ttlInSeconds) - NOW() at time zone 'utc'))) as "
				"timeToLiveAvailable "
				"from MMS_DeliveryAuthorization "
				"where deliveryAuthorizationKey = {}",
				deliveryAuthorizationKey
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
				/* 2024-06-12: dopo aver inserito il token reusable (per usare la cache della CDN), ho preferito non verificare currentRetriesNumber
				else if (currentRetriesNumber >= maxRetries)
				{
					string errorMessage = __FILEREF__ + "maxRetries is already reached" +
										  ", currentRetriesNumber: " + to_string(currentRetriesNumber) + ", maxRetries: " + to_string(maxRetries) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				*/
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
			string sqlStatement = std::format(
				"update MMS_DeliveryAuthorization set currentRetriesNumber = currentRetriesNumber + 1 "
				"where deliveryAuthorizationKey = {} ",
				deliveryAuthorizationKey
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

	return authorizationOK;
}

bool MMSEngineDBFacade::onceExecution(OnceType onceType)
{
	_logger->info(__FILEREF__ + "onceExecution");
	bool alreadyExecuted;
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
		/*
			this method is called when a procedure, identified by 'oncePerDayKey'
			parameter, has to be executed once a day
			It returns:
				- true if the procedure was already executed for today
				- false if the procedure was NOT already executed for today
					and has to be executed
		*/

		// Poichè l'engine potrebbe essere molto occupato e quindi potrebbe chiamare questo metodo anche
		// con 5 minuti di differenza rispetto all'orario pianificato, verifichiamo se sia stato fatto l'update
		// usando un range di 15 min
		int rangeInMinutes = 15;
		chrono::system_clock::time_point now = chrono::system_clock::now();
		chrono::system_clock::time_point start = now - chrono::minutes(rangeInMinutes);
		chrono::system_clock::time_point end = now + chrono::minutes(rangeInMinutes);
		string sNow;
		string sStart;
		string sEnd;
		{
			tm tmDateTime;

			time_t utcTime = chrono::system_clock::to_time_t(now);
			localtime_r(&utcTime, &tmDateTime);
			sNow = std::format(
				"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday, tmDateTime.tm_hour,
				tmDateTime.tm_min, tmDateTime.tm_sec
			);

			utcTime = chrono::system_clock::to_time_t(start);
			localtime_r(&utcTime, &tmDateTime);
			sStart = std::format(
				"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday, tmDateTime.tm_hour,
				tmDateTime.tm_min, tmDateTime.tm_sec
			);

			utcTime = chrono::system_clock::to_time_t(end);
			localtime_r(&utcTime, &tmDateTime);
			sEnd = std::format(
				"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday, tmDateTime.tm_hour,
				tmDateTime.tm_min, tmDateTime.tm_sec
			);
		}

		{
			string sqlStatement = std::format(
				"update MMS_OnceExecution set lastExecutionTime = {} "
				"where type = {} and (lastExecutionTime is null OR "
				"lastExecutionTime < to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS') "
				"or lastExecutionTime > to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS')) ",
				trans.transaction->quote(sNow), trans.transaction->quote(toString(onceType)), trans.transaction->quote(sStart),
				trans.transaction->quote(sEnd)
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

			alreadyExecuted = (rowsUpdated == 0 ? true : false);
		}

		return alreadyExecuted;
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

string MMSEngineDBFacade::getPostgresArray(const vector<string> &arrayElements, const bool emptyElementToBeRemoved, const PostgresConnTrans &trans)
{
	string postgresArray;
	for (const string& element : arrayElements)
	{
		if (emptyElementToBeRemoved && element.empty())
			continue;
		if (postgresArray.empty())
			postgresArray = trans.transaction->quote(element);
		else
			postgresArray += "," + trans.transaction->quote(element);
	}
	if (postgresArray.empty())
		postgresArray = "ARRAY []::text[]";
	else
		postgresArray = "ARRAY [" + postgresArray + "]";

	return postgresArray;
}

string MMSEngineDBFacade::getPostgresArray(const json& arrayRoot, const bool emptyElementToBeRemoved, const PostgresConnTrans &trans)
{
	string postgresArray;
	for (const auto & index : arrayRoot)
	{
		string element = JSONUtils::asString(index);

		if (emptyElementToBeRemoved && element.empty())
			continue;

		if (postgresArray.empty())
			postgresArray = trans.transaction->quote(element);
		else
			postgresArray += "," + trans.transaction->quote(element);
	}
	if (postgresArray.empty())
		postgresArray = "ARRAY []::text[]";
	else
		postgresArray = "ARRAY [" + postgresArray + "]";

	return postgresArray;
}
