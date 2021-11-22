
#include "PersistenceLock.h"
#include "MMSEngineDBFacade.h"

void MMSEngineDBFacade::getIngestionsToBeManaged(
        vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType,
		IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs,
		bool onlyTasksNotInvolvingMMSEngineThreads
)
{
    string      lastSQLCommand;
    bool        autoCommit = true;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
		int milliSecondsToSleepWaitingLock = 500;

		PersistenceLock persistenceLock(this,
			MMSEngineDBFacade::LockType::Ingestion,
			_maxSecondsToWaitCheckIngestionLock,
			processorMMS, "CheckIngestion",
			milliSecondsToSleepWaitingLock, _logger);

        chrono::system_clock::time_point startPoint = chrono::system_clock::now();
        if (startPoint - _lastConnectionStatsReport >=
				chrono::seconds(_dbConnectionPoolStatsReportPeriodInSeconds))
        {
            _lastConnectionStatsReport = chrono::system_clock::now();
            
            DBConnectionPoolStats dbConnectionPoolStats = _connectionPool->get_stats();	

            _logger->info(__FILEREF__ + "DB connection pool stats"
                + ", _poolSize: " + to_string(dbConnectionPoolStats._poolSize)
                + ", _borrowedSize: " + to_string(dbConnectionPoolStats._borrowedSize)
            );
        }

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // We have the Transaction because previously there was a select for update and then the update.
        // Now we have first the update and than the select. Probable the Transaction does not need,
		// anyway I left it
        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

		chrono::system_clock::time_point pointAfterLive;
		chrono::system_clock::time_point pointAfterNotLive;

		int liveProxyToBeIngested = 0;
		int liveRecorderToBeIngested = 0;
		int othersToBeIngested = 0;

        // ingested jobs that do not have to wait a dependency
        {
			// first Live-Proxy/VOD-Proxy (because if we have many many Live-Recorder,
			// Live-Proxy will never start)
			// We will add also the YouTube-Live-Broadcast because this task has also a Schedule
			{
				int minutesAheadToConsiderLiveProxy = 5;

				lastSQLCommand = 
					"select ij.ingestionJobKey, ij.label, ir.workspaceKey, ij.metaDataContent, "
					"ij.status, ij.ingestionType, "
					"DATE_FORMAT(convert_tz(ir.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
					"where ir.ingestionRootKey = ij.ingestionRootKey and "
					"ij.processorMMS is null "
					"and ij.ingestionType in ('Live-Proxy', 'VOD-Proxy', 'YouTube-Live-Broadcast') "
					"and (ij.status = ? or (ij.status in (?, ?, ?, ?) "
					"and ij.sourceBinaryTransferred = 1)) "
					// 2021-05-24: Ho dovuto commentare il controllo successivo che non considera i jobs troppo "vecchi"
					//	rispetto al campo processingStartingFrom perchè il seguente scenario non era gestito:
					//	1. viene schedulato NOW un LiveProxy che dovrà partire fra 30 gg
					//	In questo scenario il job non partirebbe perchè quando la condizione del TimePeriod
					//	sarebbe 'vera', la condizione del job "troppo vecchio" è false
					//	Per questo motivo ho commentato la condizione sotto che, essendo un job con TimePeriod,
					//	non considera la condizione di job "troppo vecchio"
					"and ij.processingStartingFrom <= NOW() " // and NOW() <= DATE_ADD(ij.processingStartingFrom, INTERVAL ? DAY) "
					"and ("
						"JSON_EXTRACT(ij.metaDataContent, '$.TimePeriod') is null "
						"or JSON_EXTRACT(ij.metaDataContent, '$.TimePeriod') = false "
						"or (JSON_EXTRACT(ij.metaDataContent, '$.TimePeriod') = true and "
							"UNIX_TIMESTAMP(convert_tz(STR_TO_DATE(JSON_EXTRACT(ij.metaDataContent, '$.ProxyPeriod.Start'), '\"%Y-%m-%dT%H:%i:%sZ\"'), '+00:00', @@session.time_zone)) - UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL ? MINUTE)) < 0) "
					") "
					// "for update "
					;
					// "limit ? offset ?"
					// "limit ? offset ? for update"
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndexIngestionJob = 1;
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
				// preparedStatement->setInt(queryParameterIndexIngestionJob++,
				// 	_doNotManageIngestionsOlderThanDays);
				preparedStatement->setInt(queryParameterIndexIngestionJob++,
					minutesAheadToConsiderLiveProxy);

				// preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlRowCount);
				// preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlOffset);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", IngestionStatus::Start_TaskQueued: " + MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued)
					+ ", IngestionStatus::SourceDownloadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress)
					+ ", IngestionStatus::SourceMovingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress)
					+ ", IngestionStatus::SourceCopingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress)
					+ ", IngestionStatus::SourceUploadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress)
					+ ", minutesAheadToConsiderLiveProxy: " + to_string(minutesAheadToConsiderLiveProxy)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);

				while (resultSet->next())
				{
					int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
					string ingestionJobLabel	= resultSet->getString("label");
					int64_t workspaceKey         = resultSet->getInt64("workspaceKey");
					string metaDataContent      = resultSet->getString("metaDataContent");
					IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(
						resultSet->getString("status"));
					IngestionType ingestionType     = MMSEngineDBFacade::toIngestionType(
						resultSet->getString("ingestionType"));
					string ingestionDate      = resultSet->getString("ingestionDate");

					tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> 
						ingestionJobToBeManagedInfo = isIngestionJobToBeManaged(
						ingestionJobKey, workspaceKey, ingestionStatus, ingestionType, conn);

					bool ingestionJobToBeManaged;
					int64_t dependOnIngestionJobKey;
					int dependOnSuccess;
					IngestionStatus ingestionStatusDependency;

					tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess,
						ingestionStatusDependency) = ingestionJobToBeManagedInfo;

					if (ingestionJobToBeManaged)
					{
						_logger->info(__FILEREF__ + "Adding jobs to be processed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ingestionStatus: " + toString(ingestionStatus)
						);
                       
						shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

						tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType,
							IngestionStatus> ingestionToBeManaged
                               = make_tuple(ingestionJobKey, ingestionJobLabel, workspace, ingestionDate,
									   metaDataContent, ingestionType, ingestionStatus);

						ingestionsToBeManaged.push_back(ingestionToBeManaged);
						liveProxyToBeIngested++;
					}
					else
					{
						_logger->debug(__FILEREF__ + "Ingestion job cannot be processed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ingestionStatus: " + toString(ingestionStatus)
							+ ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
							+ ", dependOnSuccess: " + to_string(dependOnSuccess)
							+ ", ingestionStatusDependency: " + toString(ingestionStatusDependency)
						);
					}
				}
            }

			// 2019-03-31: scenario: we have a long list of encodings to be done (113 encodings) and among these we have 2 live recordings.
			//	The Live-Recorder started after about 60 minutes. This is becasue the select returns all the other encodings and at the end
			//	the Live Recorder. To avoid this problem we force to have first the Live-Recorder and after all the others
			{
				// the last condition (and UNIX_TIMESTAMP(....) specifies that, if a Live Recorder has to start in one hour,
				// it has not to be considered. It means the select has to return only the Live-Recorder close to start.
				int minutesAheadToConsiderLiveRecorder = 15;

				lastSQLCommand = 
					"select ij.ingestionJobKey, ij.label, ir.workspaceKey, ij.metaDataContent, ij.status, "
						"ij.ingestionType, "
						"DATE_FORMAT(convert_tz(ir.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
						"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
						"where ir.ingestionRootKey = ij.ingestionRootKey and ij.processorMMS is null "
						"and ij.ingestionType = 'Live-Recorder' "
						"and (ij.status = ? or (ij.status in (?, ?, ?, ?) and ij.sourceBinaryTransferred = 1)) "
						// 2021-05-24: Ho dovuto commentare il controllo successivo che non considera i jobs troppo "vecchi"
						//	rispetto al campo processingStartingFrom perchè il seguente scenario non era gestito:
						//	1. viene schedulato NOW un LiveRecorder che dovrà partire fra 30 gg
						//	In questo scenario il job non partirebbe perchè quando la condizione del TimePeriod
						//	sarebbe 'vera', la condizione del job "troppo vecchio" è false
						//	Per questo motivo ho commentato la condizione sotto che, essendo un job con TimePeriod,
						//	non considera la condizione di job "troppo vecchio"
						"and ij.processingStartingFrom <= NOW() " // and NOW() <= DATE_ADD(ij.processingStartingFrom, INTERVAL ? DAY) "
						"and UNIX_TIMESTAMP(convert_tz(STR_TO_DATE(JSON_EXTRACT(ij.metaDataContent, '$.RecordingPeriod.Start'), '\"%Y-%m-%dT%H:%i:%sZ\"'), '+00:00', @@session.time_zone)) - UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL ? MINUTE)) < 0"
						// "for update "
						;
						// "limit ? offset ?"
						// "limit ? offset ? for update"
				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndexIngestionJob = 1;
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
				preparedStatement->setString(queryParameterIndexIngestionJob++,
					MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
				// preparedStatement->setInt(queryParameterIndexIngestionJob++,
				// 	_doNotManageIngestionsOlderThanDays);
				preparedStatement->setInt(queryParameterIndexIngestionJob++,
					minutesAheadToConsiderLiveRecorder);

				// preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlRowCount);
				// preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlOffset);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", IngestionStatus::Start_TaskQueued: " + MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued)
					+ ", IngestionStatus::SourceDownloadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress)
					+ ", IngestionStatus::SourceMovingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress)
					+ ", IngestionStatus::SourceCopingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress)
					+ ", IngestionStatus::SourceUploadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress)
					+ ", minutesAheadToConsiderLiveRecorder: " + to_string(minutesAheadToConsiderLiveRecorder)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);

				while (resultSet->next())
				{
					int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
					string ingestionJobLabel	= resultSet->getString("label");
					int64_t workspaceKey         = resultSet->getInt64("workspaceKey");
					string metaDataContent      = resultSet->getString("metaDataContent");
					IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(
						resultSet->getString("status"));
					IngestionType ingestionType     = MMSEngineDBFacade::toIngestionType(
						resultSet->getString("ingestionType"));
					string ingestionDate      = resultSet->getString("ingestionDate");

					tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> 
						ingestionJobToBeManagedInfo = isIngestionJobToBeManaged(
						ingestionJobKey, workspaceKey, ingestionStatus, ingestionType, conn);

					bool ingestionJobToBeManaged;
					int64_t dependOnIngestionJobKey;
					int dependOnSuccess;
					IngestionStatus ingestionStatusDependency;

					tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess,
						ingestionStatusDependency) = ingestionJobToBeManagedInfo;

					if (ingestionJobToBeManaged)
					{
						_logger->info(__FILEREF__ + "Adding jobs to be processed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ingestionStatus: " + toString(ingestionStatus)
						);
                       
						shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

						tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType,
							IngestionStatus> ingestionToBeManaged = make_tuple(ingestionJobKey,
								ingestionJobLabel, workspace, ingestionDate, metaDataContent,
								ingestionType, ingestionStatus);

						ingestionsToBeManaged.push_back(ingestionToBeManaged);
						liveRecorderToBeIngested++;
					}
					else
					{
						_logger->debug(__FILEREF__ + "Ingestion job cannot be processed"
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", ingestionStatus: " + toString(ingestionStatus)
							+ ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
							+ ", dependOnSuccess: " + to_string(dependOnSuccess)
							+ ", ingestionStatusDependency: " + toString(ingestionStatusDependency)
						);
					}
				}
            }

			pointAfterLive = chrono::system_clock::now();

            int mysqlOffset = 0;
            int mysqlRowCount = _ingestionJobsSelectPageSize;
            bool moreRows = true;
            while(ingestionsToBeManaged.size() < maxIngestionJobs && moreRows)
            {
				{
					_logger->info(__FILEREF__ + "getIngestionsToBeManaged"
						+ ", mysqlOffset: " + to_string(mysqlOffset)
						+ ", ingestionsToBeManaged.size(): " + to_string(ingestionsToBeManaged.size())
						+ ", moreRows: " + to_string(moreRows)
					);

					lastSQLCommand = 
						"select ij.ingestionJobKey, ij.label, ir.workspaceKey, "
						"ij.metaDataContent, ij.status, ij.ingestionType, "
						"DATE_FORMAT(convert_tz(ir.ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
						"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
						"where ir.ingestionRootKey = ij.ingestionRootKey "
						"and ij.processorMMS is null ";
					if (onlyTasksNotInvolvingMMSEngineThreads)
					{
						string tasksNotInvolvingMMSEngineThreadsList =
							"'GroupOfTask'"
							", 'Encode'"
							", 'Video-Speed'"
							", 'Picture-InPicture'"
							", 'Intro-Outro-Overlay'"
							", 'Periodical-Frames'"
							", 'I-Frames'"
							", 'Motion-JPEG-by-Periodical-Frames'"
							", 'Motion-JPEG-by-I-Frames'"
							", 'Slideshow'"
							", 'Overlay-Image-On-Video'"
							", 'Overlay-Text-On-Video'"
							", 'Media-Cross-Reference'"
							", 'Face-Recognition'"
							", 'Face-Identification'"
							// ", 'Live-Recorder'"	already asked before
							// ", 'Live-Proxy', 'VODProxy'"	already asked before
							", 'Awaiting-The-Beginning'"
							", 'Live-Grid'"
						;
						lastSQLCommand += "and ij.ingestionType in (" + tasksNotInvolvingMMSEngineThreadsList + ") ";
					}
					else
					{
						// everything but Live-Recorder, Live-Proxy, VOD-Proxy
						// already asked before
						lastSQLCommand +=
							"and ij.ingestionType not in ('Live-Recorder', 'Live-Proxy', 'VOD-Proxy', 'YouTube-Live-Broadcast') ";
					}
					lastSQLCommand +=
						"and (ij.status = ? or (ij.status in (?, ?, ?, ?) "
						"and ij.sourceBinaryTransferred = 1)) "
						"and ij.processingStartingFrom <= NOW() "
						"and NOW() <= DATE_ADD(ij.processingStartingFrom, INTERVAL ? DAY) "
						"order by ij.priority asc, ij.processingStartingFrom asc "
						"limit ? offset ?"
						// "limit ? offset ? for update"
					;
					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndexIngestionJob = 1;
					preparedStatement->setString(queryParameterIndexIngestionJob++,
					   	MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued));
					preparedStatement->setString(queryParameterIndexIngestionJob++,
					   	MMSEngineDBFacade::toString(IngestionStatus::
							SourceDownloadingInProgress));
					preparedStatement->setString(queryParameterIndexIngestionJob++,
					   	MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
					preparedStatement->setString(queryParameterIndexIngestionJob++,
					   	MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
					preparedStatement->setString(queryParameterIndexIngestionJob++,
					   	MMSEngineDBFacade::toString(IngestionStatus::
							SourceUploadingInProgress));
					preparedStatement->setInt(queryParameterIndexIngestionJob++,
						_doNotManageIngestionsOlderThanDays);

					preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlRowCount);
					preparedStatement->setInt(queryParameterIndexIngestionJob++, mysqlOffset);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", IngestionStatus::Start_TaskQueued: "
							+ MMSEngineDBFacade::toString(IngestionStatus::Start_TaskQueued)
						+ ", IngestionStatus::SourceDownloadingInProgress: "
							+ MMSEngineDBFacade::toString(IngestionStatus::
								SourceDownloadingInProgress)
						+ ", IngestionStatus::SourceMovingInProgress: "
							+ MMSEngineDBFacade::toString(IngestionStatus::
								SourceMovingInProgress)
						+ ", IngestionStatus::SourceCopingInProgress: "
							+ MMSEngineDBFacade::toString(IngestionStatus::
								SourceCopingInProgress)
						+ ", IngestionStatus::SourceUploadingInProgress: "
							+ MMSEngineDBFacade::toString(IngestionStatus::
								SourceUploadingInProgress)
						+ ", _doNotManageIngestionsOlderThanDays: "
							+ to_string(_doNotManageIngestionsOlderThanDays)
						+ ", mysqlRowCount: " + to_string(mysqlRowCount)
						+ ", mysqlOffset: " + to_string(mysqlOffset)
						+ ", onlyTasksNotInvolvingMMSEngineThreads: "
							+ to_string(onlyTasksNotInvolvingMMSEngineThreads)
						+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
						+ ", elapsed (secs): @" + to_string(
							chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);

					if (resultSet->rowsCount() < mysqlRowCount)
						moreRows = false;
					else
						moreRows = true;
					mysqlOffset += _ingestionJobsSelectPageSize;

					while (resultSet->next())
					{
						int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
						string ingestionJobLabel	= resultSet->getString("label");
						int64_t workspaceKey         = resultSet->getInt64("workspaceKey");
						string metaDataContent      = resultSet->getString("metaDataContent");
						IngestionStatus ingestionStatus     = MMSEngineDBFacade::
							toIngestionStatus(resultSet->getString("status"));
						IngestionType ingestionType     = MMSEngineDBFacade::toIngestionType(
							resultSet->getString("ingestionType"));
						string ingestionDate      = resultSet->getString("ingestionDate");

						tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> 
							ingestionJobToBeManagedInfo = isIngestionJobToBeManaged(
							ingestionJobKey, workspaceKey, ingestionStatus,
							ingestionType, conn);

						bool ingestionJobToBeManaged;
						int64_t dependOnIngestionJobKey;
						int dependOnSuccess;
						IngestionStatus ingestionStatusDependency;

						tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess,
							ingestionStatusDependency) = ingestionJobToBeManagedInfo;

						if (ingestionJobToBeManaged)
						{
							_logger->info(__FILEREF__ + "Adding jobs to be processed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", ingestionStatus: " + toString(ingestionStatus)
							);
                        
							shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

							tuple<int64_t, string, shared_ptr<Workspace>, string, string,
								IngestionType, IngestionStatus> ingestionToBeManaged =
									make_tuple(ingestionJobKey, ingestionJobLabel, workspace,
									ingestionDate, metaDataContent, ingestionType,
									ingestionStatus);

							ingestionsToBeManaged.push_back(ingestionToBeManaged);
							othersToBeIngested++;

							if (ingestionsToBeManaged.size() >= maxIngestionJobs)
								break;
						}
						else
						{
							_logger->info(__FILEREF__ + "Ingestion job cannot be processed"
								+ ", ingestionJobKey: " + to_string(ingestionJobKey)
								+ ", ingestionStatus: " + toString(ingestionStatus)
								+ ", dependOnIngestionJobKey: "
									+ to_string(dependOnIngestionJobKey)
								+ ", dependOnSuccess: " + to_string(dependOnSuccess)
								+ ", ingestionStatusDependency: "
									+ toString(ingestionStatusDependency)
							);
						}
					}
                }
            }

			pointAfterNotLive = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "getIngestionsToBeManaged (exit)"
				+ ", mysqlOffset: " + to_string(mysqlOffset)
				+ ", ingestionsToBeManaged.size(): " + to_string(ingestionsToBeManaged.size())
				+ ", moreRows: " + to_string(moreRows)
				+ ", onlyTasksNotInvolvingMMSEngineThreads: "
					+ to_string(onlyTasksNotInvolvingMMSEngineThreads)
				+ ", select not live elapsed (secs): " + to_string(
					chrono::duration_cast<chrono::seconds>(
					pointAfterNotLive - pointAfterLive).count())
			);
        }

        for (tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType,
			IngestionStatus>& ingestionToBeManaged: ingestionsToBeManaged)
        {
            int64_t ingestionJobKey;
            MMSEngineDBFacade::IngestionStatus ingestionStatus;

            tie(ingestionJobKey, ignore, ignore, ignore, ignore, ignore,
					ingestionStatus) = ingestionToBeManaged;

            if (ingestionStatus == IngestionStatus::SourceMovingInProgress
                    || ingestionStatus == IngestionStatus::SourceCopingInProgress
                    || ingestionStatus == IngestionStatus::SourceUploadingInProgress)
            {
                // let's consider IngestionStatus::SourceUploadingInProgress
                // In this scenarios, the content was already uploaded by the client
				// (sourceBinaryTransferred = 1),
                // if we set startProcessing = NOW() we would not have any difference
				// with endProcessing
                // So, in this scenarios (SourceUploadingInProgress),
				// startProcessing-endProcessing is the time
                // between the client ingested the Workflow and the content completely uploaded.
                // In this case, if the client has to upload 10 contents sequentially,
				// the last one is the sum of all the other contents

                lastSQLCommand = 
                    "update MMS_IngestionJob set processorMMS = ? where ingestionJobKey = ?";
            }
            else
            {
                lastSQLCommand = 
					"update MMS_IngestionJob set startProcessing = NOW(), "
					"processorMMS = ? where ingestionJobKey = ?";
            }
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processorMMS: " + processorMMS
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

        chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "getIngestionsToBeManaged statistics"
			+ ", total elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
			+ ", select live elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(pointAfterLive - startPoint).count())
			+ ", select not live elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(pointAfterNotLive - pointAfterLive).count())
			+ ", processing entries elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - pointAfterNotLive).count())
			+ ", ingestionsToBeManaged.size: " + to_string(ingestionsToBeManaged.size())
			+ ", maxIngestionJobs: " + to_string(maxIngestionJobs)
			+ ", liveProxyToBeIngested: " + to_string(liveProxyToBeIngested)
			+ ", liveRecorderToBeIngested: " + to_string(liveRecorderToBeIngested)
			+ ", othersToBeIngested: " + to_string(othersToBeIngested)
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
        string exceptionMessage(e.what());
        
        _logger->warn(__FILEREF__ + "SQL exception"
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

tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> 
	MMSEngineDBFacade::isIngestionJobToBeManaged(
		int64_t ingestionJobKey,
		int64_t workspaceKey,
		IngestionStatus ingestionStatus,
		IngestionType ingestionType,
		shared_ptr<MySQLConnection> conn
		)
{

    bool ingestionJobToBeManaged = true;
	int64_t dependOnIngestionJobKey = -1;
	int dependOnSuccess = -1;
	IngestionStatus ingestionStatusDependency;
    string      lastSQLCommand;

    try
    {
		// _logger->info(__FILEREF__ + "Analyzing dependencies for the IngestionJob"
		// + ", ingestionJobKey: " + to_string(ingestionJobKey)
		// );

		bool atLeastOneDependencyRowFound = false;

		lastSQLCommand = 
			"select dependOnIngestionJobKey, dependOnSuccess "
			"from MMS_IngestionJobDependency "
			"where ingestionJobKey = ? order by orderNumber asc";
		shared_ptr<sql::PreparedStatement> preparedStatementDependency (
		conn->_sqlConnection->prepareStatement(lastSQLCommand));
		int queryParameterIndexDependency = 1;
		preparedStatementDependency->setInt64(queryParameterIndexDependency++, ingestionJobKey);

		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		shared_ptr<sql::ResultSet> resultSetDependency (
			preparedStatementDependency->executeQuery());
		_logger->info(__FILEREF__ + "@SQL statistics@"
			+ ", lastSQLCommand: " + lastSQLCommand
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", resultSetDependency->rowsCount: "
				+ to_string(resultSetDependency->rowsCount())
			+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - startSql).count()) + "@"
		);
		// 2019-10-05: GroupOfTasks has always to be executed once the dependencies are in a final state.
		//	This is because the manageIngestionJobStatusUpdate do not update the GroupOfTasks with a state 
		//	like End_NotToBeExecuted
		while (resultSetDependency->next())
		{
			if (!atLeastOneDependencyRowFound)
				atLeastOneDependencyRowFound = true;

			if (!resultSetDependency->isNull("dependOnIngestionJobKey"))
			{
				dependOnIngestionJobKey     = resultSetDependency->getInt64("dependOnIngestionJobKey");
				dependOnSuccess                 = resultSetDependency->getInt("dependOnSuccess");

				lastSQLCommand = 
					"select status from MMS_IngestionJob where ingestionJobKey = ?";
				shared_ptr<sql::PreparedStatement> preparedStatementIngestionJob (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndexStatus = 1;
				preparedStatementIngestionJob->setInt64(queryParameterIndexStatus++, dependOnIngestionJobKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSetIngestionJob (preparedStatementIngestionJob->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
					+ ", resultSetIngestionJob->rowsCount: " + to_string(resultSetIngestionJob->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (resultSetIngestionJob->next())
				{
					string sStatus = resultSetIngestionJob->getString("status");

					// _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
					// + ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
					// + ", dependOnSuccess: " + to_string(dependOnSuccess)
					// + ", status (dependOnIngestionJobKey): " + sStatus
					// );

					ingestionStatusDependency     = MMSEngineDBFacade::toIngestionStatus(sStatus);

					if (ingestionType == IngestionType::GroupOfTasks)
					{
						if (!MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatusDependency))
						{
							// 2019-10-05: In case of GroupOfTasks, it has to be managed when all the dependencies
							// are in a final state
							ingestionJobToBeManaged = false;

							break;
						}
					}
					else
					{
						if (MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatusDependency))
						{
							if (dependOnSuccess == 1
								&& MMSEngineDBFacade::isIngestionStatusFailed(ingestionStatusDependency))
							{
								ingestionJobToBeManaged = false;

								break;
							}
							else if (dependOnSuccess == 0
								&& MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatusDependency))
							{
								ingestionJobToBeManaged = false;

								break;
							}
							// else if (dependOnSuccess == -1)
							//      It means OnComplete and we have to do it since it's a final state
						}
						else
						{
							ingestionJobToBeManaged = false;

							break;
						}
					}
				}
				else
				{
					_logger->info(__FILEREF__ + "Dependency for the IngestionJob"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
						+ ", dependOnSuccess: " + to_string(dependOnSuccess)
						+ ", status: " + "no row"
						);
				}
			}
			else
			{
				_logger->info(__FILEREF__ + "Dependency for the IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", dependOnIngestionJobKey: " + "null"
					);
			}
		}
                    
		if (!atLeastOneDependencyRowFound)
		{
			// this is not possible, even an ingestionJob without dependency has a row
			// (with dependOnIngestionJobKey NULL)

			_logger->error(__FILEREF__ + "No dependency Row for the IngestionJob"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", ingestionStatus: " + to_string(static_cast<int>(ingestionStatus))
				+ ", ingestionType: " + to_string(static_cast<int>(ingestionType))
			);
			ingestionJobToBeManaged = false;
		}

		return make_tuple(ingestionJobToBeManaged, dependOnIngestionJobKey,
			dependOnSuccess, ingestionStatusDependency);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }        
}

shared_ptr<MySQLConnection> MMSEngineDBFacade::beginIngestionJobs ()
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
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

    return conn;    
}

