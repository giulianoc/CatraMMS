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

#define ENABLE_DBLOGGER 0
#if ENABLE_DBLOGGER == 1
    #include "spdlog/spdlog.h"
    static shared_ptr<spdlog::logger> _globalLogger = nullptr;
    #define DB_BORROW_DEBUG_LOGGER(x) if (_globalLogger != nullptr) _globalLogger->info(x);
    #define DB_BORROW_ERROR_LOGGER(x) if (_globalLogger != nullptr) _globalLogger->info(x);
    // #include <iostream>
    // #define DB_DEBUG_LOGGER(x) std::cout << x << std::endl;
    // #define DB_ERROR_LOGGER(x) std::cerr << x << std::endl;
#endif

#include "PersistenceLock.h"
#include "MMSEngineDBFacade.h"
#include <fstream>
#include <sstream>


// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
        Json::Value configuration,
		size_t dbPoolSize,
        shared_ptr<spdlog::logger> logger) 
{
    _logger     = logger;
    #if ENABLE_DBLOGGER == 1
        _globalLogger = logger;
    #endif

    _defaultContentProviderName     = "default";
    // _defaultTerritoryName           = "default";

	/*
    size_t dbPoolSize = configuration["database"].get("poolSize", 5).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->poolSize: " + to_string(dbPoolSize)
    );
	*/
    string dbServer = configuration["database"].get("server", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->server: " + dbServer
    );
    _dbConnectionPoolStatsReportPeriodInSeconds = JSONUtils::asInt(configuration["database"], "dbConnectionPoolStatsReportPeriodInSeconds", 5);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->dbConnectionPoolStatsReportPeriodInSeconds: " + to_string(_dbConnectionPoolStatsReportPeriodInSeconds)
    );

    string dbUsername = configuration["database"].get("userName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->userName: " + dbUsername
    );
    string dbPassword;
    {
        string encryptedPassword = configuration["database"].get("password", "XXX").asString();
        dbPassword = Encrypt::decrypt(encryptedPassword);        
    }    
    string dbName = configuration["database"].get("dbName", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->dbName: " + dbName
    );
    string selectTestingConnection = configuration["database"].get("selectTestingConnection", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->selectTestingConnection: " + selectTestingConnection
    );
    _ingestionWorkflowRetentionInDays = JSONUtils::asInt(configuration["database"], "ingestionWorkflowRetentionInDays", 30);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->ingestionWorkflowRetentionInDays: " + to_string(_ingestionWorkflowRetentionInDays)
    );
    _doNotManageIngestionsOlderThanDays = JSONUtils::asInt(configuration["mms"], "doNotManageIngestionsOlderThanDays", 7);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->doNotManageIngestionsOlderThanDays: " + to_string(_doNotManageIngestionsOlderThanDays)
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
    _contentNotTransferredRetentionInDays    = JSONUtils::asInt(configuration["mms"], "contentNotTransferredRetentionInDays", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentNotTransferredRetentionInDays: " + to_string(_contentNotTransferredRetentionInDays)
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

    _predefinedVideoProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("videoDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->videoDir: " + _predefinedVideoProfilesDirectoryPath
    );
    _predefinedAudioProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("audioDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->audioDir: " + _predefinedAudioProfilesDirectoryPath
    );
    _predefinedImageProfilesDirectoryPath = configuration["encoding"]["predefinedProfiles"].get("imageDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->predefinedProfiles->imageDir: " + _predefinedImageProfilesDirectoryPath
    );

    _predefinedWorkflowLibraryDirectoryPath = configuration["mms"].get("predefinedWorkflowLibraryDir", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->predefinedWorkflowLibraryDir: " + _predefinedWorkflowLibraryDirectoryPath
    );

    _logger->info(__FILEREF__ + "Creating MySQLConnectionFactory...");
	bool reconnect = true;
	string defaultCharacterSet = "utf8";
    _mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName,
            reconnect, defaultCharacterSet, selectTestingConnection);

    // 2018-04-05: without an open stream the first connection fails
    // 2018-05-22: It seems the problem is when the stdout of the spdlog is true!!!
    //      Stdout of the spdlog is now false and I commented the ofstream statement
    // ofstream aaa("/tmp/a.txt");
    _logger->info(__FILEREF__ + "Creating DBConnectionPool...");
    _connectionPool = make_shared<DBConnectionPool<MySQLConnection>>(
            dbPoolSize, _mySQLConnectionFactory);
     
    _lastConnectionStatsReport = chrono::system_clock::now();

    _logger->info(__FILEREF__ + "createTablesIfNeeded...");
    createTablesIfNeeded();
}

