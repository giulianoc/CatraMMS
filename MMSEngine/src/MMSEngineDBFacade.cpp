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
    _dbConnectionPoolStatsReportPeriodInSeconds = configuration["database"].get("dbConnectionPoolStatsReportPeriodInSeconds", 5).asInt();
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

    _maxEncodingFailures            = configuration["encoding"].get("maxEncodingFailures", 3).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->maxEncodingFailures: " + to_string(_maxEncodingFailures)
    );

    _confirmationCodeRetentionInDays    = configuration["mms"].get("confirmationCodeRetentionInDays", 3).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
    );

    _contentRetentionInMinutesDefaultValue    = configuration["mms"].get("contentRetentionInMinutesDefaultValue", 1).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentRetentionInMinutesDefaultValue: " + to_string(_contentRetentionInMinutesDefaultValue)
    );
    _contentNotTransferredRetentionInDays    = configuration["mms"].get("contentNotTransferredRetentionInDays", 1).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", mms->contentNotTransferredRetentionInDays: " + to_string(_contentNotTransferredRetentionInDays)
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
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = null, transcoder = null where processorMMS = ? and status = ?";
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

void MMSEngineDBFacade::manageMainAndBackupOfRunnungLiveRecordingHA()

{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
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
							"and retentionInMinutes != 0 "
							"group by JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') "
								"having count(*) = 2 for update"
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

						int64_t mediaItemKeyChunk_1;
						ContentType contentType;
						bool mainChunk_1;
						int64_t durationInMilliSecondsChunk_1;
						int64_t mediaItemKeyChunk_2;
						bool mainChunk_2;
						int64_t durationInMilliSecondsChunk_2;

						lastSQLCommand =
							string("select mediaItemKey, contentType, "
									"CAST(JSON_EXTRACT(userData, '$.mmsData.main') as SIGNED INTEGER) as main from MMS_MediaItem "
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
							mediaItemKeyChunk_1 =
								resultSetMediaItemDetails->getInt64("mediaItemKey");
							contentType = MMSEngineDBFacade::toContentType(resultSetMediaItemDetails->getString("contentType"));
							mainChunk_1 =
								resultSetMediaItemDetails->getInt("main") == 1 ? true : false;

							if (contentType == ContentType::Video)
							{
								int videoWidth;
								int videoHeight;
								long bitRate;
								string videoCodecName;
								string videoProfile;
								string videoAvgFrameRate;
								long videoBitRate;
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
									videoDetails = getVideoDetails(mediaItemKeyChunk_1,
											physicalPathKey);

								tie(durationInMilliSecondsChunk_1, bitRate,
									videoCodecName, videoProfile, videoWidth, videoHeight,
										videoAvgFrameRate, videoBitRate,
									audioCodecName, audioSampleRate, audioChannels, audioBitRate)
										= videoDetails;
							}
							else // if (contentType == ContentType::Audio)
							{
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,string,long,long,int> audioDetails =
									getAudioDetails(mediaItemKeyChunk_1, physicalPathKey);

								tie(durationInMilliSecondsChunk_1, 
									audioCodecName, audioBitRate, audioSampleRate, audioChannels)
										= audioDetails;
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

							if (contentType == ContentType::Video)
							{
								int videoWidth;
								int videoHeight;
								long bitRate;
								string videoCodecName;
								string videoProfile;
								string videoAvgFrameRate;
								long videoBitRate;
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long>
									videoDetails = getVideoDetails(mediaItemKeyChunk_2,
											physicalPathKey);

								tie(durationInMilliSecondsChunk_2, bitRate,
									videoCodecName, videoProfile, videoWidth, videoHeight,
										videoAvgFrameRate, videoBitRate,
									audioCodecName, audioSampleRate, audioChannels, audioBitRate)
										= videoDetails;
							}
							else // if (contentType == ContentType::Audio)
							{
								string audioCodecName;
								long audioSampleRate;
								int audioChannels;
								long audioBitRate;

								int64_t physicalPathKey = -1;
								tuple<int64_t,string,long,long,int> audioDetails =
									getAudioDetails(mediaItemKeyChunk_2, physicalPathKey);

								tie(durationInMilliSecondsChunk_2, 
									audioCodecName, audioBitRate, audioSampleRate, audioChannels)
										= audioDetails;
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
							_logger->error(__FILEREF__ + "It should never happen"
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
							if (durationInMilliSecondsChunk_1 == durationInMilliSecondsChunk_2)
							{
								if (mainChunk_1)
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
								}
								else
								{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
								}
							}
							else if (durationInMilliSecondsChunk_1 < durationInMilliSecondsChunk_2)
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_1;
									mediaItemKeyValidated = mediaItemKeyChunk_2;
							}
							else
							{
									mediaItemKeyNotValidated = mediaItemKeyChunk_2;
									mediaItemKeyValidated = mediaItemKeyChunk_1;
							}

							_logger->info(__FILEREF__
								+ "Manage HA LiveRecording, reset of chunk, main and backup (couple)"
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
								preparedStatementUpdate->setInt64(
										queryParameterIndex++, mediaItemKeyValidated);

								int rowsUpdated = preparedStatementUpdate->executeUpdate();
								if (rowsUpdated != 1)
									_logger->error(__FILEREF__ + "It should never happen"
										+ ", mediaItemKeyToBeValidated: "
										+ to_string(mediaItemKeyValidated)
									);
							}

							// mediaItemKeyNotValidated
							{
								lastSQLCommand = 
									"update MMS_MediaItem set retentionInMinutes = 0, userData = JSON_SET(userData, '$.mmsData.validated', false) "
									"where mediaItemKey = ?";
								shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
					int chunksToBeManagedWithinSeconds = 60;

					_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single)"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						);

					lastSQLCommand =
						string("select mediaItemKey  from MMS_MediaItem where "
							"JSON_EXTRACT(userData, '$.mmsData.dataType') = 'liveRecordingChunk' "
							"and JSON_EXTRACT(userData, '$.mmsData.ingestionJobKey') = ? "
							"and JSON_EXTRACT(userData, '$.mmsData.validated') is null "
							"and NOW() > DATE_ADD(ingestionDate, INTERVAL ? SECOND) "
							"and retentionInMinutes != 0 for update"
						);

					shared_ptr<sql::PreparedStatement> preparedStatementMediaItemKey (conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementMediaItemKey->setInt64(queryParameterIndex++, ingestionJobKey);
					preparedStatementMediaItemKey->setInt(queryParameterIndex++, chunksToBeManagedWithinSeconds);
					shared_ptr<sql::ResultSet> resultSetMediaItemKey (preparedStatementMediaItemKey->executeQuery());
					while (resultSetMediaItemKey->next())
					{
						int64_t mediaItemKeyChunk = resultSetMediaItemKey->getInt64("mediaItemKey");

						_logger->info(__FILEREF__ + "Manage HA LiveRecording, main or backup (single), set to validated"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
							);

						{
							lastSQLCommand = 
								"update MMS_MediaItem set userData = JSON_SET(userData, '$.mmsData.validated', true) "
								"where mediaItemKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementUpdate->setInt64(queryParameterIndex++, mediaItemKeyChunk);

							int rowsUpdated = preparedStatementUpdate->executeUpdate();            
							if (rowsUpdated != 1)
								_logger->error(__FILEREF__ + "It should never happen"
									+ ", mediaItemKeyChunk: " + to_string(mediaItemKeyChunk)
								);
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

tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> MMSEngineDBFacade::getStorageDetails(
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
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
        string deliveryFileName;
        string title;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.title, mi.deliveryFileName, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and pp.physicalPathKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
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

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes);
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

tuple<int64_t,int,shared_ptr<Workspace>,string,string,string,string,int64_t> MMSEngineDBFacade::getStorageDetails(
        int64_t mediaItemKey,
        int64_t encodingProfileKey
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
        int64_t physicalPathKey;
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        int64_t sizeInBytes;
        string deliveryFileName;
        string title;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, mi.title, mi.deliveryFileName, pp.physicalPathKey, pp.partitionNumber, pp.relativePath, pp.fileName, pp.sizeInBytes "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? "
                "and pp.encodingProfileKey " + (encodingProfileKey == -1 ? "is null" : "= ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            if (encodingProfileKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                title = resultSet->getString("title");
                if (!resultSet->isNull("deliveryFileName"))
                    deliveryFileName = resultSet->getString("deliveryFileName");
                physicalPathKey = resultSet->getInt64("physicalPathKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");
                sizeInBytes = resultSet->getInt64("sizeInBytes");
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/EncodingProfileKey are not present"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(physicalPathKey, mmsPartitionNumber, workspace, relativePath, fileName, deliveryFileName, title, sizeInBytes);
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

void MMSEngineDBFacade::getAllStorageDetails(
        int64_t mediaItemKey, vector<tuple<int,string,string,string>>& allStorageDetails
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
        int mmsPartitionNumber;
        string relativePath;
        string fileName;
        {
            lastSQLCommand = string("") +
                "select mi.workspaceKey, pp.partitionNumber, pp.relativePath, pp.fileName "
                "from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.mediaItemKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                workspaceKey = resultSet->getInt64("workspaceKey");
                mmsPartitionNumber = resultSet->getInt("partitionNumber");
                relativePath = resultSet->getString("relativePath");
                fileName = resultSet->getString("fileName");

                shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);
        
                tuple<int,string,string,string> storageDetails =
                    make_tuple(mmsPartitionNumber, workspace->_directoryName, relativePath, fileName);
                
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

int64_t MMSEngineDBFacade::createDeliveryAuthorization(
    int64_t userKey,
    string clientIPAddress,
    int64_t physicalPathKey,
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
            lastSQLCommand = 
                "insert into MMS_DeliveryAuthorization(deliveryAuthorizationKey, userKey, clientIPAddress, physicalPathKey, deliveryURI, ttlInSeconds, currentRetriesNumber, maxRetries) values ("
                "NULL, ?, ?, ?, ?, ?, 0, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            if (clientIPAddress == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, clientIPAddress);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
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

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

bool MMSEngineDBFacade::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
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

