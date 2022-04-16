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
    string defaultCharacterSet = configuration["database"].get("defaultCharacterSet", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", database->defaultCharacterSet: " + defaultCharacterSet
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
		// dbPassword = encryptedPassword;
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

	_ffmpegEncoderUser = configuration["ffmpeg"].get("encoderUser", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
	);
	_ffmpegEncoderPassword = configuration["ffmpeg"].get("encoderPassword", "").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->encoderPassword: " + "..."
	);
	_ffmpegEncoderStatusURI = configuration["ffmpeg"].get("encoderStatusURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI
    );
	_ffmpegEncoderInfoURI = configuration["ffmpeg"].get("encoderInfoURI", "").asString();
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
    _contentNotTransferredRetentionInHours    = JSONUtils::asInt(configuration["mms"], "contentNotTransferredRetentionInHours", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentNotTransferredRetentionInHours: " + to_string(_contentNotTransferredRetentionInHours)
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

	_getIngestionJobsCurrentIndex	= 0;
	_getEncodingJobsCurrentIndex	= 0;

	_logger->info(__FILEREF__ + "Looking for adminEmailAddresses");
	for(int adminEmailAddressesIndex = 0;
			adminEmailAddressesIndex < configuration["api"]["adminEmailAddresses"].size();
			adminEmailAddressesIndex++)
	{
		string adminEmailAddress = configuration["api"]["adminEmailAddresses"]
			[adminEmailAddressesIndex].asString();
		_adminEmailAddresses.push_back(adminEmailAddress);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", mms->adminEmailAddresses[adminEmailAddressesIndex]: " + adminEmailAddress
		);
	}

    _logger->info(__FILEREF__ + "Creating MySQLConnectionFactory...");
	bool reconnect = true;
	// string defaultCharacterSet = "utf8";
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", active: 0"
				+ ", processorMMS: " + processorMMS
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

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
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
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
			_logger->info(__FILEREF__ + "@SQL statistics@"
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
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "reset updateIngestionJob failed"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + "End_CanceledByMMS"
						+ ", errorMessage: " + errorMessage
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
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
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
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
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
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
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMS: " + processorMMS
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();            
			_logger->info(__FILEREF__ + "@SQL statistics@ (resetProcessingJobsIfNeeded)"
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
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", toleranceMinutes: " + to_string(toleranceMinutes)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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
                sizeInBytes = resultSet->getInt64("sizeInBytes");
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

			// we will remove by steps to avoid error because of transaction log overflow
			int maxToBeRemoved = 100;
			int totalRowsRemoved = 0;
			bool moreRowsToBeRemoved = true;
			while (moreRowsToBeRemoved)
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
			}

			_logger->info(__FILEREF__ + "Deletion obsolete DeliveryAuthorization"
				+ ", totalRowsRemoved: " + to_string(totalRowsRemoved)
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