MMSEngineDBFacade::~MMSEngineDBFacade() 
{
}

/*
void MMSEngineDBFacade::getTerritories(shared_ptr<Workspace> workspace)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	
    _logger->debug(__FILEREF__ + "DB connection borrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );

    string lastSQLCommand =
        "select territoryKey, name from MMS_Territory t where workspaceKey = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        lastSQLCommand));
    preparedStatement->setInt(1, workspace->_workspaceKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    while (resultSet->next())
    {
        workspace->_territories.insert(make_pair(resultSet->getInt("territoryKey"), resultSet->getString("name")));
    }

    _logger->debug(__FILEREF__ + "DB connection unborrow"
        + ", getConnectionId: " + to_string(conn->getConnectionId())
    );
    _connectionPool->unborrow(conn);
	conn = nullptr;
}
*/

/*
int64_t MMSEngineDBFacade::addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t workspaceKey,
        string territoryName
)
{
    int64_t         territoryKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into MMS_Territory (territoryKey, workspaceKey, name, currency) values ("
                "NULL, ?, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, territoryName);
            string currency("");
            if (currency == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, currency);

            preparedStatement->executeUpdate();
        }
        
        territoryKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return territoryKey;
}
*/

void MMSEngineDBFacade::resetProcessingJobsIfNeeded(string processorMMS)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded"
			+ ", processorMMS: " + processorMMS
			);

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
        conn = _connectionPool->borrow();	
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

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_Lock) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. Downloading of IngestionJobs not completed"
					+ ", processorMMS: " + processorMMS
					);
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ? where processorMMS = ? and "
                "status in (?, ?, ?) and sourceBinaryTransferred = 0";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_IngestionJob) to be reset because downloading was interrupted"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

        {
			_logger->info(__FILEREF__ + "resetProcessingJobsIfNeeded. IngestionJobs assigned without final state"
					+ ", processorMMS: " + processorMMS
					);
            lastSQLCommand = 
                "update MMS_IngestionJob set processorMMS = NULL where processorMMS = ? and status not like 'End_%'";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_IngestionJob) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
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

            int rowsUpdated = preparedStatement->executeUpdate();            
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_EncodingJob) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
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

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Found Processing jobs (MMS_MediaItem) to be reset"
                    + ", processorMMS: " + processorMMS
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

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

            int rowsExpired = preparedStatement->executeUpdate();            
            if (rowsExpired > 0)
                _logger->warn(__FILEREF__ + "Rows (MMS_EncodingJob) that were expired"
                    + ", rowsExpired: " + to_string(rowsExpired)
                );
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

