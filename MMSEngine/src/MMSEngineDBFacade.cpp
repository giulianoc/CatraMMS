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

#include "PersistenceLock.h"
#include "MMSEngineDBFacade.h"
#include <fstream>
#include <sstream>


// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
	Json::Value configuration,
	size_t masterDbPoolSize,
	size_t slaveDbPoolSize,
	shared_ptr<spdlog::logger> logger) 
{
    _logger			= logger;
	_configuration	= configuration;

	_defaultContentProviderName     = "default";

    _dbConnectionPoolStatsReportPeriodInSeconds = JSONUtils::asInt(configuration["database"], "dbConnectionPoolStatsReportPeriodInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->dbConnectionPoolStatsReportPeriodInSeconds: " + to_string(_dbConnectionPoolStatsReportPeriodInSeconds)
    );

    _ingestionWorkflowRetentionInDays = JSONUtils::asInt(configuration["database"], "ingestionWorkflowRetentionInDays", 30);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->ingestionWorkflowRetentionInDays: " + to_string(_ingestionWorkflowRetentionInDays)
    );
    _statisticRetentionInMonths = JSONUtils::asInt(configuration["database"], "statisticRetentionInMonths", 12);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->statisticRetentionInMonths: " + to_string(_statisticRetentionInMonths)
    );
    _statisticsEnabled = JSONUtils::asBool(configuration["database"], "statisticsEnabled", true);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->statisticsEnabled: " + to_string(_statisticsEnabled)
    );
    _doNotManageIngestionsOlderThanDays = JSONUtils::asInt(configuration["mms"], "doNotManageIngestionsOlderThanDays", 7);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->doNotManageIngestionsOlderThanDays: " + to_string(_doNotManageIngestionsOlderThanDays)
    );

	_ffmpegEncoderUser = JSONUtils::asString(configuration["ffmpeg"], "encoderUser", "");
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
	);
	_ffmpegEncoderPassword = JSONUtils::asString(configuration["ffmpeg"], "encoderPassword", "");
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderPassword: " + "..."
	);
	_ffmpegEncoderStatusURI = JSONUtils::asString(configuration["ffmpeg"], "encoderStatusURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI
    );
	_ffmpegEncoderInfoURI = JSONUtils::asString(configuration["ffmpeg"], "encoderInfoURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderInfoURI: " + _ffmpegEncoderInfoURI
    );
	_ffmpegEncoderInfoTimeout = JSONUtils::asInt(configuration["ffmpeg"],
		"encoderInfoTimeout", 2);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderInfoTimeout: " + to_string(_ffmpegEncoderInfoTimeout)
    );

    _ingestionJobsSelectPageSize = JSONUtils::asInt(configuration["mms"], "ingestionJobsSelectPageSize", 500);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->ingestionJobsSelectPageSize: " + to_string(_ingestionJobsSelectPageSize)
    );

    _maxEncodingFailures            = JSONUtils::asInt(configuration["encoding"], "maxEncodingFailures", 3);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->maxEncodingFailures: " + to_string(_maxEncodingFailures)
    );

    _confirmationCodeRetentionInDays    = JSONUtils::asInt(configuration["mms"], "confirmationCodeRetentionInDays", 3);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
    );

    _contentRetentionInMinutesDefaultValue    = JSONUtils::asInt(configuration["mms"], "contentRetentionInMinutesDefaultValue", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentRetentionInMinutesDefaultValue: " + to_string(_contentRetentionInMinutesDefaultValue)
    );
    _contentNotTransferredRetentionInHours    = JSONUtils::asInt(configuration["mms"], "contentNotTransferredRetentionInDays", 1);
	_contentNotTransferredRetentionInHours	*= 24;
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentNotTransferredRetentionInDays*24: " + to_string(_contentNotTransferredRetentionInHours)
    );

    _maxSecondsToWaitUpdateIngestionJobLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitUpdateIngestionJobLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitUpdateIngestionJobLock: " + to_string(_maxSecondsToWaitUpdateIngestionJobLock)
    );
    _maxSecondsToWaitUpdateEncodingJobLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
    );
    _maxSecondsToWaitCheckIngestionLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitCheckIngestionLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitCheckIngestionLock: " + to_string(_maxSecondsToWaitCheckIngestionLock)
    );
    _maxSecondsToWaitCheckEncodingJobLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitCheckEncodingJobLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitCheckEncodingJobLock: " + to_string(_maxSecondsToWaitCheckEncodingJobLock)
    );
    _maxSecondsToWaitMainAndBackupLiveChunkLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitMainAndBackupLiveChunkLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitMainAndBackupLiveChunkLock: " + to_string(_maxSecondsToWaitMainAndBackupLiveChunkLock)
    );
    _maxSecondsToWaitSetNotToBeExecutedLock    = JSONUtils::asInt(configuration["mms"]["locks"], "maxSecondsToWaitSetNotToBeExecutedLock", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->maxSecondsToWaitSetNotToBeExecutedLock,: " + to_string(_maxSecondsToWaitSetNotToBeExecutedLock)
    );

    _predefinedVideoProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "videoDir", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->videoDir: " + _predefinedVideoProfilesDirectoryPath
    );
    _predefinedAudioProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "audioDir", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->audioDir: " + _predefinedAudioProfilesDirectoryPath
    );
    _predefinedImageProfilesDirectoryPath = JSONUtils::asString(configuration["encoding"]["predefinedProfiles"], "imageDir", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->imageDir: " + _predefinedImageProfilesDirectoryPath
    );

    _predefinedWorkflowLibraryDirectoryPath = JSONUtils::asString(configuration["mms"], "predefinedWorkflowLibraryDir", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->predefinedWorkflowLibraryDir: " + _predefinedWorkflowLibraryDirectoryPath
    );

	_getIngestionJobsCurrentIndex	= 0;
	_getEncodingJobsCurrentIndex	= 0;

	_logger->info(__FILEREF__ + "Looking for adminEmailAddresses");
	for(int adminEmailAddressesIndex = 0;
			adminEmailAddressesIndex < configuration["api"]["adminEmailAddresses"].size();
			adminEmailAddressesIndex++)
	{
		string adminEmailAddress = JSONUtils::asString(configuration["api"]["adminEmailAddresses"]
			[adminEmailAddressesIndex]);
		_adminEmailAddresses.push_back(adminEmailAddress);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", mms->adminEmailAddresses[adminEmailAddressesIndex]: " + adminEmailAddress
		);
	}

	{
		string masterDbServer = JSONUtils::asString(configuration["database"]["master"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->server: " + masterDbServer
		);
		string slaveDbServer = JSONUtils::asString(configuration["database"]["slave"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->server: " + slaveDbServer
		);
		string defaultCharacterSet = JSONUtils::asString(configuration["database"], "defaultCharacterSet", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->defaultCharacterSet: " + defaultCharacterSet
		);
		string masterDbUsername = JSONUtils::asString(configuration["database"]["master"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->userName: " + masterDbUsername
		);
		string slaveDbUsername = JSONUtils::asString(configuration["database"]["slave"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->userName: " + slaveDbUsername
		);
		string dbPassword;
		{
			string encryptedPassword = JSONUtils::asString(configuration["database"], "password", "");
			dbPassword = Encrypt::opensslDecrypt(encryptedPassword);        
			// dbPassword = encryptedPassword;
		}    
		string dbName = JSONUtils::asString(configuration["database"], "dbName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->dbName: " + dbName
		);
		string selectTestingConnection = JSONUtils::asString(configuration["database"], "selectTestingConnection", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->selectTestingConnection: " + selectTestingConnection
		);

		_logger->info(__FILEREF__ + "Creating MySQLConnectionFactory...");
		bool reconnect = true;
		_mySQLMasterConnectionFactory = 
			make_shared<MySQLConnectionFactory>(masterDbServer, masterDbUsername, dbPassword, dbName,
				reconnect, defaultCharacterSet, selectTestingConnection);
		_mySQLSlaveConnectionFactory = 
			make_shared<MySQLConnectionFactory>(slaveDbServer, slaveDbUsername, dbPassword, dbName,
				reconnect, defaultCharacterSet, selectTestingConnection);

		// 2018-04-05: without an open stream the first connection fails
		// 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
		//      Stdout of the spdlog is now false and I commented the ofstream statement
		// ofstream aaa("/tmp/a.txt");
		_logger->info(__FILEREF__ + "Creating MasterDBConnectionPool...");
		_masterConnectionPool = make_shared<DBConnectionPool<MySQLConnection>>(
			masterDbPoolSize, _mySQLMasterConnectionFactory);

		_logger->info(__FILEREF__ + "Creating SlaveDBConnectionPool...");
		_slaveConnectionPool = make_shared<DBConnectionPool<MySQLConnection>>(
			slaveDbPoolSize, _mySQLSlaveConnectionFactory);
	}
	{
		string masterDbServer = JSONUtils::asString(configuration["postgres"]["master"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->server: " + masterDbServer
		);
		string slaveDbServer = JSONUtils::asString(configuration["postgres"]["slave"], "server", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->server: " + slaveDbServer
		);
		string masterDbUsername = JSONUtils::asString(configuration["postgres"]["master"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->master->userName: " + masterDbUsername
		);
		string slaveDbUsername = JSONUtils::asString(configuration["postgres"]["slave"], "userName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->slave->userName: " + slaveDbUsername
		);
		string dbPassword;
		{
			string encryptedPassword = JSONUtils::asString(configuration["postgres"], "password", "");
			dbPassword = Encrypt::opensslDecrypt(encryptedPassword);        
			// dbPassword = encryptedPassword;
		}    
		string dbName = JSONUtils::asString(configuration["postgres"], "dbName", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->dbName: " + dbName
		);
		string selectTestingConnection = JSONUtils::asString(configuration["postgres"], "selectTestingConnection", "");
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", database->selectTestingConnection: " + selectTestingConnection
		);

		_logger->info(__FILEREF__ + "Creating PostgresConnectionFactory...");
		bool reconnect = true;
		_postgresMasterConnectionFactory = 
			make_shared<PostgresConnectionFactory>(masterDbServer, masterDbUsername, dbPassword, dbName,
				selectTestingConnection);
		_postgresSlaveConnectionFactory = 
			make_shared<PostgresConnectionFactory>(slaveDbServer, slaveDbUsername, dbPassword, dbName,
				selectTestingConnection);

		// 2018-04-05: without an open stream the first connection fails
		// 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
		//      Stdout of the spdlog is now false and I commented the ofstream statement
		// ofstream aaa("/tmp/a.txt");
		_logger->info(__FILEREF__ + "Creating MasterDBConnectionPool...");
		_masterPostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(
			masterDbPoolSize, _postgresMasterConnectionFactory);

		_logger->info(__FILEREF__ + "Creating SlaveDBConnectionPool...");
		_slavePostgresConnectionPool = make_shared<DBConnectionPool<PostgresConnection>>(
			slaveDbPoolSize, _postgresSlaveConnectionFactory);
	}

    _lastConnectionStatsReport = chrono::system_clock::now();

    _logger->info(__FILEREF__ + "createTablesIfNeeded...");
    createTablesIfNeeded();
    // _logger->info(__FILEREF__ + "createTablesIfNeeded_Postgres...");
    // createTablesIfNeeded_Postgres();
}

MMSEngineDBFacade::~MMSEngineDBFacade() 
{
}

void MMSEngineDBFacade::resetProcessingJobsIfNeeded(string processorMMS)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded"
		+ ", processorMMS: " + processorMMS
	);

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

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
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. Locks"
					+ ", processorMMS: " + processorMMS
					);
            lastSQLCommand = 
				"update MMS_Lock set active = ? where owner = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, 0);
            preparedStatement->setString(queryParameterIndex++, processorMMS);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. Locks)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", active: 0"
				+ ", processorMMS: " + processorMMS
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

		// 2022-09-27: next procedure should be already covered by retentionOfIngestionData,
		//		anyway, we will leave it here
        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. Downloading of IngestionJobs not completed"
					+ ", processorMMS: " + processorMMS
					);
			// 2021-07-17: Scenario:
			//	1. added the new MMS_0003 partition
			//	2. restarted the engine during the upload of a content
			//	Found the following combination:
			//		processorMMS is NULL, status is SourceDownloadingInProgress, sourceBinaryTransferred is 0
			//	Since we cannot have the above combination (processorMMS is NULL, status is SourceDownloadingInProgress)
			//	then next update was changed to consider also processorMMS as null
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ? where (processorMMS is NULL or processorMMS = ?) and "
                "status in (?, ?, ?) and sourceBinaryTransferred = 0";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. Downloading of IngestionJobs not completed)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", IngestionStatus::Start_TaskQueued: " + MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued)
				+ ", processorMMS: " + processorMMS
				+ ", IngestionStatus::SourceDownloadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress)
				+ ", IngestionStatus::SourceMovingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress)
				+ ", IngestionStatus::SourceCopingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. IngestionJobs assigned without final state"
					+ ", processorMMS: " + processorMMS
					);
            lastSQLCommand = 
                "update MMS_IngestionJob set processorMMS = NULL where processorMMS = ? and status not like 'End_%'";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. IngestionJobs assigned without final state)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMS: " + processorMMS
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
                        
        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. EncodingJobs assigned with state Processing"
					+ ", processorMMS: " + processorMMS
					);
			// transcoder does not have to be reset because the Engine, in case of restart,
			// is still able to attach to it (encoder)
            // lastSQLCommand = 
            //   "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ?";
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null where processorMMS = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();            
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. EncodingJobs assigned with state Processing)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", processorMMS: " + processorMMS
				+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. MediaItems retention assigned"
					+ ", processorMMS: " + processorMMS
					);
            lastSQLCommand = 
                "update MMS_MediaItem set processorMMSForRetention = NULL where processorMMSForRetention = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded. MediaItems retention assigned)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMS: " + processorMMS
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