shared_ptr<MySQLConnection> MMSEngineDBFacade::endIngestionJobs (
    shared_ptr<MySQLConnection> conn, bool commit,
	int64_t ingestionRootKey, string processedMetadataContent)
{
    string      lastSQLCommand;

    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "endIngestionJobs"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
            + ", commit: " + to_string(commit)
        );

		if (ingestionRootKey != -1)
        {
			lastSQLCommand = 
				"update MMS_IngestionRoot set processedMetaDataContent = ? "
				"where ingestionRootKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processedMetadataContent);
            preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", processedMetadataContent: " + processedMetadataContent
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processedMetadataContent: " + processedMetadataContent
                        + ", ingestionRootKey: " + to_string(ingestionRootKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        if (commit)
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
        else
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
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

    return conn;    
}

int64_t MMSEngineDBFacade::addIngestionRoot (
        shared_ptr<MySQLConnection> conn,
    	int64_t workspaceKey, int64_t userKey, string rootType, string rootLabel,
		string metaDataContent
)
{
    int64_t     ingestionRootKey;
    
    string      lastSQLCommand;
    
    try
    {
        {
            {                
                lastSQLCommand = 
                    "insert into MMS_IngestionRoot (ingestionRootKey, workspaceKey, userKey, type, label, "
					"metaDataContent, ingestionDate, lastUpdate, status) "
					"values (                       NULL,             ?,            ?,       ?,    ?, "
					"?,               NOW(),         NOW(),      ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
                preparedStatement->setInt64(queryParameterIndex++, userKey);
                preparedStatement->setString(queryParameterIndex++, rootType);
                preparedStatement->setString(queryParameterIndex++, rootLabel);
                preparedStatement->setString(queryParameterIndex++, metaDataContent);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionRootStatus::NotCompleted));
 
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", userKey: " + to_string(userKey)
					+ ", rootType: " + rootType
					+ ", rootLabel: " + rootLabel
					+ ", metaDataContent: " + metaDataContent
					+ ", IngestionRootStatus::NotCompleted: " + MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionRootStatus::NotCompleted)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }

            ingestionRootKey = getLastInsertId(conn);           
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
    
    return ingestionRootKey;
}