void MMSEngineDBFacade::manageMainAndBackupOfRunnungLiveRecordingHA(string processorMMS)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
		int milliSecondsToSleepWaitingLock = 500;

		PersistenceLock persistenceLock(this,
			MMSEngineDBFacade::LockType::MainAndBackupLiveRecordingHA,
			_maxSecondsToWaitMainAndBackupLiveChunkLock,
			processorMMS, "MainAndBackupLiveRecording",
			milliSecondsToSleepWaitingLock, _logger);

		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "Live Recording HA just started");

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
			// the setting of this variable is done also in EncoderVideoAudioProxy::processLiveRecorder
			// method. So in case this is changed, also in EncoderVideoAudioProxy::processLiveRecorder
			// has to be changed too

			// if the ingestionJob is 'just' finished, anyway we have to get the ingestionJob
			// in order to remove the last backup file 
			int toleranceMinutes = 5;
            lastSQLCommand =
				string("select ingestionJobKey from MMS_IngestionJob "
					"where ingestionType = 'Live-Recorder' "
					"and JSON_EXTRACT(metaDataContent, '$.HighAvailability') = true "
					"and (status = 'EncodingQueued' "
					"or (status = 'End_TaskSuccess' and "
						"NOW() <= DATE_ADD(endProcessing, INTERVAL ? MINUTE))) "
				);
			// This select returns all the ingestion job key of running HA LiveRecording

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, toleranceMinutes);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

				// get all the couples, main and backup, and set one validated
				//	and the other not validated
				{
					_logger->info(__FILEREF__ + "Manage HA LiveRecording, main and backup (couple)"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						);

					lastSQLCommand =
						string("select JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') "
							"as utcChunkStartTime from MMS_MediaItem where "
							"JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
							"and JSON_EXTRACT(userData, '$.mmsData.validated') is null "
							"and retentionInMinutes != 0 "
							"group by JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') "
								"having count(*) = 2"
								// "having count(*) = 2 for update"
						);
					// This select returns all the chunks (media item utcChunkStartTime)
					// that are present two times (because of HA live recording)

					shared_ptr<sql::PreparedStatement> preparedStatementChunkStartTime (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementChunkStartTime->setInt64(
							queryParameterIndex++, ingestionJobKey);
					shared_ptr<sql::ResultSet> resultSetChunkStartTime (
							preparedStatementChunkStartTime->executeQuery());
					while (resultSetChunkStartTime->next())
					{
						int64_t utcChunkStartTime =
							resultSetChunkStartTime->getInt64("utcChunkStartTime");

						_logger->info(__FILEREF__
								+ "Manage HA LiveRecording Chunk, main and backup (couple)"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

						int64_t workspaceKey;
						ContentType contentType;

						int64_t mediaItemKeyChunk_1;
						bool mainChunk_1;
						int64_t durationInMilliSecondsChunk_1;
						string uniqueName_1;

						int64_t mediaItemKeyChunk_2;
						bool mainChunk_2;
						int64_t durationInMilliSecondsChunk_2;
						string uniqueName_2;

						lastSQLCommand =
							string("select workspaceKey, mediaItemKey, contentType, "
								"CAST(JSON_EXTRACT(userData, '$.mmsData.main') as SIGNED INTEGER) as main, "
								"JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.uniqueName')) as uniqueName "
								"from MMS_MediaItem "
								"where JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
								"and JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') = ? "
							);
						shared_ptr<sql::PreparedStatement> preparedStatementMediaItemDetails (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatementMediaItemDetails->setInt64(
								queryParameterIndex++, ingestionJobKey);
						preparedStatementMediaItemDetails->setInt64(
								queryParameterIndex++, utcChunkStartTime);
						shared_ptr<sql::ResultSet> resultSetMediaItemDetails (
								preparedStatementMediaItemDetails->executeQuery());
						if (resultSetMediaItemDetails->next())
						{
							workspaceKey =
								resultSetMediaItemDetails->getInt64("workspaceKey");
							mediaItemKeyChunk_1 =
								resultSetMediaItemDetails->getInt64("mediaItemKey");
							contentType = MMSEngineDBFacade::toContentType(resultSetMediaItemDetails->getString("contentType"));
							mainChunk_1 =
								resultSetMediaItemDetails->getInt("main") == 1 ? true : false;
							if (!resultSetMediaItemDetails->isNull("uniqueName"))
								uniqueName_1 = resultSetMediaItemDetails->getString("uniqueName");

							try
							{
								int64_t physicalPathKey = -1;
								durationInMilliSecondsChunk_1 = getMediaDurationInMilliseconds(mediaItemKeyChunk_1,
									physicalPathKey);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "getMediaDurationInMilliseconds failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
									+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
									+ ", exception: " + e.what()
								);

								continue;
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "getMediaDurationInMilliseconds failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
									+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
								);

								continue;
							}
						}
						else
						{
							_logger->error(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						if (resultSetMediaItemDetails->next())
						{
							mediaItemKeyChunk_2 =
								resultSetMediaItemDetails->getInt64("mediaItemKey");
							mainChunk_2 =
								resultSetMediaItemDetails->getInt("main") == 1 ? true : false;
							if (!resultSetMediaItemDetails->isNull("uniqueName"))
								uniqueName_2 = resultSetMediaItemDetails->getString("uniqueName");

							try
							{
								int64_t physicalPathKey = -1;
								durationInMilliSecondsChunk_2 = getMediaDurationInMilliseconds(mediaItemKeyChunk_2,
									physicalPathKey);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "getMediaDurationInMilliseconds failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
									+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
									+ ", exception: " + e.what()
								);

								continue;
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "getMediaDurationInMilliseconds failed"
									+ ", ingestionJobKey: " + to_string(ingestionJobKey)
									+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
									+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
								);

								continue;
							}
						}
						else
						{
							_logger->error(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						if (resultSetMediaItemDetails->next())
						{
							// _logger->error(__FILEREF__ + "It should never happen"
							_logger->warn(__FILEREF__ + "It should never happen"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							);

							continue;
						}

						_logger->info(__FILEREF__
							+ "Manage HA LiveRecording Chunks, main and backup (couple)"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
							+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
							+ ", mainChunk_1: " + to_string(mainChunk_1)
							+ ", durationInMilliSecondsChunk_1: "
							+ to_string(durationInMilliSecondsChunk_1)
							+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
							+ ", mainChunk_2: " + to_string(mainChunk_2)
							+ ", durationInMilliSecondsChunk_2: "
								+ to_string(durationInMilliSecondsChunk_2)
							);

						{
							int64_t mediaItemKeyNotValidated;
							int64_t mediaItemKeyValidated;
							string uniqueNameValidated;
							if (durationInMilliSecondsChunk_1 == durationInMilliSecondsChunk_2)
							{
								if (mainChunk_1)
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
									uniqueNameValidated = uniqueName_1;
								}
								else
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
									uniqueNameValidated = uniqueName_2;
								}
							}
							else if (durationInMilliSecondsChunk_1 < durationInMilliSecondsChunk_2)
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
									uniqueNameValidated = uniqueName_2;
							}
							else
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
									uniqueNameValidated = uniqueName_1;
							}

							_logger->info(__FILEREF__
								+ "Manage HA LiveRecording, reset of chunk, main and backup (couple)"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", utcChunkStartTime: " + to_string(utcChunkStartTime)
								+ ", mediaItemKeyChunk_1: " + to_string(mediaItemKeyChunk_1)
								+ ", uniqueName_1: " + uniqueName_1
								+ ", mainChunk_1: " + to_string(mainChunk_1)
								+ ", durationInMilliSecondsChunk_1: "
									+ to_string(durationInMilliSecondsChunk_1)
								+ ", mediaItemKeyChunk_2: " + to_string(mediaItemKeyChunk_2)
								+ ", uniqueName_2: " + uniqueName_2
								+ ", mainChunk_2: " + to_string(mainChunk_2)
								+ ", durationInMilliSecondsChunk_2: "
									+ to_string(durationInMilliSecondsChunk_2)
								+ ", mediaItemKeyNotValidated: "
									+ to_string(mediaItemKeyNotValidated)
								+ ", mediaItemKeyValidated: " + to_string(mediaItemKeyValidated)
							);

							// mediaItemKeyValidated
							{
								lastSQLCommand =
									"update MMS_MediaItem set userData = JSON_SET(userData, '$.mmsData.validated', true) "
									"where mediaItemKey = ?";
								shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
								int queryParameterIndex = 1;
								preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyValidated);

								int rowsUpdated = preparedStatementUpdate->executeUpdate();
								if (rowsUpdated != 1)
									_logger->error(__FILEREF__ + "It should never happen"
										+ ", mediaItemKeyToBeValidated: " + to_string(mediaItemKeyValidated)
										+ ", rowsUpdated: " + to_string(rowsUpdated)
									);

								if (uniqueNameValidated != "")
								{
									try
									{
										// it should not happen duplicated unique name
										bool allowUniqueNameOverride = true;
										addExternalUniqueName(conn, workspaceKey, mediaItemKeyValidated,
											allowUniqueNameOverride, uniqueNameValidated);
									}
									catch(sql::SQLException se)
									{
										string exceptionMessage(se.what());
        
										_logger->error(__FILEREF__ + "SQL exception"
										// 	+ ", lastSQLCommand: " + lastSQLCommand
											+ ", exceptionMessage: " + exceptionMessage
											+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
										);

										// continue;
									}
									catch(AlreadyLocked e)
									{
										_logger->error(__FILEREF__ + "SQL exception"
											+ ", e.what(): " + e.what()
										// 	+ ", lastSQLCommand: " + lastSQLCommand
											+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
										);

										// continue;
									}
									catch(runtime_error e)
									{
										_logger->error(__FILEREF__ + "SQL exception"
											+ ", e.what(): " + e.what()
										// 	+ ", lastSQLCommand: " + lastSQLCommand
											+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
										);

										// continue;
									}
									catch(exception e)
									{
										_logger->error(__FILEREF__ + "SQL exception"
										// 	+ ", lastSQLCommand: " + lastSQLCommand
											+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
										);

										// continue;
									}
								}
							}

							// mediaItemKeyNotValidated
							{
								lastSQLCommand = 
									"update MMS_MediaItem set retentionInMinutes = 0, "
									"userData = JSON_SET(userData, '$.mmsData.validated', false) "
									"where mediaItemKey = ?";
								shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
								int queryParameterIndex = 1;
								preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyNotValidated);

								int rowsUpdated = preparedStatementUpdate->executeUpdate();            
								if (rowsUpdated != 1)
									_logger->error(__FILEREF__ + "It should never happen"
										+ ", mediaItemKeyNotValidated: " + to_string(mediaItemKeyNotValidated)
									);
							}
						}
					}
				}

				// get all the singles, main or backup, that, after a while,
				// was not marked as validated ot not. Mark them as validated.
				// This is the scenario where just one chunk is generated, main or backup,
				// So it has to be marked as validated
				{
					// 2020-04-30: before it was 60 and we saw it was too early, scenario:
					//	1. chunks main and backup were ingested
					//	2. this method delay to be called and 60 seconds were passed
					//	3. next select get this chunk and set it to validated
					//	4. once the previous select (the one looking for the couple of chunks) get it
					//		it is too late and we got an error because the update to validated fails
					//		since it is already validated
					// For this reason we increased chunksToBeManagedWithinSeconds to 180 (3 * 60)
					// I added also the following contition to the previous select
					//		(the one looking for the couple of chunks)
					//		"and JSON_EXTRACT(userData, '$.mmsData.validated') is null "
					// Previous condition is important because in the scenario of high availability,
					// $.mmsData.validated is null.

					// 2020-06-06: Executing the following select 
					//	select title, ingestionDate, JSON_EXTRACT (userData, '$.mmsData.validated') from MMS_MediaItem where workspaceKey = 7 and title like '44 -%' order by title;
					//	I saw that the difference between the ingestionDate of the main and the backup chunks arrived up to 14 minutes
					//	So this scenario is the following:
					//	- one chunk is ingested (assuming it is the main)
					//	- 3 minutes passed
					//	- next select set it as validated
					//	- after 5 minutes the other chunk arrives
					//	- it is still single and next select set it as validated
					//	The wrong result is that we have two chunks that are the same and both validated
					//	To avoid that:
					//	- we still maintain 3 minutes max to validate the chunk BUT
					//	- before to validate it, we check if the same reduntant chunk was already validated.
					//		If yes we do not validated the new one

					int chunksToBeManagedWithinSeconds = 3 * 60;

					_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single)"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						);

					lastSQLCommand =
						string("select workspaceKey, mediaItemKey, "
							"JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.uniqueName')) as uniqueName, "
							"JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') as utcChunkStartTime, "
							"CAST(JSON_EXTRACT(userData, '$.mmsData.main') as SIGNED INTEGER) as main "
							"from MMS_MediaItem where "
							"JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' "
							"and JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
							"and JSON_EXTRACT(userData, '$.mmsData.validated') is null "
							"and NOW() > DATE_ADD(ingestionDate, INTERVAL ? SECOND) "
							"and retentionInMinutes != 0"
							// "and retentionInMinutes != 0 for update"
						);

					shared_ptr<sql::PreparedStatement> preparedStatementMediaItemKey (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementMediaItemKey->setInt64(queryParameterIndex++, ingestionJobKey);
					preparedStatementMediaItemKey->setInt(queryParameterIndex++, chunksToBeManagedWithinSeconds);
					shared_ptr<sql::ResultSet> resultSetMediaItemKey (preparedStatementMediaItemKey->executeQuery());
					while (resultSetMediaItemKey->next())
					{
						int64_t workspaceKey = resultSetMediaItemKey->getInt64("workspaceKey");
						int64_t mediaItemKeyChunk = resultSetMediaItemKey->getInt64("mediaItemKey");
						string uniqueName;
						if (!resultSetMediaItemKey->isNull("uniqueName"))
							uniqueName = resultSetMediaItemKey->getString("uniqueName");
						int64_t utcChunkStartTime = resultSetMediaItemKey->getInt64("utcChunkStartTime");
						bool main = resultSetMediaItemKey->getInt("main") == 1 ? true : false;

						// as specified in the above 2020-06-06 comment, before to validate it,
						// we will check if the other chunk (the duplicated one) was already validated
						bool validatedAlreadyPresent = false;
						{
							lastSQLCommand =
								string("select count(*) from MMS_MediaItem where ")
									+ "JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' "
									+ "and JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
									+ "and JSON_EXTRACT(userData, '$.mmsData.validated') = true "
									+ "and JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') = ? "
									+ "and JSON_EXTRACT(userData, '$.mmsData.main') = " + (main ? "false" : "true")
								;

							shared_ptr<sql::PreparedStatement> preparedStatementCheckValidation (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementCheckValidation->setInt64(queryParameterIndex++, ingestionJobKey);
							preparedStatementCheckValidation->setInt64(queryParameterIndex++, utcChunkStartTime);
							shared_ptr<sql::ResultSet> resultSetCheckValidation (
								preparedStatementCheckValidation->executeQuery());
							if (resultSetCheckValidation->next())
							{
								validatedAlreadyPresent = resultSetCheckValidation->getInt(1) == 1;
							}
							else
							{
								string errorMessage ("select count(*) failed");
								_logger->error(errorMessage);

								// throw runtime_error(errorMessage);
							}
						}

						{
							_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single), set validation"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
								+ ", validatedAlreadyPresent: " + to_string(validatedAlreadyPresent)
							);

							lastSQLCommand = 
								string("update MMS_MediaItem ")
								+ "set userData = JSON_SET(userData, '$.mmsData.validated', "
									+ (validatedAlreadyPresent ? "false" : "true") + ") "
								+ "where mediaItemKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyChunk);

							int rowsUpdated = preparedStatementUpdate->executeUpdate();            
							if (rowsUpdated != 1)
								_logger->error(__FILEREF__ + "It should never happen"
									+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
								);

							if (!validatedAlreadyPresent && uniqueName != "")
							{
								try
								{
									// it should not happen duplicated unique name
									bool allowUniqueNameOverride = true;
									addExternalUniqueName(conn, workspaceKey, mediaItemKeyChunk,
										allowUniqueNameOverride, uniqueName);
								}
								catch(sql::SQLException se)
								{
									string exceptionMessage(se.what());
       
									_logger->error(__FILEREF__ + "SQL exception"
										// + ", lastSQLCommand: " + lastSQLCommand
										+ ", exceptionMessage: " + exceptionMessage
										+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
									);

									// continue;
								}
								catch(AlreadyLocked e)
								{
									_logger->error(__FILEREF__ + "SQL exception"
										+ ", e.what(): " + e.what()
										// + ", lastSQLCommand: " + lastSQLCommand
										+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
									);

									// continue;
								}
								catch(runtime_error e)
								{
									_logger->error(__FILEREF__ + "SQL exception"
										+ ", e.what(): " + e.what()
										// + ", lastSQLCommand: " + lastSQLCommand
										+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
									);

									// continue;
								}
								catch(exception e)
								{
									_logger->error(__FILEREF__ + "SQL exception"
									// 	+ ", lastSQLCommand: " + lastSQLCommand
										+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
									);

									// continue;
								}
							}
						}
					}
				}
            }
        }
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count();
		_logger->info(__FILEREF__ + "manageMainAndBackupOfRunnungLiveRecordingHA"
			+ ", elapsed in seconds: " + to_string(elapsedInSeconds)
		);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw se;
    }
    catch(AlreadyLocked e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            try
            {
                // conn->_sqlConnection->rollback(); OR execute ROLLBACK
                if (!autoCommit)
                {
                    shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
                    statement->execute("ROLLBACK");
                }

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(sql::SQLException se)
            {
                _logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
                    + ", exceptionMessage: " + se.what()
                );

                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
            }
            catch(exception e)
            {
                _logger->error(__FILEREF__ + "exception doing unborrow"
                    + ", exceptionMessage: " + e.what()
                );

				/*
                _logger->debug(__FILEREF__ + "DB connection unborrow"
                    + ", getConnectionId: " + to_string(conn->getConnectionId())
                );
                _connectionPool->unborrow(conn);
				conn = nullptr;
				*/
            }
        }

        throw e;
    }
}