string MMSEngineDBFacade::nextRelativePathToBeUsed (
    int64_t workspaceKey
)
{
    string      relativePathToBeUsed;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select currentDirLevel1, currentDirLevel2, currentDirLevel3 from MMS_WorkspaceMoreInfo where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                currentDirLevel1 = resultSet->getInt("currentDirLevel1");
                currentDirLevel2 = resultSet->getInt("currentDirLevel2");
                currentDirLevel3 = resultSet->getInt("currentDirLevel3");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        {
            char pCurrentRelativePath [64];
            
            sprintf (pCurrentRelativePath, "/%03d/%03d/%03d/", 
                currentDirLevel1, currentDirLevel2, currentDirLevel3);
            
            relativePathToBeUsed = pCurrentRelativePath;
        }
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    
    return relativePathToBeUsed;
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
	string, string, string, string, uint64_t, bool>
	MMSEngineDBFacade::getStorageDetails(
		int64_t physicalPathKey,
		bool fromMaster
)
{
        
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

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
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.contentType, mi.title, mi.deliveryFileName, pp.externalReadOnlyStorage, "
				"pp.encodingProfileKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and pp.physicalPathKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
				contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                title = resultSet->getString("title");
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
                externalReadOnlyStorage = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
                if (resultSet->isNull("encodingProfileKey"))
                    encodingProfileKey = -1;
				else
                    encodingProfileKey = resultSet->getInt64("encodingProfileKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getUInt64("sizeInBytes");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey is not present"
                    + ", physicalPathKey: " + to_string(physicalPathKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
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
            lastSQLCommand = string("") +
                "select deliveryTechnology from MMS_EncodingProfile "
                "where encodingProfileKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                deliveryTechnology = toDeliveryTechnology(resultSet->getString("deliveryTechnology"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey is not present"
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

		_logger->info(__FILEREF__ + "getStorageDetails"
			+ ", deliveryTechnology: " + toString(deliveryTechnology)
			+ ", physicalPathKey: " + to_string(physicalPathKey)
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", mmsPartitionNumber: " + to_string(mmsPartitionNumber)
			+ ", relativePath: " + relativePath
			+ ", fileName: " + fileName
			+ ", deliveryFileName: " + deliveryFileName
			+ ", title: " + title
			+ ", sizeInBytes: " + to_string(sizeInBytes)
			+ ", externalReadOnlyStorage: " + to_string(externalReadOnlyStorage)
		);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        return make_tuple(physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath,
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage);
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
	string, string, string, string, uint64_t, bool>
	MMSEngineDBFacade::getStorageDetails(
		int64_t mediaItemKey,
		// encodingProfileKey == -1 means it is requested the source file (the one having the bigger size in case there are more than one)
		int64_t encodingProfileKey,
		bool warningIfMissing,
		bool fromMaster
)
{

    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

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
            lastSQLCommand =
                "select mi.workspaceKey, mi.title, mi.contentType, mi.deliveryFileName, "
				"pp.externalReadOnlyStorage, pp.physicalPathKey, pp.partitionNumber, "
				"pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? "
                "and pp.encodingProfileKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", encodingProfileKey: " + to_string(encodingProfileKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
				contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
                externalReadOnlyStorage = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
                physicalPathKey = resultSet->getInt64("physicalPathKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getUInt64("sizeInBytes");
            }

			if (physicalPathKey == -1)
            {
                string errorMessage = __FILEREF__
					+ "MediaItemKey/EncodingProfileKey are not present"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
				if (warningIfMissing)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);                    
			}

			// default
			if (contentType == ContentType::Video || contentType == ContentType::Audio)
				deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
			else
				deliveryTechnology = DeliveryTechnology::Download;

			{
				lastSQLCommand = string("") +
					"select deliveryTechnology from MMS_EncodingProfile "
					"where encodingProfileKey = ? ";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSet->next())
				{
					deliveryTechnology = toDeliveryTechnology(resultSet->getString("deliveryTechnology"));
				}
				else
				{
					string errorMessage = __FILEREF__ + "encodingProfileKey is not present"
						+ ", encodingProfileKey: " + to_string(encodingProfileKey)
						+ ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}
        }
		else
		{
			tuple<int64_t, int, string, string, uint64_t, bool, int64_t> sourcePhysicalPathDetails =
				getSourcePhysicalPath(mediaItemKey, warningIfMissing, fromMaster);
			tie(physicalPathKey, mmsPartitionNumber, relativePath,
					fileName, sizeInBytes, externalReadOnlyStorage, ignore) = sourcePhysicalPathDetails;

            lastSQLCommand = string("") +
                "select workspaceKey, contentType, title, deliveryFileName "
                "from MMS_MediaItem where mediaItemKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
				contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");

				// default
				MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
				if (contentType == ContentType::Video || contentType == ContentType::Audio)
					deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
				else
					deliveryTechnology = DeliveryTechnology::Download;
            }

			if (workspaceKey == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/EncodingProfileKey are not present"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
				if (warningIfMissing)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        return make_tuple(physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath,
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage);
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(MediaItemKeyNotFound& e)
    {        
		if (warningIfMissing)
			_logger->warn(__FILEREF__ + "MediaItemKeyNotFound"
			 + ", e.what(): " + e.what()
			 + ", lastSQLCommand: " + lastSQLCommand
			 + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
			);
		else
			_logger->error(__FILEREF__ + "MediaItemKeyNotFound"
			 + ", e.what(): " + e.what()
			 + ", lastSQLCommand: " + lastSQLCommand
			 + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
			);

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(runtime_error& e)
    {
        _logger->error(__FILEREF__ + "runtime_error"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

void MMSEngineDBFacade::getAllStorageDetails(
        int64_t mediaItemKey,
		bool fromMaster,
		vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>>&
		allStorageDetails
)
{

    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool;
	if (fromMaster)
		connectionPool = _masterConnectionPool;
	else
		connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t workspaceKey;
		int64_t encodingProfileKey;
        int mmsPartitionNumber;
        int64_t sizeInBytes;
        int externalReadOnlyStorage;
        string relativePath;
        string fileName;
		ContentType contentType;
        {
			lastSQLCommand =
				"select mi.workspaceKey, mi.contentType, pp.externalReadOnlyStorage, pp.encodingProfileKey, "
				"pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
				"from MMS_MediaItem mi, MMS_PhysicalPath pp "
				"where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
				contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
                externalReadOnlyStorage = resultSet->getInt("externalReadOnlyStorage") == 0 ? false : true;
                if (resultSet->isNull("encodingProfileKey"))
                    encodingProfileKey = -1;
				else
                    encodingProfileKey = resultSet->getInt64("encodingProfileKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getInt64("sizeInBytes");

                shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);
        
				// default
				MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;
				if (contentType == ContentType::Video || contentType == ContentType::Audio)
					deliveryTechnology = DeliveryTechnology::DownloadAndStreaming;
				else
					deliveryTechnology = DeliveryTechnology::Download;

				if (encodingProfileKey != -1)
				{
					lastSQLCommand = string("") +
						"select deliveryTechnology from MMS_EncodingProfile "
						"where encodingProfileKey = ? ";

					shared_ptr<sql::PreparedStatement> technologyPreparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					technologyPreparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> technologyResultSet (technologyPreparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", encodingProfileKey: " + to_string(encodingProfileKey)
						+ ", technologyResultSet->rowsCount: " + to_string(technologyResultSet->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (technologyResultSet->next())
					{
						deliveryTechnology = toDeliveryTechnology(technologyResultSet->getString("deliveryTechnology"));
					}
					else
					{
						string errorMessage = __FILEREF__ + "encodingProfileKey is not present"
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", lastSQLCommand: " + lastSQLCommand
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                    
					}
				}

                tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool> storageDetails =
                    make_tuple(deliveryTechnology, mmsPartitionNumber, workspace->_directoryName,
							relativePath, fileName, sizeInBytes, externalReadOnlyStorage);
                
                allStorageDetails.push_back(storageDetails);
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

// at least one between physicalPathKey and liveDeliveryKey has to be -1
int64_t MMSEngineDBFacade::createDeliveryAuthorization(
    int64_t userKey,
    string clientIPAddress,
    int64_t physicalPathKey,	// vod key
	int64_t liveDeliveryKey,	// live key
    string deliveryURI,
    int ttlInSeconds,
    int maxRetries)
{
    int64_t     deliveryAuthorizationKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
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
            lastSQLCommand = 
                "insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, "
				"contentType, contentKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
                "                                      NULL,                     ?,       ?, "
				"?,           ?,          ?,           ?,            0,                    ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            if (clientIPAddress == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, clientIPAddress);
			preparedStatement->setString(queryParameterIndex++, contentType);
			preparedStatement->setInt64(queryParameterIndex++, contentKey);
            preparedStatement->setString(queryParameterIndex++, deliveryURI);
            preparedStatement->setInt(queryParameterIndex++, ttlInSeconds);
            preparedStatement->setInt(queryParameterIndex++, maxRetries);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", userKey: " + to_string(userKey)
				+ ", clientIPAddress: " + clientIPAddress
				+ ", contentType: " + contentType
				+ ", contentKey: " + to_string(contentKey)
				+ ", deliveryURI: " + deliveryURI
				+ ", ttlInSeconds: " + to_string(ttlInSeconds)
				+ ", maxRetries: " + to_string(maxRetries)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        deliveryAuthorizationKey = getLastInsertId(conn);
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        

    return deliveryAuthorizationKey;
}

bool MMSEngineDBFacade::checkDeliveryAuthorization(
        int64_t deliveryAuthorizationKey,
        string contentURI)
{
    bool        authorizationOK = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
    
        {
            lastSQLCommand =
                "select deliveryURI, currentRetriesNumber, maxRetries, "
                "(DATE_ADD(authorizationTimestamp, INTERVAL ttlInSeconds SECOND) - NOW()) as timeToLiveAvailable "
                "from MMS_DeliveryAuthorization "
                "where deliveryAuthorizationKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, deliveryAuthorizationKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", deliveryAuthorizationKey: " + to_string(deliveryAuthorizationKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                string deliveryURI = resultSet->getString("deliveryURI");
                int currentRetriesNumber = resultSet->getInt("currentRetriesNumber");
                int maxRetries = resultSet->getInt("maxRetries");
                int timeToLiveAvailable = resultSet->getInt("timeToLiveAvailable");

                if (contentURI != deliveryURI)
                {
                    string errorMessage = __FILEREF__ + "contentURI and deliveryURI are different"
                        + ", contentURI: " + contentURI
                        + ", deliveryURI: " + deliveryURI
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
                else if (currentRetriesNumber >= maxRetries)
                {
                    string errorMessage = __FILEREF__ + "maxRetries is already reached"
                        + ", currentRetriesNumber: " + to_string(currentRetriesNumber)
                        + ", maxRetries: " + to_string(maxRetries)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
                else if (timeToLiveAvailable < 0)
                {
                    string errorMessage = __FILEREF__ + "TTL expired"
                        + ", timeToLiveAvailable: " + to_string(timeToLiveAvailable)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "deliveryAuthorizationKey not found"
                    + ", deliveryAuthorizationKey: " + to_string(deliveryAuthorizationKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            
            authorizationOK = true;
        }
        
        if (authorizationOK)
        {
            lastSQLCommand = 
                "update MMS_DeliveryAuthorization set currentRetriesNumber = currentRetriesNumber + 1 "
                "where deliveryAuthorizationKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, deliveryAuthorizationKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", deliveryAuthorizationKey: " + to_string(deliveryAuthorizationKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
                    
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        // throw e;
    }        
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        

    return authorizationOK;
}

void MMSEngineDBFacade::retentionOfDeliveryAuthorization()
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "retentionOfDeliveryAuthorization"
			);

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

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
					lastSQLCommand = 
						"delete from MMS_DeliveryAuthorization "
						"where DATE_ADD(authorizationTimestamp, "
							"INTERVAL (ttlInSeconds + ?) SECOND) < NOW() "
						"limit ?";

					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setInt(queryParameterIndex++, retention);
					preparedStatement->setInt(queryParameterIndex++, maxToBeRemoved);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", retention: " + to_string(retention)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					totalRowsRemoved += rowsUpdated;
					if (rowsUpdated == 0)
						moreRowsToBeRemoved = false;

					currentRetriesOnError = 0;
				}
				catch(sql::SQLException& se)
				{
					currentRetriesOnError++;
					if (currentRetriesOnError >= maxRetriesOnError)
						throw se;

					// Deadlock!!!
					_logger->error(__FILEREF__ + "SQL exception"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", se.what(): " + se.what()
						+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
					);

					int secondsBetweenRetries = 15;
					_logger->info(__FILEREF__ + "retentionOfDeliveryAuthorization failed, "
						+ "waiting before to try again"
						+ ", currentRetriesOnError: " + to_string(currentRetriesOnError)
						+ ", maxRetriesOnError: " + to_string(maxRetriesOnError)
						+ ", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}

			_logger->info(__FILEREF__ + "Deletion obsolete DeliveryAuthorization"
				+ ", totalRowsRemoved: " + to_string(totalRowsRemoved)
			);
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

int64_t MMSEngineDBFacade::getLastInsertId(shared_ptr<MySQLConnection> conn)
{
    int64_t         lastInsertId;
    
    string      lastSQLCommand;

    try
    {
        lastSQLCommand = 
            "select LAST_INSERT_ID()";
        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
		_logger->info(__FILEREF__ + "@SQL statistics@"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
			+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startSql).count()) + "@"
		);

        if (resultSet->next())
        {
            lastInsertId = resultSet->getInt64(1);
        }
        else
        {
            string error ("select LAST_INSERT_ID failed");
            
            _logger->error(error);
            
            throw runtime_error(error);
        }
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return lastInsertId;
}

bool MMSEngineDBFacade::oncePerDayExecution(OncePerDayType oncePerDayType)
{
    string		lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "oncePerDayExecution"
	);

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

	bool		alreadyExecuted;
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
			tm			tmDateTime;
			char		strDateTime [64];
			time_t utcTime = chrono::system_clock::to_time_t(
				chrono::system_clock::now());


			localtime_r (&utcTime, &tmDateTime);

			sprintf (strDateTime, "%04d-%02d-%02d",
				tmDateTime.tm_year + 1900,
				tmDateTime.tm_mon + 1,
				tmDateTime.tm_mday
			);
			today_yyyy_mm_dd = strDateTime;
		}

		conn = connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

        {
            lastSQLCommand = 
				"update MMS_OncePerDayExecution set lastExecutionTime = ? "
				"where type = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, today_yyyy_mm_dd);
            preparedStatement->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(oncePerDayType));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (oncePerDayExecution)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", today_yyyy_mm_dd: " + today_yyyy_mm_dd
				+ ", type: " + MMSEngineDBFacade::toString(oncePerDayType)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			alreadyExecuted = (rowsUpdated == 0 ? true : false);
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;


		return alreadyExecuted;
    }
    catch(sql::SQLException& se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception& e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
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
	transform(fileFormat.begin(), fileFormat.end(),                                               
		fileFormatLowerCase.begin(), [](unsigned char c){return tolower(c); } );

	if (fileFormatLowerCase == "mp4"
		|| fileFormatLowerCase == "mkv"
		|| fileFormatLowerCase == "mov"
		|| fileFormatLowerCase == "webm"
	)
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::DownloadAndStreaming;
	else if (fileFormatLowerCase == "hls"
		|| fileFormatLowerCase == "m3u8"
		|| fileFormatLowerCase == "dash"
	)
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming;
	else if (fileFormatLowerCase == "ts"
		|| fileFormatLowerCase == "mts"
		|| fileFormatLowerCase == "avi"
	)
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::Download;
	else
		deliveryTechnology = MMSEngineDBFacade::DeliveryTechnology::Download;

	return deliveryTechnology;
}