int64_t MMSEngineDBFacade::addIngestionJob (
        shared_ptr<MySQLConnection> conn, int64_t workspaceKey,
    	int64_t ingestionRootKey, string label, string metadataContent,
        MMSEngineDBFacade::IngestionType ingestionType, 
		string processingStartingFrom,
        vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
        vector<int64_t> waitForGlobalIngestionJobKeys
)
{
    int64_t     ingestionJobKey;
    
    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "select c.isEnabled, c.workspaceType from MMS_Workspace c where c.workspaceKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
                int isEnabled = resultSet->getInt("isEnabled");
                int workspaceType = resultSet->getInt("workspaceType");
                
                if (isEnabled != 1)
                {
                    string errorMessage = __FILEREF__ + "Workspace is not enabled"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
                else if (workspaceType != static_cast<int>(WorkspaceType::IngestionAndDelivery) &&
                        workspaceType != static_cast<int>(WorkspaceType::EncodingOnly))
                {
                    string errorMessage = __FILEREF__ + "Workspace is not enabled to ingest content"
                        + ", workspaceKey: " + to_string(workspaceKey);
                        + ", workspaceType: " + to_string(static_cast<int>(workspaceType))
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
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

        IngestionStatus ingestionStatus;
        string errorMessage;
        {
            {
                lastSQLCommand = 
                    "insert into MMS_IngestionJob (ingestionJobKey, ingestionRootKey, label, "
					"metaDataContent, ingestionType, priority, "
					"processingStartingFrom, "
					"startProcessing, endProcessing, downloadingProgress, "
					"uploadingProgress, sourceBinaryTransferred, processorMMS, status, errorMessage) values ("
                                                  "NULL,            ?,                ?, "
					"?,               ?,             ?, "
					"convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone), "
					"NULL,            NULL,         NULL, "
					"NULL,              0,                       NULL,         ?,      NULL)";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
                preparedStatement->setString(queryParameterIndex++, label);
                preparedStatement->setString(queryParameterIndex++, metadataContent);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(ingestionType));
                preparedStatement->setInt(queryParameterIndex++, getIngestionTypePriority(ingestionType));
                preparedStatement->setString(queryParameterIndex++, processingStartingFrom);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(
					MMSEngineDBFacade::IngestionStatus::Start_TaskQueued));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", label: " + label
					+ ", metadataContent: " + metadataContent
					+ ", ingestionType: " + MMSEngineDBFacade::toString(ingestionType)
					+ ", processingStartingFrom: " + processingStartingFrom
					+ ", IngestionStatus::Start_TaskQueued: " + MMSEngineDBFacade::toString(MMSEngineDBFacade::IngestionStatus::Start_TaskQueued)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }

            ingestionJobKey = getLastInsertId(conn);
            
            {
				int orderNumber = 0;
				bool referenceOutputDependency = false;
                if (dependOnIngestionJobKeys.size() == 0)
                {
					addIngestionJobDependency (conn, ingestionJobKey, dependOnSuccess, -1, orderNumber,
							referenceOutputDependency);
					orderNumber++;
                }
                else
                {
                    for (int64_t dependOnIngestionJobKey: dependOnIngestionJobKeys)
                    {
						addIngestionJobDependency (conn, ingestionJobKey, dependOnSuccess,
								dependOnIngestionJobKey, orderNumber, referenceOutputDependency);

                        orderNumber++;
                    }
                }

				{
					int waitForDependOnSuccess = -1;	// OnComplete
                    for (int64_t dependOnIngestionJobKey: waitForGlobalIngestionJobKeys)
                    {
						addIngestionJobDependency (conn, ingestionJobKey, waitForDependOnSuccess,
								dependOnIngestionJobKey, orderNumber, referenceOutputDependency);

                        orderNumber++;
                    }
				}
            }
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
    
    return ingestionJobKey;
}

int MMSEngineDBFacade::getIngestionTypePriority(
	MMSEngineDBFacade::IngestionType ingestionType)
{
	// The priority is used when engine retrieves the ingestion jobs to be executed
	//
	// Really LiveProxy and LiveRecording, since they are drived by their Start time, have the precedence
	// on every other task (see getIngestionJobs method)
	//
	// Regarding the other Tasks, AddContent has the precedence
	//		Scenario: we have a LiveRecording and, at the end, all the chunks will be concatenated
	//			If we have a lot of Tasks, the AddContent of the Chunk could be done lated and,
	//			the concatenation of the LiveRecorder chunks, will not have all the chunks
	//
	if (ingestionType == IngestionType::LiveProxy)
		return 1;
	else if (ingestionType == IngestionType::LiveRecorder)
		return 5;
	else if (ingestionType == IngestionType::AddContent)
		return 10;
	else
		return 15;
}

void MMSEngineDBFacade::getIngestionJobsKeyByGlobalLabel (
	int64_t workspaceKey, string globalIngestionLabel,
	vector<int64_t>& ingestionJobsKey
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

		{
			lastSQLCommand = 
				"select ij.ingestionJobKey "
                "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ir.workspaceKey = ? and ij.label = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setString(queryParameterIndex++, globalIngestionLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", globalIngestionLabel: " + globalIngestionLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
				ingestionJobsKey.push_back(resultSet->getInt64("ingestionJobKey"));
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

void MMSEngineDBFacade::addIngestionJobDependency (
	shared_ptr<MySQLConnection> conn,
	int64_t ingestionJobKey,
	int dependOnSuccess,
	int64_t dependOnIngestionJobKey,
	int orderNumber,
	bool referenceOutputDependency
)
{
    string      lastSQLCommand;

    try
    {
		int localOrderNumber = 0;
		if (orderNumber == -1)
		{
			lastSQLCommand = 
				"select max(orderNumber) as maxOrderNumber from MMS_IngestionJobDependency "
				"where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->isNull("maxOrderNumber"))
					localOrderNumber = 0;
				else
					localOrderNumber = resultSet->getInt("maxOrderNumber") + 1;
            }
		}
		else
		{
			localOrderNumber = orderNumber;
		}

        {
			lastSQLCommand = 
				"insert into MMS_IngestionJobDependency (ingestionJobDependencyKey, "
				"ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, "
				"orderNumber, referenceOutputDependency) values ("
				"NULL, ?, ?, ?, ?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);
			if (dependOnIngestionJobKey == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
			else
				preparedStatement->setInt64(queryParameterIndex++, dependOnIngestionJobKey);
			preparedStatement->setInt(queryParameterIndex++, localOrderNumber);
			preparedStatement->setInt(queryParameterIndex++, referenceOutputDependency ? 1 : 0);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", dependOnSuccess: " + to_string(dependOnSuccess)
				+ ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
				+ ", localOrderNumber: " + to_string(localOrderNumber)
				+ ", referenceOutputDependency: " + to_string(referenceOutputDependency ? 1 : 0)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::updateIngestionJobMetadataContent (
	int64_t ingestionJobKey,
	string metadataContent
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

        updateIngestionJobMetadataContent (conn, ingestionJobKey, metadataContent);

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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
    catch(AlreadyLocked e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

void MMSEngineDBFacade::updateIngestionJobMetadataContent (
	shared_ptr<MySQLConnection> conn,
	int64_t ingestionJobKey,
	string metadataContent
)
{
    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set metadataContent = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, metadataContent);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", metadataContent: " + metadataContent
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", metadataContent: " + metadataContent
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", metadataContent: " + metadataContent
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::updateIngestionJobParentGroupOfTasks(
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        int64_t parentGroupOfTasksIngestionJobKey
)
{
    string      lastSQLCommand;
    
    try
    {
        {
            lastSQLCommand = 
                "update MMS_IngestionJob set parentGroupOfTasksIngestionJobKey = ? where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, parentGroupOfTasksIngestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", parentGroupOfTasksIngestionJobKey: " + to_string(parentGroupOfTasksIngestionJobKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", parentGroupOfTasksIngestionJobKey: " + to_string(parentGroupOfTasksIngestionJobKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", parentGroupOfTasksIngestionJobKey: " + to_string(parentGroupOfTasksIngestionJobKey)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}
 
void MMSEngineDBFacade::getGroupOfTasksChildrenStatus(
	int64_t groupOfTasksIngestionJobKey,
	vector<pair<int64_t, MMSEngineDBFacade::IngestionStatus>>& groupOfTasksChildrenStatus
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
		groupOfTasksChildrenStatus.clear();

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		{
			lastSQLCommand = 
				"select ingestionJobKey, status "
                "from MMS_IngestionJob "
				"where parentGroupOfTasksIngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, groupOfTasksIngestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", groupOfTasksIngestionJobKey: " + to_string(groupOfTasksIngestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(
						resultSet->getString("status"));

				pair<int64_t, MMSEngineDBFacade::IngestionStatus> groupOfTasksChildStatus =
					make_pair(ingestionJobKey, ingestionStatus);

                groupOfTasksChildrenStatus.push_back(groupOfTasksChildStatus);
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

void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
		/*
		int milliSecondsToSleepWaitingLock = 500;

		PersistenceLock persistenceLock(this,
			MMSEngineDBFacade::LockType::Ingestion,
			_maxSecondsToWaitUpdateIngestionJobLock,
			processorMMS, "UpdateIngestionJob",
			milliSecondsToSleepWaitingLock, _logger);
		*/

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        updateIngestionJob (conn, ingestionJobKey, newIngestionStatus,
            errorMessage, processorMMS);

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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
    catch(AlreadyLocked e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

void MMSEngineDBFacade::updateIngestionJob (
        shared_ptr<MySQLConnection> conn,
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    try
    {
        string errorMessageForSQL;
        if (errorMessage.length() >= 1024)
            errorMessageForSQL = errorMessage.substr(0, 1024);
        else
            errorMessageForSQL = errorMessage;
		{
			string strToBeReplaced = "FFMpeg";
			string strToReplace = "XXX";
			if (errorMessageForSQL.find(strToBeReplaced) != string::npos)
				errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced),
					strToBeReplaced.length(), strToReplace);
		}

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
			if (processorMMS != "noToBeUpdated")
				lastSQLCommand = 
					string("update MMS_IngestionJob set status = ?, ")
					+ (errorMessageForSQL == "" ? "" : "errorMessage = SUBSTR(CONCAT(?, '\n', IFNULL(errorMessage, '')), 1, 1024 * 20), ")
					+ "endProcessing = NOW(), processorMMS = ? "
					+ "where ingestionJobKey = ?";
			else
				lastSQLCommand = 
					string("update MMS_IngestionJob set status = ?, ")
					+ (errorMessageForSQL == "" ? "" : "errorMessage = SUBSTR(CONCAT(?, '\n', IFNULL(errorMessage, '')), 1, 1024 * 20), ")
					+ "endProcessing = NOW() "
					+ "where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(newIngestionStatus));
            if (errorMessageForSQL != "")
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
			if (processorMMS != "noToBeUpdated")
			{
				if (processorMMS == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, processorMMS);
			}
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
				+ ", errorMessageForSQL: " + errorMessageForSQL
				+ ", processorMMS: " + processorMMS
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

				// it is not so important to block the continuation of this method
				// Also the exception caused a crash of the process
                // throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
			if (processorMMS != "noToBeUpdated")
				lastSQLCommand = 
					string("update MMS_IngestionJob set status = ?, ")
					+ (errorMessageForSQL == "" ? "" : "errorMessage = SUBSTR(CONCAT(?, '\n', IFNULL(errorMessage, '')), 1, 1024 * 20), ")
					+ "processorMMS = ? "
					+ "where ingestionJobKey = ?";
			else
				lastSQLCommand = 
					string("update MMS_IngestionJob set status = ? ")
					+ (errorMessageForSQL == "" ? "" : ", errorMessage = SUBSTR(CONCAT(?, '\n', IFNULL(errorMessage, '')), 1, 1024 * 20) ")
					+ "where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (errorMessageForSQL != "")
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
			if (processorMMS != "noToBeUpdated")
			{
				if (processorMMS == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, processorMMS);
			}
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
				+ ", errorMessageForSQL: " + errorMessageForSQL
				+ ", processorMMS: " + processorMMS
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

				// it is not so important to block the continuation of this method
				// Also the exception caused a crash of the process
                // throw runtime_error(errorMessage);                    
            }
        }
            
		bool updateIngestionRootStatus = true;
		manageIngestionJobStatusUpdate (ingestionJobKey, newIngestionStatus, updateIngestionRootStatus,
			conn);

        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", processorMMS: " + processorMMS
		);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::appendIngestionJobErrorMessage (
	int64_t ingestionJobKey,
	string errorMessage
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

        string errorMessageForSQL;
        if (errorMessage.length() >= 1024)
            errorMessageForSQL = errorMessage.substr(0, 1024);
        else
            errorMessageForSQL = errorMessage;
		{
			string strToBeReplaced = "FFMpeg";
			string strToReplace = "XXX";
			if (errorMessageForSQL.find(strToBeReplaced) != string::npos)
				errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced),
					strToBeReplaced.length(), strToReplace);
		}

		if (errorMessageForSQL != "")
        {
			lastSQLCommand = 
				"update MMS_IngestionJob "
				"set errorMessage = SUBSTR(CONCAT(?, '\n', IFNULL(errorMessage, '')), 1, 1024 * 20) "
				"where ingestionJobKey = ? and status not like 'End_%'";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", errorMessageForSQL: " + errorMessageForSQL
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

				// throw runtime_exception(errorMessage);
            }
			else
			{
				_logger->info(__FILEREF__ + "IngestionJob updated successful"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

void MMSEngineDBFacade::manageIngestionJobStatusUpdate (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
		bool updateIngestionRootStatus,
        shared_ptr<MySQLConnection> conn
)
{
    string      lastSQLCommand;
    

    try
    {
        _logger->info(__FILEREF__ + "manageIngestionJobStatusUpdate"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", newIngestionStatus: " + toString(newIngestionStatus)
            + ", updateIngestionRootStatus: " + to_string(updateIngestionRootStatus)
        );

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            int dependOnSuccess;

            if (MMSEngineDBFacade::isIngestionStatusSuccess(newIngestionStatus))
            {
                // set to NotToBeExecuted the tasks depending on this task on failure

                dependOnSuccess = 0;
            }
            else
            {
                // set to NotToBeExecuted the tasks depending on this task on success

                dependOnSuccess = 1;
            }

            // found all the ingestionJobKeys to be set as End_NotToBeExecuted
            string hierarchicalIngestionJobKeysDependencies;
            string ingestionJobKeysToFindDependencies = to_string(ingestionJobKey);
            {
                // all dependencies from ingestionJobKey (using dependOnSuccess) and
                // all dependencies from the keys dependent from ingestionJobKey (without dependOnSuccess check)
                // and so on recursively
                int maxHierarchicalLevelsManaged = 50;
                for (int hierarchicalLevelIndex = 0; hierarchicalLevelIndex < maxHierarchicalLevelsManaged; hierarchicalLevelIndex++)
                {
					// in the first select we have to found the dependencies according dependOnSuccess,
					// so in case the parent task was successful, we have to set End_NotToBeExecuted for
					// all the tasks depending on it in case of failure
					// Starting from the second select, we have to set End_NotToBeExecuted for all the other
					// tasks, because we have that the father is End_NotToBeExecuted, so all the subtree
					// has to be set as End_NotToBeExecuted
					// _logger->error(__FILEREF__ + "select"
					// 	+ ", ingestionJobKeysToFindDependencies: " + ingestionJobKeysToFindDependencies
					// );
					// 2019-09-23: we have to exclude the IngestionJobKey of the GroupOfTasks. This is because:
					// - GroupOfTasks cannot tbe set to End_NotToBeExecuted, it has always to be executed

					// 2019-10-05: referenceOutputDependency==1 specifies that this dependency comes
					//	from a ReferenceOutput, it means his parent is a GroupOfTasks.
					//	This dependency has not to be considered here because, the only dependency
					//	to be used is the one with the GroupOfTasks
					if (hierarchicalLevelIndex == 0)
					{
						lastSQLCommand = 
                            "select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey in ("
                            + ingestionJobKeysToFindDependencies
                            + ") and ijd.dependOnSuccess = ?";
					}
					else
					{
						lastSQLCommand = 
                            "select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey in ("
                            + ingestionJobKeysToFindDependencies
                            + ")";
					}
                    shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
					if (hierarchicalLevelIndex == 0)
						preparedStatement->setInt(queryParameterIndex++, dependOnSuccess);

					// _logger->info(__FILEREF__ + "select"
					// 	+ ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
					// 	+ ", ingestionJobKeysToFindDependencies: " + ingestionJobKeysToFindDependencies
					// 	+ ", dependOnSuccess (important in case of levelIndex 0): " + to_string(dependOnSuccess)
					// );

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", dependOnSuccess: " + to_string(dependOnSuccess)
						+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    bool dependenciesFound = false;
                    ingestionJobKeysToFindDependencies = "";
                    while (resultSet->next())
                    {
                        dependenciesFound = true;

                        if (hierarchicalIngestionJobKeysDependencies == "")
                            hierarchicalIngestionJobKeysDependencies = to_string(resultSet->getInt64("ingestionJobKey"));
                        else
                            hierarchicalIngestionJobKeysDependencies += (", " + to_string(resultSet->getInt64("ingestionJobKey")));
                        
                        if (ingestionJobKeysToFindDependencies == "")
                            ingestionJobKeysToFindDependencies = to_string(resultSet->getInt64("ingestionJobKey"));
                        else
                            ingestionJobKeysToFindDependencies += (", " + to_string(resultSet->getInt64("ingestionJobKey")));
                    }
                    
					// _logger->info(__FILEREF__ + "select result"
					// 	+ ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
					// 	+ ", hierarchicalIngestionJobKeysDependencies: " + hierarchicalIngestionJobKeysDependencies
					// );

                    if (!dependenciesFound)
                    {
                        _logger->info(__FILEREF__ + "Finished to find dependencies"
                            + ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
                            + ", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        );

                        break;
                    }
                    else if (dependenciesFound && hierarchicalLevelIndex + 1 >= maxHierarchicalLevelsManaged)
                    {
                        string errorMessage = __FILEREF__ + "after maxHierarchicalLevelsManaged we still found dependencies, so maxHierarchicalLevelsManaged has to be increased"
                            + ", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged)
                            + ", ingestionJobKey: " + to_string(ingestionJobKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->warn(errorMessage);
                    }            
                }
            }
            
            if (hierarchicalIngestionJobKeysDependencies != "")
            {
				_logger->info(__FILEREF__ + "manageIngestionJobStatusUpdate. update"
					+ ", status: " + "End_NotToBeExecuted"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", hierarchicalIngestionJobKeysDependencies: " + hierarchicalIngestionJobKeysDependencies
				);

                lastSQLCommand = 
                    "update MMS_IngestionJob set status = ?, "
					"startProcessing = IF(startProcessing IS NULL, NOW(), startProcessing), "
					"endProcessing = NOW() where ingestionJobKey in (" + hierarchicalIngestionJobKeysDependencies + ")";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++,
						MMSEngineDBFacade::toString(IngestionStatus::End_NotToBeExecuted));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", IngestionStatus::End_NotToBeExecuted: " + MMSEngineDBFacade::toString(IngestionStatus::End_NotToBeExecuted)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
            }
        }

        if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
        {
            int64_t ingestionRootKey;
            IngestionRootStatus currentIngestionRootStatus;

            {
                lastSQLCommand = 
                    "select ir.ingestionRootKey, ir.status "
                    "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                    "where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (resultSet->next())
                {
                    ingestionRootKey = resultSet->getInt64("ingestionRootKey");
                    currentIngestionRootStatus = MMSEngineDBFacade::toIngestionRootStatus(
						resultSet->getString("Status"));                
                }
                else
                {
                    string errorMessage = __FILEREF__ + "IngestionJob is not found"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }

            int successStatesCount = 0;
            int failureStatesCount = 0;
            int intermediateStatesCount = 0;

            {
                lastSQLCommand = 
                    "select status from MMS_IngestionJob where ingestionRootKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                while (resultSet->next())
                {                
                    IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(
						resultSet->getString("Status"));

                    if (!MMSEngineDBFacade::isIngestionStatusFinalState(ingestionStatus))
                        intermediateStatesCount++;
                    else
                    {
                        if (MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatus))
                            successStatesCount++;
                        else
                            failureStatesCount++;
                    }
                }
            }

            _logger->info(__FILEREF__ + "Job status"
                + ", ingestionRootKey: " + to_string(ingestionRootKey)
                + ", intermediateStatesCount: " + to_string(intermediateStatesCount)
                + ", successStatesCount: " + to_string(successStatesCount)
                + ", failureStatesCount: " + to_string(failureStatesCount)
            );

            IngestionRootStatus newIngestionRootStatus;

            if (intermediateStatesCount > 0)
                newIngestionRootStatus = IngestionRootStatus::NotCompleted;
            else
            {
                if (failureStatesCount > 0)
                    newIngestionRootStatus = IngestionRootStatus::CompletedWithFailures;
                else
                    newIngestionRootStatus = IngestionRootStatus::CompletedSuccessful;
            }

            if (newIngestionRootStatus != currentIngestionRootStatus)            
            {
                lastSQLCommand = 
                    "update MMS_IngestionRoot set lastUpdate = NOW(), status = ? where ingestionRootKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newIngestionRootStatus));
                preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", newIngestionRootStatus: " + MMSEngineDBFacade::toString(newIngestionRootStatus)
					+ ", ingestionRootKey: " + to_string(ingestionRootKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", ingestionRootKey: " + to_string(ingestionRootKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::setNotToBeExecutedStartingFromBecauseChunkNotSelected (
		int64_t ingestionJobKey, string processorMMS)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
		/*
		int milliSecondsToSleepWaitingLock = 500;

		PersistenceLock persistenceLock(this,
			MMSEngineDBFacade::LockType::Ingestion,
			_maxSecondsToWaitSetNotToBeExecutedLock,
			processorMMS, "setNotToBeExecutedStartingFrom",
			milliSecondsToSleepWaitingLock, _logger);
		*/

        chrono::system_clock::time_point startPoint = chrono::system_clock::now();

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
        
		_logger->info(__FILEREF__ + "Update IngestionJob"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", IngestionStatus: " + toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected)
			+ ", errorMessage: " + ""
			+ ", processorMMS: " + ""
		);
		updateIngestionJob (conn, ingestionJobKey,
			MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected,
			"",	// errorMessage,
			"" // processorMMS
		);

		// to set 'not to be executed' to the tasks depending from ingestionJobKey,, we will call manageIngestionJobStatusUpdate
		// simulating the IngestionJob failed, that cause the setting to 'not to be executed'
		// for the onSuccess tasks                                        

		bool updateIngestionRootStatus = false;
		manageIngestionJobStatusUpdate (ingestionJobKey, IngestionStatus::End_IngestionFailure,
				updateIngestionRootStatus, conn);

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
		_logger->info(__FILEREF__ + "setNotToBeExecutedStartingFrom statistics"
			+ ", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
        );
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage)
{
    
    bool        toBeCancelled = false;
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
                "update MMS_IngestionJob set downloadingProgress = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, downloadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", downloadingPercentage: " + to_string(downloadingPercentage)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", downloadingPercentage: " + to_string(downloadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand = 
                "select status from MMS_IngestionJob where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));
                
                if (ingestionStatus == IngestionStatus::End_CanceledByUser)
                    toBeCancelled = true;
            }
            else
            {
                string errorMessage = __FILEREF__ + "IngestionJob is not found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
    
    return toBeCancelled;
}

bool MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage)
{
    
    bool        toBeCancelled = false;
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
                "update MMS_IngestionJob set uploadingProgress = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, uploadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", uploadingPercentage: " + to_string(uploadingPercentage)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", uploadingPercentage: " + to_string(uploadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand = 
                "select status from MMS_IngestionJob where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));
                
                if (ingestionStatus == IngestionStatus::End_CanceledByUser)
                    toBeCancelled = true;
            }
            else
            {
                string errorMessage = __FILEREF__ + "IngestionJob is not found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

    return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred)
{
    
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
                "update MMS_IngestionJob set sourceBinaryTransferred = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, sourceBinaryTransferred ? 1 : 0);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred ? 1 : 0)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

string MMSEngineDBFacade::getIngestionRootMetaDataContent (
	shared_ptr<Workspace> workspace, int64_t ingestionRootKey, bool processedMetadata
)
{    
    string      lastSQLCommand;
	string		metaDataContent;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			string columnName;
			if (processedMetadata)
				columnName = "processedMetaDataContent";
			else
				columnName = "metaDataContent";

			lastSQLCommand = 
				string("select ") + columnName + " from MMS_IngestionRoot "
				+ "where workspaceKey = ? and ingestionRootKey = ?";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                metaDataContent = static_cast<string>(resultSet->getString(columnName));
            }
			else
            {
                string errorMessage = __FILEREF__ + "ingestion root not found"
                        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", ingestionRootKey: " + to_string(ingestionRootKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
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
    
    return metaDataContent;
}

tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, string, string>
	MMSEngineDBFacade::getIngestionJobDetails(
		// 2021-02-20: workspaceKey is used just to be sure the ingestionJobKey
		//	will belong to the specified workspaceKey. We do that because the updateIngestionJob API
		//	calls this method, to be sure an end user can do an update of any IngestionJob (also
		//	belonging to another workspace)
		int64_t workspaceKey,
		int64_t ingestionJobKey
)
{
    string      lastSQLCommand;
	string		label;
	IngestionType	ingestionType;
	IngestionStatus ingestionStatus;
	string		errorMessage;
	string		metaDataContent;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t ingestionRootKey;
        {
            lastSQLCommand = 
                string("select ingestionRootKey, label, ingestionType, status, "
						"metaDataContent, errorMessage "
						"from MMS_IngestionJob where ingestionJobKey = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                ingestionRootKey = resultSet->getInt64("ingestionRootKey");
                label = static_cast<string>(resultSet->getString("label"));
				ingestionType = toIngestionType(resultSet->getString("ingestionType"));
				ingestionStatus = toIngestionStatus(resultSet->getString("status"));
                metaDataContent = static_cast<string>(resultSet->getString("metaDataContent"));
                errorMessage = static_cast<string>(resultSet->getString("errorMessage"));
            }
			else
            {
                string errorMessage = __FILEREF__ + "ingestion job not found"
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
		int64_t localWorkspaceKey;
        {
            lastSQLCommand = 
                string("select workspaceKey "
						"from MMS_IngestionRoot where ingestionRootKey = ?");

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localWorkspaceKey = resultSet->getInt64("workspaceKey");
            }
			else
            {
                string errorMessage = __FILEREF__ + "ingestion root not found"
                        + ", ingestionRootKey: " + to_string(ingestionRootKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

		if (workspaceKey != localWorkspaceKey)
		{
			string errorMessage = __FILEREF__ + "ingestion job was found but it is not belonging to your workspace"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", localWorkspaceKey: " + to_string(localWorkspaceKey)
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
    
    return make_tuple(label, ingestionType, ingestionStatus,
		metaDataContent, errorMessage);
}

void MMSEngineDBFacade::addIngestionJobOutput(
	int64_t ingestionJobKey,
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	int64_t liveRecordingIngestionJobKey
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

		addIngestionJobOutput(conn, ingestionJobKey, mediaItemKey, physicalPathKey,
				liveRecordingIngestionJobKey);

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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
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

void MMSEngineDBFacade::addIngestionJobOutput(
	shared_ptr<MySQLConnection> conn,
	int64_t ingestionJobKey,
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	int64_t liveRecordingIngestionJobKey
)
{
	string      lastSQLCommand;

	try
	{
		{
			lastSQLCommand = 
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
				"?, ?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			_logger->info(__FILEREF__ + "insert into MMS_IngestionJobOutput"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
			);                            
		}

		if (liveRecordingIngestionJobKey != -1)
		{
			lastSQLCommand = 
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey) values ("
				"?, ?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, liveRecordingIngestionJobKey);
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", liveRecordingIngestionJobKey: " + to_string(liveRecordingIngestionJobKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			_logger->info(__FILEREF__ + "insert into MMS_IngestionJobOutput"
				+ ", liveRecordingIngestionJobKey: " + to_string(liveRecordingIngestionJobKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", physicalPathKey: " + to_string(physicalPathKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
			);                            
		}
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }
}

long MMSEngineDBFacade::getIngestionJobOutputsCount(
	int64_t ingestionJobKey
)
{
    string      lastSQLCommand;
    long		ingestionJobOutputsCount = -1;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		{
            lastSQLCommand = "select count(*) from MMS_IngestionJobOutput where ingestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage ("select count(*) failed");

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			ingestionJobOutputsCount = resultSet->getInt64(1);
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
    
    return ingestionJobOutputsCount;
}

Json::Value MMSEngineDBFacade::getIngestionRootsStatus (
        shared_ptr<Workspace> workspace,
		int64_t ingestionRootKey, int64_t mediaItemKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        string label, string status, bool asc, bool ingestionJobOutputs
)
{
    string      lastSQLCommand;
    Json::Value statusListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (ingestionRootKey != -1)
            {
                field = "ingestionRootKey";
                requestParametersRoot[field] = ingestionRootKey;
            }

            if (mediaItemKey != -1)
            {
                field = "mediaItemKey";
                requestParametersRoot[field] = mediaItemKey;
            }

            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            field = "label";
            requestParametersRoot[field] = label;
            
            field = "ingestionJobOutputs";
            requestParametersRoot[field] = ingestionJobOutputs;

            field = "status";
            requestParametersRoot[field] = status;

            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }

		vector<int64_t> ingestionTookKeysByMediaItemKey;
		if (mediaItemKey != -1)
		{
            lastSQLCommand = "select distinct ir.ingestionRootKey "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_IngestionJobOutput ijo "
					"where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ijo.ingestionJobKey "
					"and ir.workspaceKey = ? and ijo.mediaItemKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
				ingestionTookKeysByMediaItemKey.push_back(resultSet->getInt64("ingestionRootKey"));
            }
        }

        string sqlWhere = string ("where workspaceKey = ? ");
        if (ingestionRootKey != -1 || ingestionTookKeysByMediaItemKey.size() > 0)
		{
			string ingestionRootKeysWhere;
			int ingestionRookKeysNumber = 0;

			if (ingestionRootKey != -1)
				ingestionRookKeysNumber++;
			ingestionRookKeysNumber += ingestionTookKeysByMediaItemKey.size();

			for (int ingestionRookKeyIndex = 0; ingestionRookKeyIndex < ingestionRookKeysNumber; ingestionRookKeyIndex++)
			{
				if (ingestionRootKeysWhere == "")
					ingestionRootKeysWhere = "?";
				else
					ingestionRootKeysWhere += ", ?";
			}

			if (ingestionRookKeysNumber > 1)
				sqlWhere += ("and ingestionRootKey in (" + ingestionRootKeysWhere + ") ");
			else
				sqlWhere += ("and ingestionRootKey = " + ingestionRootKeysWhere + " ");
		}
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (label != "")
            sqlWhere += ("and LOWER(label) like LOWER(?) ");		// LOWER was used because the column is using utf8_bin that is case sensitive
        {
            string allStatus = "all";
            // compare case insensitive
            if (!(
                    status.length() != allStatus.length() ? false : 
                        equal(status.begin(), status.end(), allStatus.begin(),
                            [](int c1, int c2){ return toupper(c1) == toupper(c2); })
                ))
            {
                sqlWhere += ("and status = '" + status + "' ");
            }
        }
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
			if (ingestionRootKey != -1 || ingestionTookKeysByMediaItemKey.size() > 0)
			{
				if (ingestionRootKey != -1)
					preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
				for (int ingestionRookKeyIndex = 0;
						ingestionRookKeyIndex < ingestionTookKeysByMediaItemKey.size();
						ingestionRookKeyIndex++)
					preparedStatement->setInt64(queryParameterIndex++,
							ingestionTookKeysByMediaItemKey[ingestionRookKeyIndex]);
			}
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", label: " + "%" + label + "%"
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage (__FILEREF__ + "select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Json::Value workflowsRoot(Json::arrayValue);
        {
            lastSQLCommand = 
                string("select ingestionRootKey, userKey, label, status, "
                    "DATE_FORMAT(convert_tz(ingestionDate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as ingestionDate, "
                    "DATE_FORMAT(convert_tz(lastUpdate, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as lastUpdate "
                    "from MMS_IngestionRoot ")
                    + sqlWhere
                    + "order by ingestionDate"  + (asc ? " asc " : " desc ")
                    + "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
			if (ingestionRootKey != -1 || ingestionTookKeysByMediaItemKey.size() > 0)
			{
				if (ingestionRootKey != -1)
					preparedStatement->setInt64(queryParameterIndex++, ingestionRootKey);
				for (int ingestionRookKeyIndex = 0;
						ingestionRookKeyIndex < ingestionTookKeysByMediaItemKey.size();
						ingestionRookKeyIndex++)
					preparedStatement->setInt64(queryParameterIndex++,
							ingestionTookKeysByMediaItemKey[ingestionRookKeyIndex]);
			}
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionRootKey: " + to_string(ingestionRootKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", label: " + "%" + label + "%"
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value workflowRoot;
                
                int64_t currentIngestionRootKey = resultSet->getInt64("ingestionRootKey");
                field = "ingestionRootKey";
                workflowRoot[field] = currentIngestionRootKey;

				int64_t userKey = resultSet->getInt64("userKey");
                field = "userKey";
                workflowRoot[field] = userKey;

				{
					pair<string, string> userDetails = getUserDetails(userKey);

					string userName;

					tie(ignore, userName) = userDetails;

					field = "userName";
					workflowRoot[field] = userName;
				}

                field = "label";
                workflowRoot[field] = static_cast<string>(resultSet->getString("label"));

                field = "status";
                workflowRoot[field] = static_cast<string>(resultSet->getString("status"));

                field = "ingestionDate";
                workflowRoot[field] = static_cast<string>(resultSet->getString("ingestionDate"));

                field = "lastUpdate";
                workflowRoot[field] = static_cast<string>(resultSet->getString("lastUpdate"));

                Json::Value ingestionJobsRoot(Json::arrayValue);
                {            
                    lastSQLCommand = 
                        "select ingestionJobKey, label, ingestionType, metaDataContent, processorMMS, "
                        "DATE_FORMAT(convert_tz(processingStartingFrom, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as processingStartingFrom, "
                        "DATE_FORMAT(convert_tz(startProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startProcessing, "
                        "DATE_FORMAT(convert_tz(endProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endProcessing, "
                        "IF(startProcessing is null, NOW(), startProcessing) as newStartProcessing, "
                        "IF(endProcessing is null, NOW(), endProcessing) as newEndProcessing, downloadingProgress, uploadingProgress, "
                        "status, errorMessage from MMS_IngestionJob where ingestionRootKey = ? "
                        "order by ingestionJobKey asc";
                        // "order by newStartProcessing asc, newEndProcessing asc";
                    shared_ptr<sql::PreparedStatement> preparedStatementIngestionJob (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementIngestionJob->setInt64(queryParameterIndex++, currentIngestionRootKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> resultSetIngestionJob (preparedStatementIngestionJob->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", currentIngestionRootKey: " + to_string(currentIngestionRootKey)
						+ ", resultSetIngestionJob->rowsCount: " + to_string(resultSetIngestionJob->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    while (resultSetIngestionJob->next())
                    {
                        Json::Value ingestionJobRoot = getIngestionJobRoot(
                                workspace, resultSetIngestionJob, currentIngestionRootKey,
								ingestionJobOutputs, conn);

                        ingestionJobsRoot.append(ingestionJobRoot);
                    }
                }

                field = "ingestionJobs";
                workflowRoot[field] = ingestionJobsRoot;

                workflowsRoot.append(workflowRoot);
            }
        }
        
        field = "workflows";
        responseRoot[field] = workflowsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

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
    
    return statusListRoot;
}

Json::Value MMSEngineDBFacade::getIngestionJobsStatus (
        shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
        int start, int rows, string label,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
		string ingestionType, string jsonParametersCondition,
        bool asc, string status,
		bool ingestionJobOutputs	// added because output could be thousands of entries
)
{
    string      lastSQLCommand;
    Json::Value statusListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        string field;
        
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (ingestionJobKey != -1)
            {
                field = "ingestionJobKey";
                requestParametersRoot[field] = ingestionJobKey;
            }
            
            field = "label";
            requestParametersRoot[field] = label;
            
            field = "status";
            requestParametersRoot[field] = status;

            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (ingestionType != "")
            {
                field = "ingestionType";
                requestParametersRoot[field] = ingestionType;
            }

            if (jsonParametersCondition != "")
            {
                field = "jsonParametersCondition";
                requestParametersRoot[field] = jsonParametersCondition;
            }

            field = "ingestionJobOutputs";
            requestParametersRoot[field] = ingestionJobOutputs;

            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where ir.ingestionRootKey = ij.ingestionRootKey ");
        sqlWhere += ("and ir.workspaceKey = ? ");
        if (ingestionJobKey != -1)
            sqlWhere += ("and ij.ingestionJobKey = ? ");
        if (label != "")
			sqlWhere += ("and LOWER(ij.label) like LOWER(?) ");		// LOWER was used because the column is using utf8_bin that is case sensitive
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (ingestionType != "")
            sqlWhere += ("and ij.ingestionType = ? ");
        if (jsonParametersCondition != "")
            sqlWhere += ("and " + jsonParametersCondition + " ");
        if (status == "completed")
            sqlWhere += ("and ij.status like 'End_%' ");
        else if (status == "notCompleted")
            sqlWhere += ("and ij.status not like 'End_%' ");
            
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (ingestionType != "")
                preparedStatement->setString(queryParameterIndex++, ingestionType);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", label: " + "%" + label + "%"
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", ingestionType: " + ingestionType
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                field = "numFound";
                responseRoot[field] = resultSet->getInt64(1);
            }
            else
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Json::Value ingestionJobsRoot(Json::arrayValue);
        {            
            lastSQLCommand = 
                "select ir.ingestionRootKey, ij.ingestionJobKey, ij.label, ij.ingestionType, ij.metaDataContent, ij.processorMMS, "
                "DATE_FORMAT(convert_tz(ij.processingStartingFrom, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as processingStartingFrom, "
                "DATE_FORMAT(convert_tz(ij.startProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as startProcessing, "
                "DATE_FORMAT(convert_tz(ij.endProcessing, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as endProcessing, "
                "IF(ij.startProcessing is null, NOW(), ij.startProcessing) as newStartProcessing, "
                "IF(ij.endProcessing is null, NOW(), ij.endProcessing) as newEndProcessing, ij.downloadingProgress, ij.uploadingProgress, "
                "ij.status, ij.errorMessage from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                + sqlWhere
                + "order by newStartProcessing" + (asc ? " asc" : " desc") + ", newEndProcessing " + 
                + "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (ingestionJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
			if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
			if (ingestionType != "")
                preparedStatement->setString(queryParameterIndex++, ingestionType);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", label: " + "%" + label + "%"
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", ingestionType: " + ingestionType
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value ingestionJobRoot = getIngestionJobRoot(
                        workspace, resultSet, resultSet->getInt64("ingestionRootKey"),
						ingestionJobOutputs, conn);

                ingestionJobsRoot.append(ingestionJobRoot);
            }
        }
        
        field = "ingestionJobs";
        responseRoot[field] = ingestionJobsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

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
    
    return statusListRoot;
}

Json::Value MMSEngineDBFacade::getIngestionJobRoot(
        shared_ptr<Workspace> workspace,
        shared_ptr<sql::ResultSet> resultSet,
        int64_t ingestionRootKey,
		bool ingestionJobOutputs,	// added because output could be thousands of entries
        shared_ptr<MySQLConnection> conn
)
{
    Json::Value ingestionJobRoot;
    string      lastSQLCommand;


    try
    {
        int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");

        string field = "ingestionType";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("ingestionType"));
        IngestionType ingestionType = toIngestionType(resultSet->getString("ingestionType"));

        field = "ingestionJobKey";
        ingestionJobRoot[field] = ingestionJobKey;

        field = "status";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("status"));
        IngestionStatus ingestionStatus = toIngestionStatus(resultSet->getString("status"));

        field = "metaDataContent";
        ingestionJobRoot[field] = static_cast<string>(resultSet->getString("metaDataContent"));

        field = "processorMMS";
        if (resultSet->isNull("processorMMS"))
            ingestionJobRoot[field] = Json::nullValue;
        else
			ingestionJobRoot[field] = static_cast<string>(resultSet->getString("processorMMS"));

        field = "label";
        if (resultSet->isNull("label"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("label"));

		{
			tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> 
				ingestionJobToBeManagedInfo = isIngestionJobToBeManaged(
				ingestionJobKey, workspace->_workspaceKey, ingestionStatus,
				ingestionType, conn);

			bool ingestionJobToBeManaged;
			int64_t dependOnIngestionJobKey;
			int dependOnSuccess;
			IngestionStatus ingestionStatusDependency;

			tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency)
				= ingestionJobToBeManagedInfo;

			if (dependOnIngestionJobKey != -1)
			{
				field = "dependOnIngestionJobKey";
				ingestionJobRoot[field] = dependOnIngestionJobKey;

				field = "dependOnSuccess";
				ingestionJobRoot[field] = dependOnSuccess;

				field = "dependencyIngestionStatus";
				ingestionJobRoot[field] = toString(ingestionStatusDependency);
			}
		}

        Json::Value mediaItemsRoot(Json::arrayValue);
		if(ingestionJobOutputs)
        {
            lastSQLCommand = 
                "select mediaItemKey, physicalPathKey from MMS_IngestionJobOutput "
				"where ingestionJobKey = ? order by mediaItemKey";

            shared_ptr<sql::PreparedStatement> preparedStatementMediaItems (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementMediaItems->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSetMediaItems (preparedStatementMediaItems->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSetMediaItems->rowsCount: " + to_string(resultSetMediaItems->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSetMediaItems->next())
            {
                Json::Value mediaItemRoot;

                field = "mediaItemKey";
                int64_t mediaItemKey = resultSetMediaItems->getInt64("mediaItemKey");
                mediaItemRoot[field] = mediaItemKey;

                field = "physicalPathKey";
                int64_t physicalPathKey = resultSetMediaItems->getInt64("physicalPathKey");
                mediaItemRoot[field] = physicalPathKey;

                mediaItemsRoot.append(mediaItemRoot);
            }
        }
        field = "mediaItems";
        ingestionJobRoot[field] = mediaItemsRoot;

        field = "processingStartingFrom";
		ingestionJobRoot[field] = static_cast<string>(resultSet->getString("processingStartingFrom"));

        field = "startProcessing";
        if (resultSet->isNull("startProcessing"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("startProcessing"));

        field = "endProcessing";
        if (resultSet->isNull("endProcessing"))
            ingestionJobRoot[field] = Json::nullValue;
        else
            ingestionJobRoot[field] = static_cast<string>(resultSet->getString("endProcessing"));

        // if (ingestionType == IngestionType::AddContent)
        {
            field = "downloadingProgress";
            if (resultSet->isNull("downloadingProgress"))
                ingestionJobRoot[field] = Json::nullValue;
            else
                ingestionJobRoot[field] = resultSet->getInt64("downloadingProgress");
        }

        // if (ingestionType == IngestionType::AddContent)
        {
            field = "uploadingProgress";
            if (resultSet->isNull("uploadingProgress"))
                ingestionJobRoot[field] = Json::nullValue;
            else
                ingestionJobRoot[field] = resultSet->getInt64("uploadingProgress");
        }

        field = "ingestionRootKey";
        ingestionJobRoot[field] = ingestionRootKey;

        field = "errorMessage";
        if (resultSet->isNull("errorMessage"))
            ingestionJobRoot[field] = Json::nullValue;
        else
		{
			int maxErrorMessageLength = 2000;

			string errorMessage = resultSet->getString("errorMessage");
			if (errorMessage.size() > maxErrorMessageLength)
			{
				ingestionJobRoot[field] = errorMessage.substr(0, maxErrorMessageLength);

				field = "errorMessageTruncated";
				ingestionJobRoot[field] = true;
			}
			else
			{
				ingestionJobRoot[field] = errorMessage;

				field = "errorMessageTruncated";
				ingestionJobRoot[field] = true;
			}
		}

        if (ingestionType == IngestionType::Encode 
                || ingestionType == IngestionType::OverlayImageOnVideo
                || ingestionType == IngestionType::OverlayTextOnVideo
                || ingestionType == IngestionType::PeriodicalFrames
                || ingestionType == IngestionType::IFrames
                || ingestionType == IngestionType::MotionJPEGByPeriodicalFrames
                || ingestionType == IngestionType::MotionJPEGByIFrames
                || ingestionType == IngestionType::Slideshow
                || ingestionType == IngestionType::FaceRecognition
                || ingestionType == IngestionType::FaceIdentification
                || ingestionType == IngestionType::LiveRecorder
                || ingestionType == IngestionType::VideoSpeed
                || ingestionType == IngestionType::PictureInPicture
                || ingestionType == IngestionType::IntroOutroOverlay
                || ingestionType == IngestionType::LiveProxy
                || ingestionType == IngestionType::VODProxy
                || ingestionType == IngestionType::LiveGrid
                || ingestionType == IngestionType::AwaitingTheBeginning
				// IngestionType::Cut has the EncodingJob only in case of FrameAccurate
                || ingestionType == IngestionType::Cut
                )
        {
			// in case of LiveRecorder and HighAvailability true, we will have 2 encodingJobs, one for the main and one for the backup,
			//	we will take the main one. In case HighAvailability is false, we still have the main field set to true
			// 2021-05-12: HighAvailability true is not used anymore
			/*
			if (ingestionType == IngestionType::LiveRecorder)
				lastSQLCommand = 
					"select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
					"DATE_FORMAT(convert_tz(encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
					"DATE_FORMAT(convert_tz(encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
					"processorMMS, encoderKey, encodingPid, failuresNumber from MMS_EncodingJob where ingestionJobKey = ? "
					"and JSON_EXTRACT(parameters, '$.main') = true "
					;
			else
			*/
				lastSQLCommand = 
					"select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
					"DATE_FORMAT(convert_tz(encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
					"DATE_FORMAT(convert_tz(encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
					"processorMMS, encoderKey, encodingPid, failuresNumber from MMS_EncodingJob "
					"where ingestionJobKey = ? "
					;

            shared_ptr<sql::PreparedStatement> preparedStatementEncodingJob (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncodingJob->setInt64(queryParameterIndex++, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSetEncodingJob (
				preparedStatementEncodingJob->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", resultSetEncodingJob->rowsCount: " + to_string(resultSetEncodingJob->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSetEncodingJob->next())
            {
                Json::Value encodingJobRoot;

                int64_t encodingJobKey = resultSetEncodingJob->getInt64("encodingJobKey");
                
				field = "ownedByCurrentWorkspace";
				encodingJobRoot[field] = true;

                field = "encodingJobKey";
                encodingJobRoot[field] = encodingJobKey;

                field = "ingestionJobKey";
                encodingJobRoot[field] = ingestionJobKey;

                field = "type";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("type"));

                {
                    string parameters = resultSetEncodingJob->getString("parameters");

                    Json::Value parametersRoot;
                    if (parameters != "")
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(parameters.c_str(),
                                parameters.c_str() + parameters.size(), 
                                &parametersRoot, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                    + ", encodingJobKey: " + to_string(encodingJobKey)
                                    + ", errors: " + errors
                                    + ", parameters: " + parameters
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }

                    field = "parameters";
                    encodingJobRoot[field] = parametersRoot;
                }

                field = "status";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("status"));
                EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSetEncodingJob->getString("status"));

                field = "encodingPriority";
                encodingJobRoot[field] = toString(static_cast<EncodingPriority>(resultSetEncodingJob->getInt("encodingPriority")));

                field = "encodingPriorityCode";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingPriority");

                field = "maxEncodingPriorityCode";
                encodingJobRoot[field] = workspace->_maxEncodingPriority;

                field = "progress";
                if (resultSetEncodingJob->isNull("encodingProgress"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingProgress");

                field = "start";
                if (encodingStatus == EncodingStatus::ToBeProcessed)
                    encodingJobRoot[field] = Json::nullValue;
                else
                {
                    if (resultSetEncodingJob->isNull("encodingJobStart"))
                        encodingJobRoot[field] = Json::nullValue;
                    else
                        encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobStart"));
                }

                field = "end";
                if (resultSetEncodingJob->isNull("encodingJobEnd"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("encodingJobEnd"));

                field = "processorMMS";
                if (resultSetEncodingJob->isNull("processorMMS"))
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("processorMMS"));

                field = "encoderKey";
				if (resultSetEncodingJob->isNull("encoderKey"))
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = resultSetEncodingJob->getInt64("encoderKey");

                field = "encodingPid";
				if (resultSetEncodingJob->isNull("encodingPid"))
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = resultSetEncodingJob->getInt64("encodingPid");

                field = "failuresNumber";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("failuresNumber");  

                field = "encodingJob";
                ingestionJobRoot[field] = encodingJobRoot;
            }
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

		/*
        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
		*/

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

		/*
        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
		*/

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

		/*
        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            _connectionPool->unborrow(conn);
			conn = nullptr;
        }
		*/

        throw e;
    } 
    
    return ingestionJobRoot;
}

void MMSEngineDBFacade::checkWorkspaceStorageAndMaxIngestionNumber (
    int64_t workspaceKey
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        int maxIngestionsNumber;
        int currentIngestionsNumber;
        EncodingPeriod encodingPeriod;
        string periodStartDateTime;
        string periodEndDateTime;

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select c.maxIngestionsNumber, cmi.currentIngestionsNumber, c.encodingPeriod, "
                    "DATE_FORMAT(cmi.startDateTime, '%Y-%m-%d %H:%i:%s') as LocalStartDateTime, "
                    "DATE_FORMAT(cmi.endDateTime, '%Y-%m-%d %H:%i:%s') as LocalEndDateTime "
                "from MMS_Workspace c, MMS_WorkspaceMoreInfo cmi where c.workspaceKey = cmi.workspaceKey and c.workspaceKey = ?";
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
                maxIngestionsNumber = resultSet->getInt("maxIngestionsNumber");
                currentIngestionsNumber = resultSet->getInt("currentIngestionsNumber");
                encodingPeriod = toEncodingPeriod(resultSet->getString("encodingPeriod"));
                periodStartDateTime = resultSet->getString("LocalStartDateTime");
                periodEndDateTime = resultSet->getString("LocalEndDateTime");                
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

        // check maxStorage first        
        {
            int64_t workSpaceUsageInBytes;
            int64_t maxStorageInMB;

            pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB = getWorkspaceUsage(conn, workspaceKey);
            tie(workSpaceUsageInBytes, maxStorageInMB) = workSpaceUsageInBytesAndMaxStorageInMB;
            
            int64_t totalSizeInMB = workSpaceUsageInBytes / 1000000;
            if (totalSizeInMB >= maxStorageInMB)
            {
                string errorMessage = __FILEREF__ + "Reached the max storage dedicated for your Workspace"
                    + ", maxStorageInMB: " + to_string(maxStorageInMB)
                    + ", totalSizeInMB: " + to_string(totalSizeInMB)
                    + ". It is needed to increase Workspace capacity."
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        bool ingestionsAllowed = true;
        bool periodExpired = false;
        char newPeriodStartDateTime [64];
        char newPeriodEndDateTime [64];

        {
            char                strDateTimeNow [64];
            tm                  tmDateTimeNow;
            chrono::system_clock::time_point now = chrono::system_clock::now();
            time_t utcTimeNow = chrono::system_clock::to_time_t(now);
            localtime_r (&utcTimeNow, &tmDateTimeNow);

            sprintf (strDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d",
                tmDateTimeNow. tm_year + 1900,
                tmDateTimeNow. tm_mon + 1,
                tmDateTimeNow. tm_mday,
                tmDateTimeNow. tm_hour,
                tmDateTimeNow. tm_min,
                tmDateTimeNow. tm_sec);

            if (periodStartDateTime.compare(strDateTimeNow) <= 0 && periodEndDateTime.compare(strDateTimeNow) >= 0)
            {
                // Period not expired

                // periodExpired = false; already initialized
                
                if (currentIngestionsNumber >= maxIngestionsNumber)
                {
                    // no more ingestions are allowed for this workspace

                    ingestionsAllowed = false;
                }
            }
            else
            {
                // Period expired

                periodExpired = true;
                
                if (encodingPeriod == EncodingPeriod::Daily)
                {
                    sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            0,  // tmDateTimeNow. tm_hour,
                            0,  // tmDateTimeNow. tm_min,
                            0  // tmDateTimeNow. tm_sec
                    );
                    sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            23,  // tmCurrentDateTime. tm_hour,
                            59,  // tmCurrentDateTime. tm_min,
                            59  // tmCurrentDateTime. tm_sec
                    );
                }
                else if (encodingPeriod == EncodingPeriod::Weekly)
                {
                    // from monday to sunday
                    // monday
                    {
                        int daysToHavePreviousMonday;

                        if (tmDateTimeNow.tm_wday == 0)  // Sunday
                            daysToHavePreviousMonday = 6;
                        else
                            daysToHavePreviousMonday = tmDateTimeNow.tm_wday - 1;

                        chrono::system_clock::time_point mondayOfCurrentWeek;
                        if (daysToHavePreviousMonday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHavePreviousMonday);
                            mondayOfCurrentWeek = now - days;
                        }
                        else
                            mondayOfCurrentWeek = now;

                        tm                  tmMondayOfCurrentWeek;
                        time_t utcTimeMondayOfCurrentWeek = chrono::system_clock::to_time_t(mondayOfCurrentWeek);
                        localtime_r (&utcTimeMondayOfCurrentWeek, &tmMondayOfCurrentWeek);

                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmMondayOfCurrentWeek. tm_year + 1900,
                                tmMondayOfCurrentWeek. tm_mon + 1,
                                tmMondayOfCurrentWeek. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // sunday
                    {
                        int daysToHaveNextSunday;

                        daysToHaveNextSunday = 7 - tmDateTimeNow.tm_wday;

                        chrono::system_clock::time_point sundayOfCurrentWeek;
                        if (daysToHaveNextSunday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHaveNextSunday);
                            sundayOfCurrentWeek = now + days;
                        }
                        else
                            sundayOfCurrentWeek = now;

                        tm                  tmSundayOfCurrentWeek;
                        time_t utcTimeSundayOfCurrentWeek = chrono::system_clock::to_time_t(sundayOfCurrentWeek);
                        localtime_r (&utcTimeSundayOfCurrentWeek, &tmSundayOfCurrentWeek);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmSundayOfCurrentWeek. tm_year + 1900,
                                tmSundayOfCurrentWeek. tm_mon + 1,
                                tmSundayOfCurrentWeek. tm_mday,
                                23,  // tmSundayOfCurrentWeek. tm_hour,
                                59,  // tmSundayOfCurrentWeek. tm_min,
                                59  // tmSundayOfCurrentWeek. tm_sec
                        );
                    }
                }
                else if (encodingPeriod == EncodingPeriod::Monthly)
                {
                    // first day of the month
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        tm                  tmLastDayOfCurrentMonth = tmDateTimeNow;

                        tmLastDayOfCurrentMonth.tm_mday = 1;

                        // Next month 0=Jan
                        if (tmLastDayOfCurrentMonth.tm_mon == 11)    // Dec
                        {
                            tmLastDayOfCurrentMonth.tm_mon = 0;
                            tmLastDayOfCurrentMonth.tm_year++;
                        }
                        else
                        {
                            tmLastDayOfCurrentMonth.tm_mon++;
                        }

                        // Get the first day of the next month
                        time_t utcTimeLastDayOfCurrentMonth = mktime (&tmLastDayOfCurrentMonth);

                        // Subtract 1 day
                        utcTimeLastDayOfCurrentMonth -= 86400;

                        // Convert back to date and time
                        localtime_r (&utcTimeLastDayOfCurrentMonth, &tmLastDayOfCurrentMonth);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmLastDayOfCurrentMonth. tm_year + 1900,
                                tmLastDayOfCurrentMonth. tm_mon + 1,
                                tmLastDayOfCurrentMonth. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
                else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
                {
                    // first day of the year
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                1,  // tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                12, // tmDateTimeNow. tm_mon + 1,
                                31, // tmDateTimeNow. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
            }
        }
        
        if (periodExpired)
        {
            lastSQLCommand = 
                "update MMS_WorkspaceMoreInfo set currentIngestionsNumber = 0, "
                "startDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), endDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S') "
                "where workspaceKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, newPeriodStartDateTime);
            preparedStatement->setString(queryParameterIndex++, newPeriodEndDateTime);
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newPeriodStartDateTime: " + newPeriodStartDateTime
				+ ", newPeriodEndDateTime: " + newPeriodEndDateTime
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newPeriodStartDateTime: " + newPeriodStartDateTime
                        + ", newPeriodEndDateTime: " + newPeriodEndDateTime
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        if (!ingestionsAllowed)
        {
            string errorMessage = __FILEREF__ + "Reached the max number of Ingestions in your period"
                + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
                + ", encodingPeriod: " + toString(encodingPeriod)
                    + ". It is needed to increase Workspace capacity."
            ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
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
        _logger->error(__FILEREF__ + "exception"
            + ", e.what: " + e.what()
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

void MMSEngineDBFacade::fixIngestionJobsHavingWrongStatus()
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "fixIngestionJobsHavingWrongStatus"
			);

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		// Scenarios: EncodingJob in final status but IngestionJob not in final status
		//	This is independently by the specific instance of mms-engine (because in this scenario
		//	often the processor field is empty) but someone has to do it
		//	This scenario may happen in case the mms-engine is shutdown not in friendly way
		{
			int toleranceInHours = 4;
			lastSQLCommand = 
				"select ij.ingestionJobKey, ej.encodingJobKey, "
				"ij.status as ingestionJobStatus, ej.status as encodingJobStatus "
				"from MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ij.ingestionJobKey = ej.ingestionJobKey "
				"and ij.status not like 'End_%' and ej.status like 'End_%'"
				"and ej.encodingJobEnd < DATE_SUB(NOW(), INTERVAL ? HOUR)";
			;

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, toleranceInHours);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", toleranceInHours: " + to_string(toleranceInHours)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			while (resultSet->next())
			{
				int64_t ingestionJobKey = resultSet->getInt64("ingestionJobKey");
				int64_t encodingJobKey = resultSet->getInt64("encodingJobKey");
				string ingestionJobStatus = resultSet->getString("ingestionJobStatus");
				string encodingJobStatus = resultSet->getString("encodingJobStatus");

				{
					string errorMessage = string("Found IngestionJob having wrong status")
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", ingestionJobStatus: " + ingestionJobStatus
						+ ", encodingJobStatus: " + encodingJobStatus
					;
					_logger->error(__FILEREF__ + errorMessage);

					updateIngestionJob (
						conn,
						ingestionJobKey,
						IngestionStatus::End_CanceledByMMS,
						errorMessage);
				}
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

void MMSEngineDBFacade::updateIngestionJob_LiveRecorder (
	int64_t workspaceKey,
	int64_t ingestionJobKey,
	bool ingestionJobLabelModified, string newIngestionJobLabel,
	bool channelLabelModified, string newChannelLabel,
	bool recordingPeriodStartModified, string newRecordingPeriodStart,
	bool recordingPeriodEndModified, string newRecordingPeriodEnd,
	bool recordingVirtualVODModified, bool newRecordingVirtualVOD,
	bool admin
	)
{
    string		lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		if (ingestionJobLabelModified || channelLabelModified
				|| recordingPeriodStartModified || recordingPeriodEndModified)
        {
			string setSQL;

			if (ingestionJobLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "label = ?";
			}

			if (channelLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "metaDataContent = JSON_SET(metaDataContent, '$.ConfigurationLabel', ?)";
			}

			if (recordingPeriodStartModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "metaDataContent = JSON_SET(metaDataContent, '$.RecordingPeriod.Start', ?)";
			}

			if (recordingPeriodEndModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += "metaDataContent = JSON_SET(metaDataContent, '$.RecordingPeriod.End', ?)";
			}

			if (recordingVirtualVODModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				if (newRecordingVirtualVOD)
					setSQL += "metaDataContent = JSON_SET(metaDataContent, '$.LiveRecorderVirtualVOD', '{}')";
				else
					setSQL += "metaDataContent = JSON_REMOVE(metaDataContent, '$.LiveRecorderVirtualVOD')";
			}

			setSQL = "set " + setSQL + " ";

			lastSQLCommand = 
				string("update MMS_IngestionJob ") + setSQL
				+ "where ingestionJobKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (ingestionJobLabelModified)
				preparedStatement->setString(queryParameterIndex++, newIngestionJobLabel);
			if (channelLabelModified)
				preparedStatement->setString(queryParameterIndex++, newChannelLabel);
			if (recordingPeriodStartModified)
				preparedStatement->setString(queryParameterIndex++, newRecordingPeriodStart);
			if (recordingPeriodEndModified)
				preparedStatement->setString(queryParameterIndex++, newRecordingPeriodEnd);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newIngestionJobLabel: " + newIngestionJobLabel
				+ ", newChannelLabel: " + newChannelLabel
				+ ", newRecordingPeriodStart: " + newRecordingPeriodStart
				+ ", newRecordingPeriodEnd: " + newRecordingPeriodEnd
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			/*
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
						+ ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", newTitle: " + newTitle
						+ ", newUserData: " + newUserData
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
			*/
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

void MMSEngineDBFacade::retentionOfIngestionData()
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "retentionOfIngestionData"
			);

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
			_logger->info(__FILEREF__ + "retentionOfIngestionData. IngestionRoot"
					);
            lastSQLCommand = 
                "delete from MMS_IngestionRoot where ingestionDate < DATE_SUB(NOW(), INTERVAL ? DAY)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, _ingestionWorkflowRetentionInDays);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@ (retentionOfIngestionData)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", _ingestionWorkflowRetentionInDays: " + to_string(_ingestionWorkflowRetentionInDays)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

		// IngestionJobs taking too time to download/move/copy/upload the content are set to failed
		{
			_logger->info(__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time to download/move/copy/upload the content"
				);
			lastSQLCommand = 
				"select ingestionJobKey from MMS_IngestionJob "
				"where status in (?, ?, ?, ?) and sourceBinaryTransferred = 0 "
				"and DATE_ADD(startProcessing, INTERVAL ? HOUR) <= NOW() "
				;
			shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
			preparedStatement->setString(queryParameterIndex++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
			preparedStatement->setString(queryParameterIndex++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
			preparedStatement->setString(queryParameterIndex++,
				   	MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
			preparedStatement->setInt(queryParameterIndex++, _contentNotTransferredRetentionInHours);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@ (retentionOfIngestionData)"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", IngestionStatus::SourceDownloadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress)
				+ ", IngestionStatus::SourceMovingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress)
				+ ", IngestionStatus::SourceCopingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress)
				+ ", IngestionStatus::SourceUploadingInProgress: " + MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress)
				+ ", _contentNotTransferredRetentionInHours: " + to_string(_contentNotTransferredRetentionInHours)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			while (resultSet->next())
			{
				int64_t ingestionJobKey     = resultSet->getInt64("ingestionJobKey");
				{     
           			IngestionStatus newIngestionStatus = IngestionStatus::End_IngestionFailure;

					string errorMessage = "Set to Failure by MMS because of timeout to download/move/copy/upload the content";
					string processorMMS;
					_logger->info(__FILEREF__ + "Update IngestionJob"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + toString(newIngestionStatus)
						+ ", errorMessage: " + errorMessage
						+ ", processorMMS: " + processorMMS
					);                            
    				try
    				{
						updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
					}
    				catch(sql::SQLException se)
    				{
        				string exceptionMessage(se.what());
        
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", exceptionMessage: " + exceptionMessage
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}    
    				catch(runtime_error e)
    				{
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", e.what(): " + e.what()
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}        
    				catch(exception e)
    				{        
        				_logger->error(__FILEREF__ + "SQL exception"
            				+ ", lastSQLCommand: " + lastSQLCommand
            				+ ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        				);
    				}        
				}
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