bool MMSEngineDBFacade::liveRecorderMainAndBackupChunksManagementCompleted(
		int64_t ingestionJobKey)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		bool		mainAndBackupChunksManagementCompleted = false;

        {
            lastSQLCommand =
				string("select count(*) from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
					"where ijo.mediaItemKey = mi.mediaItemKey and ijo.ingestionJobKey = ? "
					"and JSON_EXTRACT(mi.userData, '$.mmsData.validated') = false "
				);
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
				mainAndBackupChunksManagementCompleted = (resultSet->getInt64(1) == 0) ? true : false;
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		return mainAndBackupChunksManagementCompleted;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
}

void MMSEngineDBFacade::getRunningLiveRecorderVirtualVODsDetails(
	vector<tuple<int64_t, int64_t, int, string, int, string, string, int64_t, string>>&
	runningLiveRecordersDetails
)
{
	string      lastSQLCommand;

	shared_ptr<MySQLConnection> conn = nullptr;

	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		runningLiveRecordersDetails.clear();

		{
			// End_TaskSuccess below: if the ingestionJob is 'just' finished, anyway we have to get the ingestionJob
			// in order to get the last chunks 
			int toleranceMinutes = 5;
			lastSQLCommand =
				string("select ir.workspaceKey, ij.ingestionJobKey, "
					"JSON_UNQUOTE(JSON_EXTRACT (ij.metaDataContent, '$.ConfigurationLabel')) as configurationLabel, "
					"JSON_UNQUOTE(JSON_EXTRACT (ij.metaDataContent, '$.LiveRecorderVirtualVODProfileLabel')) as liveRecorderVirtualVODProfileLabel, "
					"JSON_EXTRACT (ij.metaDataContent, '$.LiveRecorderVirtualVODMaxDuration') as liveRecorderVirtualVODMaxDuration, "
					"JSON_EXTRACT (ij.metaDataContent, '$.SegmentDuration') as segmentDuration, "
					"JSON_UNQUOTE(JSON_EXTRACT (ij.metaDataContent, '$.Retention')) as retention, "
					"JSON_UNQUOTE(JSON_EXTRACT (ij.metaDataContent, '$.InternalMMS.userKey')) as userKey, "
					"JSON_UNQUOTE(JSON_EXTRACT (ij.metaDataContent, '$.InternalMMS.apiKey')) as apiKey "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
					"where ir.ingestionRootKey = ij.ingestionRootKey "
					"and ij.ingestionType = 'Live-Recorder' "
					"and JSON_EXTRACT (ij.metaDataContent, '$.LiveRecorderVirtualVOD') = true "
					"and (ij.status = 'EncodingQueued' "
					"or (ij.status = 'End_TaskSuccess' and "
						"NOW() <= DATE_ADD(ij.endProcessing, INTERVAL ? MINUTE))) "
				);
			// This select returns all the ingestion job key of running LiveRecording

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt(queryParameterIndex++, toleranceMinutes);
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			while (resultSet->next())
			{
				int64_t workspaceKey = resultSet->getInt64("workspaceKey");
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

				int segmentDuration = -1;
				if (!resultSet->isNull("segmentDuration"))
					segmentDuration = resultSet->getInt("segmentDuration");

				string liveRecorderVirtualVODProfileLabel;
				if (!resultSet->isNull("liveRecorderVirtualVODProfileLabel"))
					liveRecorderVirtualVODProfileLabel = resultSet->getString("liveRecorderVirtualVODProfileLabel");

				int liveRecorderVirtualVODMaxDuration = -1;
				if (!resultSet->isNull("liveRecorderVirtualVODMaxDuration"))
					liveRecorderVirtualVODMaxDuration = resultSet->getInt("liveRecorderVirtualVODMaxDuration");

				string retention;
				if (!resultSet->isNull("retention"))
					retention = resultSet->getString("retention");

				string configurationLabel = resultSet->getString("configurationLabel");
				int64_t userKey = resultSet->getInt64("userKey");
				string apiKey = resultSet->getString("apiKey");

				runningLiveRecordersDetails.push_back(
					make_tuple(workspaceKey, ingestionJobKey,
						liveRecorderVirtualVODMaxDuration,
						liveRecorderVirtualVODProfileLabel, segmentDuration,                       
						configurationLabel, retention, userKey,                      
						apiKey)
				);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count();
		_logger->info(__FILEREF__ + "getRunningLiveRecordersDetails"
			+ ", elapsed in seconds: " + to_string(elapsedInSeconds)
		);
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
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
            _connectionPool->unborrow(conn);
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

    try
    {
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select currentDirLevel1, currentDirLevel2, currentDirLevel3 from MMS_WorkspaceMoreInfo where workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }    
    
    return relativePathToBeUsed;
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
	string, string, string, string, int64_t, bool>
	MMSEngineDBFacade::getStorageDetails(
		int64_t physicalPathKey
)
{
        
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        int64_t workspaceKey;
        bool externalReadOnlyStorage;
        int64_t encodingProfileKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
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
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
                sizeInBytes = resultSet->getInt64("sizeInBytes");
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
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        _connectionPool->unborrow(conn);
		conn = nullptr;

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        return make_tuple(physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath,
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage);
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
}

tuple<int64_t, MMSEngineDBFacade::DeliveryTechnology, int, shared_ptr<Workspace>,
	string, string, string, string, int64_t, bool>
	MMSEngineDBFacade::getStorageDetails(
		int64_t mediaItemKey,
		// encodingProfileKey == -1 means it is requested the source file (the one having the bigger size in case there are more than one)
		int64_t encodingProfileKey,
		bool warningIfMissing
)
{

    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
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
        int64_t sizeInBytes;
        string deliveryFileName;
        string title;
		ContentType contentType;
		if (encodingProfileKey != -1)
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.title, mi.contentType, mi.deliveryFileName, pp.externalReadOnlyStorage, "
				"pp.physicalPathKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? "
                "and pp.encodingProfileKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
                sizeInBytes = resultSet->getInt64("sizeInBytes");
            }

			if (physicalPathKey == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/EncodingProfileKey are not present"
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

				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
			tuple<int64_t, int, string, string, int64_t, bool> sourcePhysicalPathDetails =
				getSourcePhysicalPath(mediaItemKey, warningIfMissing);
			tie(physicalPathKey, mmsPartitionNumber, relativePath,
					fileName, sizeInBytes, externalReadOnlyStorage) = sourcePhysicalPathDetails;

            lastSQLCommand = string("") +
                "select workspaceKey, contentType, title, deliveryFileName "
                "from MMS_MediaItem where mediaItemKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
        _connectionPool->unborrow(conn);
		conn = nullptr;

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        return make_tuple(physicalPathKey, deliveryTechnology, mmsPartitionNumber, workspace, relativePath,
			fileName, deliveryFileName, title, sizeInBytes, externalReadOnlyStorage);
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(MediaItemKeyNotFound e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

void MMSEngineDBFacade::getAllStorageDetails(
        int64_t mediaItemKey,
		vector<tuple<MMSEngineDBFacade::DeliveryTechnology, int, string, string, string, int64_t, bool>>&
		allStorageDetails
)
{

    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
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
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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
            
					shared_ptr<sql::ResultSet> technologyResultSet (technologyPreparedStatement->executeQuery());
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
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

// at least one between physicalPathKey and liveURLConfKey has to be -1
int64_t MMSEngineDBFacade::createDeliveryAuthorization(
    int64_t userKey,
    string clientIPAddress,
    int64_t physicalPathKey,	// vod key
	int64_t liveURLConfKey,		// live key
    string deliveryURI,
    int ttlInSeconds,
    int maxRetries)
{
    int64_t     deliveryAuthorizationKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
			string contentType;
			int64_t contentKey;
			if (physicalPathKey == -1)
			{
				contentType = "live";
				contentKey = liveURLConfKey;
			}
			else
			{
				contentType = "vod";
				contentKey = physicalPathKey;
			}
            lastSQLCommand = 
                "insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, contentType, contentKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
                "NULL, ?, ?, ?, ?, ?, ?, 0, ?)";

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

            preparedStatement->executeUpdate();
        }
                    
        deliveryAuthorizationKey = getLastInsertId(conn);
        
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
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
            _connectionPool->unborrow(conn);
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

    try
    {
        conn = _connectionPool->borrow();	
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

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
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

            preparedStatement->executeUpdate();
        }
                    
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        // throw e;
    }        
    catch(exception e)
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
            _connectionPool->unborrow(conn);
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

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			// Once authorization is expired, we will still take it for 1 day
			int retention = 3600 * 24;

            lastSQLCommand = 
                "delete from MMS_DeliveryAuthorization "
				"where DATE_ADD(authorizationTimestamp, INTERVAL (ttlInSeconds + ?) SECOND) < NOW()";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, retention);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated > 0)
            {
                _logger->info(__FILEREF__ + "Deletion obsolete DeliveryAuthorization"
                    + ", rowsUpdated: " + to_string(rowsUpdated)
                );
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }
    catch(runtime_error e)
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
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }
    catch(exception e)
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
            _connectionPool->unborrow(conn);
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
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

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
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return lastInsertId;
}

