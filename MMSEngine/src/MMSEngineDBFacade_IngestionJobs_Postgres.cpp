
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <utility>

void MMSEngineDBFacade::getIngestionsToBeManaged(
	vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType, IngestionStatus>> &ingestionsToBeManaged, string processorMMS,
	int maxIngestionJobs, int timeBeforeToPrepareResourcesInMinutes, bool onlyTasksNotInvolvingMMSEngineThreads
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();
		if (startPoint - _lastConnectionStatsReport >= chrono::seconds(_dbConnectionPoolStatsReportPeriodInSeconds))
		{
			_lastConnectionStatsReport = chrono::system_clock::now();

			DBConnectionPoolStats dbConnectionPoolStats = connectionPool->get_stats();

			_logger->info(
				__FILEREF__ + "DB connection pool stats" + ", _poolSize: " + to_string(dbConnectionPoolStats._poolSize) +
				", _borrowedSize: " + to_string(dbConnectionPoolStats._borrowedSize)
			);
		}

		chrono::system_clock::time_point pointAfterLive;
		chrono::system_clock::time_point pointAfterNotLive;

		int liveProxyToBeIngested = 0;
		int liveRecorderToBeIngested = 0;
		int othersToBeIngested = 0;

		// ingested jobs that do not have to wait a dependency
		{
			pointAfterLive = chrono::system_clock::now();

			int initialGetIngestionJobsCurrentIndex = _getIngestionJobsCurrentIndex;

			_logger->info(
				__FILEREF__ + "getIngestionsToBeManaged" + ", initialGetIngestionJobsCurrentIndex: " + to_string(initialGetIngestionJobsCurrentIndex)
			);

			int mysqlRowCount = _ingestionJobsSelectPageSize;
			bool moreRows = true;
			// 2022-03-14: The next 'while' was commented because it causes
			//		Deadlock. That because we might have the following scenarios:
			//		- first select inside the while by Processor 1
			//		- first select inside the while by Processor 2
			//		- second select inside the while by Processor 1
			//		- second select inside the while by Processor 2
			//		Basically the selects inside the while are mixed among the Processors
			//		and that causes Deadlock
			// while(ingestionsToBeManaged.size() < maxIngestionJobs && moreRows)
			{
				/*
				lastSQLCommand =
					"select ij.ingestionJobKey, ij.label, ir.workspaceKey, "
					"ij.metaDataContent, ij.status, ij.ingestionType, "
					"DATE_FORMAT(convert_tz(ir.ingestionDate, @@session.time_zone, '+00:00'), "
						"'%Y-%m-%dT%H:%i:%sZ') as ingestionDate "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
					"where ir.ingestionRootKey = ij.ingestionRootKey "
					"and ij.processorMMS is null ";
				if (onlyTasksNotInvolvingMMSEngineThreads)
				{
					string tasksNotInvolvingMMSEngineThreadsList =
						"'GroupOfTasks'"
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
						// ", 'Countdown'"
						", 'Live-Grid'"
					;
					lastSQLCommand += "and ij.ingestionType in (" + tasksNotInvolvingMMSEngineThreadsList + ") ";
				}
				lastSQLCommand +=
					"and (ij.status = ? or (ij.status in (?, ?, ?, ?) "
					"and ij.sourceBinaryTransferred = 1)) "
					"and ij.processingStartingFrom <= NOW() "
					"and NOW() <= DATE_ADD(ij.processingStartingFrom, INTERVAL ? DAY) "
					"and ("
					"JSON_EXTRACT(ij.metaDataContent, '$.schedule.start') is null "
					"or UNIX_TIMESTAMP(convert_tz(STR_TO_DATE(JSON_EXTRACT(ij.metaDataContent, '$.schedule.start'), '\"%Y-%m-%dT%H:%i:%sZ\"'),
				'+00:00', @@session.time_zone)) - UNIX_TIMESTAMP(DATE_ADD(NOW(), INTERVAL ? MINUTE)) < 0 "
					") "
					"order by ij.priority asc, ij.processingStartingFrom asc "
					"limit ? offset ?"
					// "limit ? offset ? for update"
				;
				*/
				// 2022-01-06: I wanted to have this select running in parallel among all the engines.
				//      For this reason, I have to use 'select for update'.
				//      At the same time, I had to remove the join because a join would lock everything
				//      Without join, if the select got i.e. 20 rows, all the other rows are not locked
				//      and can be got from the other engines
				// 2023-02-07: added skip locked. Questa opzione è importante perchè evita che la select
				//	attenda eventuali lock di altri engine. Considera che un lock su una riga causa anche
				//	il lock di tutte le righe toccate dalla transazione
				string sqlStatement = fmt::format("select ij.ingestionRootKey, ij.ingestionJobKey, ij.label, "
												  "ij.metaDataContent, ij.status, ij.ingestionType "
												  "from MMS_IngestionJob ij "
												  "where ij.processorMMS is null ");
				if (onlyTasksNotInvolvingMMSEngineThreads)
				{
					string tasksNotInvolvingMMSEngineThreadsList = "'GroupOfTasks'"
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
																   // ", 'Countdown'"
																   ", 'Live-Grid'"
																   ", 'Add-Silent-Audio'";
					sqlStatement += fmt::format("and ij.ingestionType in ({}) ", tasksNotInvolvingMMSEngineThreadsList);
				}
				sqlStatement += fmt::format(
					// "and (ij.status = {} "
					// "or (ij.status in ({}, {}, {}, {}) and ij.sourceBinaryTransferred = true)) "
					"and toBeManaged_virtual "
					"and ij.processingStartingFrom <= NOW() at time zone 'utc' "
					"and NOW() at time zone 'utc' <= ij.processingStartingFrom + INTERVAL '{} days' "
					"and scheduleStart_virtual < (NOW() at time zone 'utc' + INTERVAL '{} minutes') "
					"order by ij.priority asc, ij.processingStartingFrom asc "
					"limit {} offset {} for update skip locked",
					// trans.quote(toString(IngestionStatus::Start_TaskQueued)), trans.quote(toString(IngestionStatus::SourceDownloadingInProgress)),
					// trans.quote(toString(IngestionStatus::SourceMovingInProgress)), trans.quote(toString(IngestionStatus::SourceCopingInProgress)),
					// trans.quote(toString(IngestionStatus::SourceUploadingInProgress)),
					_doNotManageIngestionsOlderThanDays, timeBeforeToPrepareResourcesInMinutes, mysqlRowCount, _getIngestionJobsCurrentIndex
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				chrono::milliseconds internalSqlDuration(0);
				result res = trans.exec(sqlStatement);
				if (res.size() < mysqlRowCount)
					moreRows = false;
				else
					moreRows = true;
				_getIngestionJobsCurrentIndex += _ingestionJobsSelectPageSize;
				int resultSetIndex = 0;
				for (auto row : res)
				{
					int64_t ingestionRootKey = row["ingestionRootKey"].as<int64_t>();
					int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
					string ingestionJobLabel = row["label"].as<string>();
					string metaDataContent = row["metaDataContent"].as<string>();
					IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(row["status"].as<string>());
					IngestionType ingestionType = MMSEngineDBFacade::toIngestionType(row["ingestionType"].as<string>());

					_logger->info(
						__FILEREF__ + "getIngestionsToBeManaged (result set loop)" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", ingestionJobLabel: " + ingestionJobLabel +
						", initialGetIngestionJobsCurrentIndex: " + to_string(initialGetIngestionJobsCurrentIndex) +
						", resultSetIndex: " + to_string(resultSetIndex) + "/" + to_string(res.size())
					);
					resultSetIndex++;

					auto [workspaceKey, ingestionDate] = workflowQuery_WorkspaceKeyIngestionDate(ingestionRootKey, false);

					tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> ingestionJobToBeManagedInfo =
						isIngestionJobToBeManaged(ingestionJobKey, workspaceKey, ingestionStatus, ingestionType, conn, &trans);

					bool ingestionJobToBeManaged;
					int64_t dependOnIngestionJobKey;
					int dependOnSuccess;
					IngestionStatus ingestionStatusDependency;

					tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency) = ingestionJobToBeManagedInfo;

					if (ingestionJobToBeManaged)
					{
						_logger->info(
							__FILEREF__ + "Adding jobs to be processed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", ingestionStatus: " + toString(ingestionStatus)
						);

						shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

						tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType, IngestionStatus> ingestionToBeManaged =
							make_tuple(ingestionJobKey, ingestionJobLabel, workspace, ingestionDate, metaDataContent, ingestionType, ingestionStatus);

						ingestionsToBeManaged.push_back(ingestionToBeManaged);
						othersToBeIngested++;

						if (ingestionsToBeManaged.size() >= maxIngestionJobs)
							break;
					}
					else
					{
						_logger->info(
							__FILEREF__ + "Ingestion job cannot be processed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", ingestionStatus: " + toString(ingestionStatus) + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey) +
							", dependOnSuccess: " + to_string(dependOnSuccess) + ", ingestionStatusDependency: " + toString(ingestionStatusDependency)
						);
					}
				}
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@getIngestionsToBeManaged@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
				);

				_logger->info(
					__FILEREF__ + "getIngestionsToBeManaged" + ", _getIngestionJobsCurrentIndex: " + to_string(_getIngestionJobsCurrentIndex) +
					", res.size: " + to_string(res.size()) + ", ingestionsToBeManaged.size(): " + to_string(ingestionsToBeManaged.size()) +
					", moreRows: " + to_string(moreRows)
				);

				if (res.size() == 0)
					_getIngestionJobsCurrentIndex = 0;
			}
			// if (ingestionsToBeManaged.size() < maxIngestionJobs)
			// 	_getIngestionJobsCurrentIndex = 0;

			pointAfterNotLive = chrono::system_clock::now();

			_logger->info(
				__FILEREF__ + "getIngestionsToBeManaged (exit)" + ", _getIngestionJobsCurrentIndex: " + to_string(_getIngestionJobsCurrentIndex) +
				", ingestionsToBeManaged.size(): " + to_string(ingestionsToBeManaged.size()) + ", moreRows: " + to_string(moreRows) +
				", onlyTasksNotInvolvingMMSEngineThreads: " + to_string(onlyTasksNotInvolvingMMSEngineThreads) +
				", select not live elapsed (millisecs): " +
				to_string(chrono::duration_cast<chrono::milliseconds>(pointAfterNotLive - pointAfterLive).count())
			);
		}

		for (tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType, IngestionStatus> &ingestionToBeManaged :
			 ingestionsToBeManaged)
		{
			int64_t ingestionJobKey;
			MMSEngineDBFacade::IngestionStatus ingestionStatus;

			tie(ingestionJobKey, ignore, ignore, ignore, ignore, ignore, ingestionStatus) = ingestionToBeManaged;

			string sqlStatement;
			if (ingestionStatus == IngestionStatus::SourceMovingInProgress || ingestionStatus == IngestionStatus::SourceCopingInProgress ||
				ingestionStatus == IngestionStatus::SourceUploadingInProgress)
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

				sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob set processorMMS = {} "
					"where ingestionJobKey = {} returning 1) select count(*) from rows",
					trans.quote(processorMMS), ingestionJobKey
				);
			}
			else
			{
				sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob set startProcessing = NOW() at time zone 'utc', "
					"processorMMS = {} where ingestionJobKey = {} returning 1) select count(*) from rows",
					trans.quote(processorMMS), ingestionJobKey
				);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(
			__FILEREF__ + "getIngestionsToBeManaged statistics" +
			", total elapsed (millisecs): " + to_string(chrono::duration_cast<chrono::milliseconds>(endPoint - startPoint).count()) +
			", select live elapsed (millisecs): " + to_string(chrono::duration_cast<chrono::milliseconds>(pointAfterLive - startPoint).count()) +
			", select not live elapsed (millisecs): " +
			to_string(chrono::duration_cast<chrono::milliseconds>(pointAfterNotLive - pointAfterLive).count()) +
			", processing entries elapsed (millisecs): " +
			to_string(chrono::duration_cast<chrono::milliseconds>(endPoint - pointAfterNotLive).count()) +
			", ingestionsToBeManaged.size: " + to_string(ingestionsToBeManaged.size()) + ", maxIngestionJobs: " + to_string(maxIngestionJobs) +
			", liveProxyToBeIngested: " + to_string(liveProxyToBeIngested) + ", liveRecorderToBeIngested: " + to_string(liveRecorderToBeIngested) +
			", othersToBeIngested: " + to_string(othersToBeIngested)
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
	catch (DBRecordNotFound &e)
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

tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> MMSEngineDBFacade::isIngestionJobToBeManaged(
	int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType, shared_ptr<PostgresConnection> conn,
	transaction_base *trans
)
{
	bool ingestionJobToBeManaged = true;
	int64_t dependOnIngestionJobKey = -1;
	int dependOnSuccess = -1;
	IngestionStatus ingestionStatusDependency;

	try
	{
		bool atLeastOneDependencyRowFound = false;

		string sqlStatement = fmt::format(
			"select dependOnIngestionJobKey, dependOnSuccess "
			"from MMS_IngestionJobDependency "
			"where ingestionJobKey = {} order by orderNumber asc",
			ingestionJobKey
		);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans->exec(sqlStatement);
		// 2019-10-05: GroupOfTasks has always to be executed once the dependencies are in a final state.
		//	This is because the manageIngestionJobStatusUpdate do not update the GroupOfTasks with a state
		//	like End_NotToBeExecuted
		for (auto row : res)
		{
			if (!atLeastOneDependencyRowFound)
				atLeastOneDependencyRowFound = true;

			if (!row["dependOnIngestionJobKey"].is_null())
			{
				dependOnIngestionJobKey = row["dependOnIngestionJobKey"].as<int64_t>();
				dependOnSuccess = row["dependOnSuccess"].as<int>();

				string sqlStatement = fmt::format("select status from MMS_IngestionJob where ingestionJobKey = {}", dependOnIngestionJobKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (!empty(res))
				{
					string sStatus = res[0]["status"].as<string>();

					// _logger->info(__FILEREF__ + "Dependency for the IngestionJob"
					// + ", ingestionJobKey: " + to_string(ingestionJobKey)
					// + ", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey)
					// + ", dependOnSuccess: " + to_string(dependOnSuccess)
					// + ", status (dependOnIngestionJobKey): " + sStatus
					// );

					ingestionStatusDependency = MMSEngineDBFacade::toIngestionStatus(sStatus);

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
							if (dependOnSuccess == 1 && MMSEngineDBFacade::isIngestionStatusFailed(ingestionStatusDependency))
							{
								ingestionJobToBeManaged = false;

								break;
							}
							else if (dependOnSuccess == 0 && MMSEngineDBFacade::isIngestionStatusSuccess(ingestionStatusDependency))
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
					_logger->info(
						__FILEREF__ + "Dependency for the IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", dependOnIngestionJobKey: " + to_string(dependOnIngestionJobKey) + ", dependOnSuccess: " + to_string(dependOnSuccess) +
						", status: " + "no row"
					);
				}
				_logger->info(
					__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", dependOnIngestionJobKey: " +
					to_string(dependOnIngestionJobKey) + ", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
					to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
				);
			}
			else
			{
				_logger->info(
					__FILEREF__ + "Dependency for the IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", dependOnIngestionJobKey: " + "null"
				);
			}
		}
		SPDLOG_INFO(
			"SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
		);

		if (!atLeastOneDependencyRowFound)
		{
			// this is not possible, even an ingestionJob without dependency has a row
			// (with dependOnIngestionJobKey NULL)

			_logger->error(
				__FILEREF__ + "No dependency Row for the IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", workspaceKey: " + to_string(workspaceKey) + ", ingestionStatus: " + to_string(static_cast<int>(ingestionStatus)) +
				", ingestionType: " + to_string(static_cast<int>(ingestionType))
			);
			ingestionJobToBeManaged = false;
		}

		return make_tuple(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency);
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

int64_t MMSEngineDBFacade::addIngestionJob(
	shared_ptr<PostgresConnection> conn, work &trans, int64_t workspaceKey, int64_t ingestionRootKey, string label, string metadataContent,
	MMSEngineDBFacade::IngestionType ingestionType, string processingStartingFrom, vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
	vector<int64_t> waitForGlobalIngestionJobKeys
)
{
	int64_t ingestionJobKey;

	try
	{
		{
			string sqlStatement = fmt::format("select c.enabled, c.workspaceType from MMS_Workspace c where c.workspaceKey = {}", workspaceKey);
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
				bool enabled = res[0]["enabled"].as<bool>();
				int workspaceType = res[0]["workspaceType"].as<int>();

				if (!enabled)
				{
					string errorMessage =
						__FILEREF__ + "Workspace is not enabled" + ", workspaceKey: " + to_string(workspaceKey) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (workspaceType != static_cast<int>(WorkspaceType::IngestionAndDelivery) &&
						 workspaceType != static_cast<int>(WorkspaceType::EncodingOnly))
				{
					string errorMessage = fmt::format(
						"Workspace is not enabled to ingest content"
						", workspaceKey: {}"
						", workspaceType: {}"
						", sqlStatement: {}",
						workspaceKey, static_cast<int>(workspaceType), sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			else
			{
				string errorMessage = __FILEREF__ + "Workspace is not present/configured" + ", workspaceKey: " + to_string(workspaceKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			_logger->info(
				__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", workspaceKey: " + to_string(workspaceKey) +
				", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
			);
		}

		IngestionStatus ingestionStatus;
		string errorMessage;
		{
			{
				string sqlStatement = fmt::format(
					"insert into MMS_IngestionJob (ingestionJobKey, ingestionRootKey, label, "
					"metaDataContent, ingestionType, priority, "
					"processingStartingFrom, "
					"startProcessing, endProcessing, downloadingProgress, "
					"uploadingProgress, sourceBinaryTransferred, processorMMS, status, errorMessage) "
					"values ("
					"DEFAULT,          {},                {}, "
					"{},               {},             {}, "
					"to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), "
					"NULL,            NULL,         NULL, "
					"NULL,              false,                   NULL,         {},      NULL) "
					"returning ingestionJobKey",
					ingestionRootKey, trans.quote(label), trans.quote(metadataContent), trans.quote(toString(ingestionType)),
					getIngestionTypePriority(ingestionType), trans.quote(processingStartingFrom),
					trans.quote(toString(MMSEngineDBFacade::IngestionStatus::Start_TaskQueued))
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				ingestionJobKey = trans.exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			{
				int orderNumber = 0;
				bool referenceOutputDependency = false;
				if (dependOnIngestionJobKeys.size() == 0)
				{
					addIngestionJobDependency(conn, trans, ingestionJobKey, dependOnSuccess, -1, orderNumber, referenceOutputDependency);
					orderNumber++;
				}
				else
				{
					for (int64_t dependOnIngestionJobKey : dependOnIngestionJobKeys)
					{
						addIngestionJobDependency(
							conn, trans, ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, orderNumber, referenceOutputDependency
						);

						orderNumber++;
					}
				}

				{
					int waitForDependOnSuccess = -1; // OnComplete
					for (int64_t dependOnIngestionJobKey : waitForGlobalIngestionJobKeys)
					{
						addIngestionJobDependency(
							conn, trans, ingestionJobKey, waitForDependOnSuccess, dependOnIngestionJobKey, orderNumber, referenceOutputDependency
						);

						orderNumber++;
					}
				}
			}
		}
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}

	return ingestionJobKey;
}

int MMSEngineDBFacade::getIngestionTypePriority(MMSEngineDBFacade::IngestionType ingestionType)
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
	if (ingestionType == IngestionType::LiveProxy || ingestionType == IngestionType::VODProxy || ingestionType == IngestionType::Countdown ||
		ingestionType == IngestionType::YouTubeLiveBroadcast)
		return 1;
	else if (ingestionType == IngestionType::LiveRecorder)
		return 5;
	else if (ingestionType == IngestionType::AddContent)
		return 10;
	else
		return 15;
}

void MMSEngineDBFacade::addIngestionJobDependency(
	shared_ptr<PostgresConnection> conn, work &trans, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey, int orderNumber,
	bool referenceOutputDependency
)
{
	try
	{
		int localOrderNumber = 0;
		if (orderNumber == -1)
		{
			string sqlStatement = fmt::format(
				"select max(orderNumber) as maxOrderNumber from MMS_IngestionJobDependency "
				"where ingestionJobKey = {}",
				ingestionJobKey
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
				if (res[0]["maxOrderNumber"].is_null())
					localOrderNumber = 0;
				else
					localOrderNumber = res[0]["maxOrderNumber"].as<int>() + 1;
			}
			_logger->info(
				__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
			);
		}
		else
		{
			localOrderNumber = orderNumber;
		}

		{
			string sqlStatement = fmt::format(
				"insert into MMS_IngestionJobDependency (ingestionJobDependencyKey,"
				"ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, "
				"orderNumber, referenceOutputDependency) values ("
				"DEFAULT, {}, {}, {}, {}, {})",
				ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey == -1 ? "null" : to_string(dependOnIngestionJobKey), localOrderNumber,
				referenceOutputDependency ? 1 : 0
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
		}
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

void MMSEngineDBFacade::changeIngestionJobDependency(int64_t previousDependOnIngestionJobKey, int64_t newDependOnIngestionJobKey)
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
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJobDependency set dependOnIngestionJobKey = {} "
				"where dependOnIngestionJobKey = {} returning 1) select count(*) from rows",
				newDependOnIngestionJobKey, previousDependOnIngestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", newDependOnIngestionJobKey: " + to_string(newDependOnIngestionJobKey) +
									  ", previousDependOnIngestionJobKey: " + to_string(previousDependOnIngestionJobKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "MMS_IngestionJobDependency updated successful" + ", newDependOnIngestionJobKey: " + to_string(newDependOnIngestionJobKey) +
			", previousDependOnIngestionJobKey: " + to_string(previousDependOnIngestionJobKey)
		);

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

void MMSEngineDBFacade::updateIngestionJobMetadataContent(int64_t ingestionJobKey, string metadataContent)
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
		updateIngestionJobMetadataContent(conn, trans, ingestionJobKey, metadataContent);

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

void MMSEngineDBFacade::updateIngestionJobMetadataContent(
	shared_ptr<PostgresConnection> conn, nontransaction &trans, int64_t ingestionJobKey, string metadataContent
)
{
	try
	{
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set metadataContent = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(metadataContent), ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", metadataContent: " + metadataContent +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "IngestionJob updated successful" + ", metadataContent: " + metadataContent +
			", ingestionJobKey: " + to_string(ingestionJobKey)
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

void MMSEngineDBFacade::updateIngestionJobParentGroupOfTasks(
	shared_ptr<PostgresConnection> conn, work &trans, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey
)
{
	try
	{
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set parentGroupOfTasksIngestionJobKey = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				parentGroupOfTasksIngestionJobKey, ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" +
									  ", parentGroupOfTasksIngestionJobKey: " + to_string(parentGroupOfTasksIngestionJobKey) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "IngestionJob updated successful" + ", parentGroupOfTasksIngestionJobKey: " + to_string(parentGroupOfTasksIngestionJobKey) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

void MMSEngineDBFacade::updateIngestionJob(int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage, string processorMMS)
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
		updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage, processorMMS);

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

void MMSEngineDBFacade::updateIngestionJob(
	shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage,
	string processorMMS
)
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
			errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
	}

	// 2022-12-08: in case of deadlock we will retry.
	//	Penso che il Deadlock sia causato dal fatto che, insieme a questo update dell'IngestionJob,
	//	abbiamo anche getIngestionsToBeManaged in esecuzione con un'altra MySQLConnection.
	//	Questo retry risolve il problema perchè, lo sleep qui permette a getIngestionsToBeManaged di terminare,
	//	e quindi, non avremmo piu il Deadlock.
	//	Inizialmente il retry era fatto sul metodo updateIngestionJob, come questo ma senza MySQLConnection,
	//	poi è stato spostato qui perchè tanti scenari chiamano direttamente questo metodo con MySQLConnection
	//	e serviva anche qui il retry.
	bool updateToBeTriedAgain = true;
	int retriesNumber = 0;
	int maxRetriesNumber = 3;
	int secondsBetweenRetries = 5;
	while (updateToBeTriedAgain && retriesNumber < maxRetriesNumber)
	{
		retriesNumber++;

		try
		{
			if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
			{
				string sqlStatement;
				string processorMMSUpdate;
				if (processorMMS != "noToBeUpdated")
				{
					if (processorMMS == "")
						processorMMSUpdate = fmt::format("processorMMS = null, ");
					else
						processorMMSUpdate = fmt::format("processorMMS = {}, ", trans->quote(processorMMS));
				}

				if (errorMessageForSQL == "")
					sqlStatement = fmt::format(
						"WITH rows AS (update MMS_IngestionJob set status = {}, "
						"{} endProcessing = NOW() at time zone 'utc' "
						"where ingestionJobKey = {} returning 1) select count(*) from rows",
						trans->quote(toString(newIngestionStatus)), processorMMSUpdate, ingestionJobKey
					);
				else
					sqlStatement = fmt::format(
						"WITH rows AS (update MMS_IngestionJob set status = {}, "
						"errorMessage = SUBSTRING({} || '\n' || coalesce(errorMessage, ''), 1, 1024 * 20), "
						"{} endProcessing = NOW() at time zone 'utc' "
						"where ingestionJobKey = {} returning 1) select count(*) from rows",
						trans->quote(toString(newIngestionStatus)), trans->quote(errorMessageForSQL), processorMMSUpdate, ingestionJobKey
					);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
										  ", errorMessageForSQL: " + errorMessageForSQL + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					// it is not so important to block the continuation of this method
					// Also the exception caused a crash of the process
					// throw runtime_error(errorMessage);
				}
			}
			else
			{
				string sqlStatement;
				string processorMMSUpdate;
				if (processorMMS != "noToBeUpdated")
				{
					if (processorMMS == "")
						processorMMSUpdate = fmt::format(", processorMMS = null ");
					else
						processorMMSUpdate = fmt::format(", processorMMS = {} ", trans->quote(processorMMS));
				}

				if (errorMessageForSQL == "")
					sqlStatement = fmt::format(
						"WITH rows AS (update MMS_IngestionJob set status = {} "
						"{} "
						"where ingestionJobKey = {} returning 1) select count(*) from rows",
						trans->quote(toString(newIngestionStatus)), processorMMSUpdate, ingestionJobKey
					);
				else
					sqlStatement = fmt::format(
						"WITH rows AS (update MMS_IngestionJob set status = {}, "
						"errorMessage = SUBSTRING({} || '\n' || coalesce(errorMessage, ''), 1, 1024 * 20) "
						"{} "
						"where ingestionJobKey = {} returning 1) select count(*) from rows",
						trans->quote(toString(newIngestionStatus)), trans->quote(errorMessageForSQL), processorMMSUpdate, ingestionJobKey
					);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
										  ", errorMessageForSQL: " + errorMessageForSQL + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					// it is not so important to block the continuation of this method
					// Also the exception caused a crash of the process
					// throw runtime_error(errorMessage);
				}
			}

			bool updateIngestionRootStatus = true;
			manageIngestionJobStatusUpdate(ingestionJobKey, newIngestionStatus, updateIngestionRootStatus, conn, trans);

			_logger->info(
				__FILEREF__ + "IngestionJob updated successful" + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", processorMMS: " + processorMMS
			);

			updateToBeTriedAgain = false;
		}
		catch (sql_error const &e)
		{
			// in caso di Postgres non so ancora la parola da cercare nell'errore che indica un deadlock,
			// per cui forzo un retry in attesa di avere l'errore e gestire meglio
			// if (exceptionMessage.find("Deadlock") != string::npos
			// 	&& retriesNumber < maxRetriesNumber)
			{
				_logger->warn(
					__FILEREF__ + "SQL exception (Deadlock), waiting before to try again" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", sqlStatement: " + e.query() + ", exceptionMessage: " + e.what() + ", retriesNumber: " + to_string(retriesNumber) +
					", maxRetriesNumber: " + to_string(maxRetriesNumber) + ", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
				);

				this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
			}
			/*
			else
			{
				SPDLOG_ERROR("SQL exception"
					", query: {}"
					", exceptionMessage: {}"
					", conn: {}",
					e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
				);

				throw e;
			}
			*/
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				"runtime_error"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);

			throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				"exception"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);

			throw e;
		}
	}
}

void MMSEngineDBFacade::appendIngestionJobErrorMessage(int64_t ingestionJobKey, string errorMessage)
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
		string errorMessageForSQL;
		if (errorMessage.length() >= 1024)
			errorMessageForSQL = errorMessage.substr(0, 1024);
		else
			errorMessageForSQL = errorMessage;
		{
			string strToBeReplaced = "FFMpeg";
			string strToReplace = "XXX";
			if (errorMessageForSQL.find(strToBeReplaced) != string::npos)
				errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}

		if (errorMessageForSQL != "")
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob "
				"set errorMessage = SUBSTRING({} || '\n' || coalesce(errorMessage, ''), 1, 1024 * 20) "
				"where ingestionJobKey = {} "
				"and status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') " // not like 'End_%' "
				"returning 1) select count(*) from rows",
				trans.quote(errorMessageForSQL), ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", errorMessageForSQL: " + errorMessageForSQL +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_exception(errorMessage);
			}
			else
			{
				_logger->info(__FILEREF__ + "IngestionJob updated successful" + ", ingestionJobKey: " + to_string(ingestionJobKey));
			}
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

void MMSEngineDBFacade::manageIngestionJobStatusUpdate(
	int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, shared_ptr<PostgresConnection> conn,
	transaction_base *trans
)
{
	try
	{
		_logger->info(
			__FILEREF__ + "manageIngestionJobStatusUpdate" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", newIngestionStatus: " + toString(newIngestionStatus) + ", updateIngestionRootStatus: " + to_string(updateIngestionRootStatus)
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
					// - GroupOfTasks cannot be set to End_NotToBeExecuted, it has always to be executed

					// 2019-10-05: referenceOutputDependency==1 specifies that this dependency comes
					//	from a ReferenceOutput, it means his parent is a GroupOfTasks.
					//	This dependency has not to be considered here because, the only dependency
					//	to be used is the one with the GroupOfTasks
					string sqlStatement;
					if (hierarchicalLevelIndex == 0)
					{
						sqlStatement = fmt::format(
							"select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey in ({}) and ijd.dependOnSuccess = {}",
							ingestionJobKeysToFindDependencies, dependOnSuccess
						);
					}
					else
					{
						sqlStatement = fmt::format(
							"select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey in ({})",
							ingestionJobKeysToFindDependencies
						);
					}
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans->exec(sqlStatement);
					bool dependenciesFound = false;
					ingestionJobKeysToFindDependencies = "";
					for (auto row : res)
					{
						dependenciesFound = true;

						if (hierarchicalIngestionJobKeysDependencies == "")
							hierarchicalIngestionJobKeysDependencies = to_string(row["ingestionJobKey"].as<int64_t>());
						else
							hierarchicalIngestionJobKeysDependencies += (", " + to_string(row["ingestionJobKey"].as<int64_t>()));

						if (ingestionJobKeysToFindDependencies == "")
							ingestionJobKeysToFindDependencies = to_string(row["ingestionJobKey"].as<int64_t>());
						else
							ingestionJobKeysToFindDependencies += (", " + to_string(row["ingestionJobKey"].as<int64_t>()));
					}
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);

					// _logger->info(__FILEREF__ + "select result"
					// 	+ ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
					// 	+ ", hierarchicalIngestionJobKeysDependencies: " + hierarchicalIngestionJobKeysDependencies
					// );

					if (!dependenciesFound)
					{
						_logger->info(
							__FILEREF__ + "Finished to find dependencies" + ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex) +
							", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged) +
							", ingestionJobKey: " + to_string(ingestionJobKey)
						);

						break;
					}
					else if (dependenciesFound && hierarchicalLevelIndex + 1 >= maxHierarchicalLevelsManaged)
					{
						string errorMessage =
							__FILEREF__ +
							"after maxHierarchicalLevelsManaged we still found dependencies, so maxHierarchicalLevelsManaged has to be increased" +
							", maxHierarchicalLevelsManaged: " + to_string(maxHierarchicalLevelsManaged) +
							", ingestionJobKey: " + to_string(ingestionJobKey) + ", sqlStatement: " + sqlStatement;
						_logger->warn(errorMessage);
					}
				}
			}

			if (hierarchicalIngestionJobKeysDependencies != "")
			{
				_logger->info(
					__FILEREF__ + "manageIngestionJobStatusUpdate. update" + ", status: " + "End_NotToBeExecuted" + ", ingestionJobKey: " +
					to_string(ingestionJobKey) + ", hierarchicalIngestionJobKeysDependencies: " + hierarchicalIngestionJobKeysDependencies
				);

				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob set status = {}, "
					"startProcessing = case when startProcessing IS NULL then NOW() at time zone 'utc' else startProcessing end, "
					"endProcessing = NOW() at time zone 'utc' where ingestionJobKey in ({}) returning 1) select count(*) from rows",
					trans->quote(toString(IngestionStatus::End_NotToBeExecuted)), hierarchicalIngestionJobKeysDependencies
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
		}

		if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
		{
			int64_t ingestionRootKey;
			IngestionRootStatus currentIngestionRootStatus;

			{
				string sqlStatement = fmt::format(
					"select ir.ingestionRootKey, ir.status "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
					"where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = {}",
					ingestionJobKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (!empty(res))
				{
					ingestionRootKey = res[0]["ingestionRootKey"].as<int64_t>();
					currentIngestionRootStatus = MMSEngineDBFacade::toIngestionRootStatus(res[0]["status"].as<string>());
				}
				else
				{
					string errorMessage = __FILEREF__ + "IngestionJob is not found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
					to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
				);
			}

			int successStatesCount = 0;
			int failureStatesCount = 0;
			int intermediateStatesCount = 0;

			{
				string sqlStatement = fmt::format("select status from MMS_IngestionJob where ingestionRootKey = {}", ingestionRootKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				for (auto row : res)
				{
					IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(row["status"].as<string>());

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
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			_logger->info(
				__FILEREF__ + "Job status" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
				", intermediateStatesCount: " + to_string(intermediateStatesCount) + ", successStatesCount: " + to_string(successStatesCount) +
				", failureStatesCount: " + to_string(failureStatesCount)
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
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionRoot set lastUpdate = NOW() at time zone 'utc', status = {} "
					"where ingestionRootKey = {} returning 1) select count(*) from rows",
					trans->quote(toString(newIngestionRootStatus)), ingestionRootKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done" + ", ingestionRootKey: " + to_string(ingestionRootKey) +
										  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

void MMSEngineDBFacade::setNotToBeExecutedStartingFromBecauseChunkNotSelected(int64_t ingestionJobKey, string processorMMS)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};

	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

		_logger->info(
			__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", IngestionStatus: " +
			toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected) + ", errorMessage: " + "" + ", processorMMS: " + ""
		);
		updateIngestionJob(
			conn, &trans, ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected,
			"", // errorMessage,
			""	// processorMMS
		);

		// to set 'not to be executed' to the tasks depending from ingestionJobKey,, we will call manageIngestionJobStatusUpdate
		// simulating the IngestionJob failed, that cause the setting to 'not to be executed'
		// for the onSuccess tasks

		bool updateIngestionRootStatus = false;
		manageIngestionJobStatusUpdate(ingestionJobKey, IngestionStatus::End_IngestionFailure, updateIngestionRootStatus, conn, &trans);

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(
			__FILEREF__ + "setNotToBeExecutedStartingFrom statistics" +
			", elapsed (millisecs): " + to_string(chrono::duration_cast<chrono::milliseconds>(endPoint - startPoint).count())
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

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress(int64_t ingestionJobKey, double downloadingPercentage)
{
	bool canceledByUser = false;

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
				"WITH rows AS (update MMS_IngestionJob set downloadingProgress = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				downloadingPercentage, ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", downloadingPercentage: " + to_string(downloadingPercentage) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = fmt::format("select status from MMS_IngestionJob where ingestionJobKey = {}", ingestionJobKey);
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
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());

				if (ingestionStatus == IngestionStatus::End_CanceledByUser)
					canceledByUser = true;
			}
			else
			{
				string errorMessage = __FILEREF__ + "IngestionJob is not found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			_logger->info(
				__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
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

	return canceledByUser;
}

bool MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress(int64_t ingestionJobKey, double uploadingPercentage)
{
	bool toBeCancelled = false;

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
				"WITH rows AS (update MMS_IngestionJob set uploadingProgress = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				uploadingPercentage, ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", uploadingPercentage: " + to_string(uploadingPercentage) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = fmt::format("select status from MMS_IngestionJob where ingestionJobKey = {}", ingestionJobKey);
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
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());

				if (ingestionStatus == IngestionStatus::End_CanceledByUser)
					toBeCancelled = true;
			}
			else
			{
				string errorMessage = __FILEREF__ + "IngestionJob is not found" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			_logger->info(
				__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
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

	return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred(int64_t ingestionJobKey, bool sourceBinaryTransferred)
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
		{
			string sqlStatement;
			if (sourceBinaryTransferred)
				sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob set sourceBinaryTransferred = {}, "
					"uploadingProgress = 100 "
					"where ingestionJobKey = {} returning 1) select count(*) from rows",
					sourceBinaryTransferred, ingestionJobKey
				);
			else
				sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob set sourceBinaryTransferred = {} "
					"where ingestionJobKey = {} returning 1) select count(*) from rows",
					sourceBinaryTransferred, ingestionJobKey
				);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
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

string MMSEngineDBFacade::ingestionRoot_columnAsString(int64_t workspaceKey, string columnName, int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		string requestedColumn = fmt::format("mms_ingestionroot:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = workflowQuery(requestedColumns, workspaceKey, ingestionRootKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>(string());
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionRootKey, fromMaster
		);

		throw e;
	}
}

/*
string MMSEngineDBFacade::ingestionRoot_MetadataContent(int64_t workspaceKey, int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionroot:.metaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = workflowQuery(requestedColumns, workspaceKey, ingestionRootKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionRootKey, fromMaster
		);

		throw e;
	}
}

string MMSEngineDBFacade::ingestionRoot_ProcessedMetadataContent(int64_t workspaceKey, int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionroot:.processedMetaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = workflowQuery(requestedColumns, workspaceKey, ingestionRootKey, fromMaster);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionRootKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionRootKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionRootKey, fromMaster
		);

		throw e;
	}
}
*/

tuple<string, MMSEngineDBFacade::IngestionType, json, string>
MMSEngineDBFacade::ingestionJob_LabelIngestionTypeMetadataContentErrorMessage(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_ingestionjob:ij.label", "mms_ingestionjob:ij.ingestionType", "mms_ingestionjob:ij.metaDataContent",
			"mms_ingestionjob:ij.errorMessage"
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return make_tuple(
			(*sqlResultSet)[0][0].as<string>(""), toIngestionType((*sqlResultSet)[0][1].as<string>("")), (*sqlResultSet)[0][2].as<json>(json()),
			(*sqlResultSet)[0][3].as<string>("")
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

pair<MMSEngineDBFacade::IngestionType, json>
MMSEngineDBFacade::ingestionJob_IngestionTypeMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.ingestionType", "mms_ingestionjob:ij.metaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return make_pair(toIngestionType((*sqlResultSet)[0][0].as<string>("")), (*sqlResultSet)[0][1].as<json>(json()));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

tuple<MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus, json>
MMSEngineDBFacade::ingestionJob_IngestionTypeStatusMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.ingestionType", "mms_ingestionjob:ij.status", "mms_ingestionjob:ij.metaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return make_tuple(
			toIngestionType((*sqlResultSet)[0][0].as<string>("")), toIngestionStatus((*sqlResultSet)[0][1].as<string>("null ingestion status!!!")),
			(*sqlResultSet)[0][2].as<json>(json())
		);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

pair<MMSEngineDBFacade::IngestionStatus, json>
MMSEngineDBFacade::ingestionJob_StatusMetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.status", "mms_ingestionjob:ij.metaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return make_pair(toIngestionStatus((*sqlResultSet)[0][0].as<string>("null ingestion status!!!")), (*sqlResultSet)[0][1].as<json>(json()));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

json MMSEngineDBFacade::ingestionJob_columnAsJson(int64_t workspaceKey, string columnName, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		string requestedColumn = fmt::format("mms_ingestionjob:ij.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<json>(json()) : json();
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

/*
json MMSEngineDBFacade::ingestionJob_MetadataContent(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.metaDataContent"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return (*sqlResultSet)[0][0].as<json>(json());
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}
*/

MMSEngineDBFacade::IngestionType MMSEngineDBFacade::ingestionJob_IngestionType(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.ingestionType"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return toIngestionType((*sqlResultSet)[0][0].as<string>(""));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

string MMSEngineDBFacade::ingestionJob_columnAsString(int64_t workspaceKey, string columnName, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		string requestedColumn = fmt::format("mms_ingestionjob:ij.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return (*sqlResultSet)[0][0].as<string>(string());
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

/*
string MMSEngineDBFacade::ingestionJob_Label(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.label"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}
*/

pair<MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus>
MMSEngineDBFacade::ingestionJob_IngestionTypeStatus(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.ingestionType", "mms_ingestionjob:ij.status"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);
		MMSEngineDBFacade::IngestionType ingestionType =
			MMSEngineDBFacade::toIngestionType((*sqlResultSet)[0][0].as<string>("null ingestion type!!!"));
		MMSEngineDBFacade::IngestionStatus ingestionStatus =
			MMSEngineDBFacade::toIngestionStatus((*sqlResultSet)[0][1].as<string>("null ingestion status!!!"));

		return make_pair(ingestionType, ingestionStatus);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

MMSEngineDBFacade::IngestionStatus MMSEngineDBFacade::ingestionJob_Status(int64_t workspaceKey, int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.status"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, ingestionJobKey, "", fromMaster);

		return MMSEngineDBFacade::toIngestionStatus((*sqlResultSet)[0][0].as<string>(""));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			workspaceKey, ingestionJobKey, fromMaster
		);

		throw e;
	}
}

void MMSEngineDBFacade::ingestionJob_IngestionJobKeys(int64_t workspaceKey, string label, bool fromMaster, vector<int64_t> &ingestionJobsKey)
{
	try
	{
		vector<string> requestedColumns = {"mms_ingestionjob:ij.ingestionJobKey"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = ingestionJobQuery(requestedColumns, workspaceKey, -1, label, fromMaster);

		for (auto row : *sqlResultSet)
			ingestionJobsKey.push_back(row[0].as<int64_t>((int64_t)-1));
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_WARN(
			"DBRecordNotFound"
			", workspaceKey: {}"
			", label: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, label, fromMaster, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", workspaceKey: {}"
			", label: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, label, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", label: {}"
			", fromMaster: {}",
			workspaceKey, label, fromMaster
		);

		throw e;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::ingestionJobQuery(
	vector<string> &requestedColumns,
	// 2021-02-20: workspaceKey is used just to be sure the ingestionJobKey
	//	will belong to the specified workspaceKey. We do that because the updateIngestionJob API
	//	calls this method, to be sure an end user can do an update of any IngestionJob (also
	//	belonging to another workspace)
	int64_t workspaceKey, int64_t ingestionJobKey, string label, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = fmt::format(
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
			string errorMessage = fmt::format(
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
			where += fmt::format("ir.ingestionRootKey = ij.ingestionRootKey and ir.workspaceKey = {} ", workspaceKey);
			if (ingestionJobKey != -1)
				where += fmt::format("{} ij.ingestionJobKey = {} ", where.size() > 0 ? "and" : "", ingestionJobKey);
			if (label != "")
				where += fmt::format("{} ij.label = {} ", where.size() > 0 ? "and" : "", trans.quote(label));

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = fmt::format("limit {} ", rows);
			if (startIndex != -1)
				offset = fmt::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = fmt::format("order by {} ", orderBy);

			string sqlStatement = fmt::format(
				"select {} "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (empty(res) && ingestionJobKey != -1 && notFoundAsException)
			{
				string errorMessage = fmt::format(
					"ingestionJob not found"
					", workspaceKey: {}"
					", ingestionJobKey: {}",
					workspaceKey, ingestionJobKey
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return sqlResultSet;
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
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

void MMSEngineDBFacade::addIngestionJobOutput(int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey)
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
		addIngestionJobOutput(conn, &trans, ingestionJobKey, mediaItemKey, physicalPathKey, sourceIngestionJobKey);

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

void MMSEngineDBFacade::addIngestionJobOutput(
	shared_ptr<PostgresConnection> conn, transaction_base *trans, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey,
	int64_t sourceIngestionJobKey
)
{
	try
	{
		/*
			2024-01-05:
			Il campo position è stato aggiunto perchè è importante mantenere l'ordine in cui addIngestionJobOutput
			viene chiamato. L'esempio che mi ha portato ad aggiungere il campo position è il seguente:
			in un workflow abbiamo un GroupOfTasks che esegue 3 tagli in parallelo. L'output del GroupOfTasks
			va in input ad un Concat che concatena i 3 tagli.
			Nel mio caso il concat non concatenava i tre tagli nel giusto ordine che è quello indicato
			dall'ordine dei Tasks del GroupOfTasks.
		*/
		int newPosition = 0;
		{
			string sqlStatement =
				fmt::format("select max(position) as newPosition from MMS_IngestionJobOutput where ingestionJobKey = {}", ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans->exec(sqlStatement);
			if (!empty(res))
			{
				if (!res[0]["newPosition"].is_null())
					newPosition = res[0]["newPosition"].as<int>() + 1;
			}

			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		{
			string sqlStatement = fmt::format(
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey, position) values ("
				"{}, {}, {}, {})",
				ingestionJobKey, mediaItemKey, physicalPathKey, newPosition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans->exec0(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		if (sourceIngestionJobKey != -1)
		{
			newPosition = 0;
			{
				string sqlStatement =
					fmt::format("select max(position) as newPosition from MMS_IngestionJobOutput where ingestionJobKey = {}", sourceIngestionJobKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				if (!empty(res))
				{
					if (!res[0]["newPosition"].is_null())
						newPosition = res[0]["newPosition"].as<int>() + 1;
				}

				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			string sqlStatement = fmt::format(
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey, position) values ("
				"{}, {}, {}, {})",
				sourceIngestionJobKey, mediaItemKey, physicalPathKey, newPosition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans->exec0(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

long MMSEngineDBFacade::getIngestionJobOutputsCount(int64_t ingestionJobKey, bool fromMaster)
{
	long ingestionJobOutputsCount = -1;

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
		{
			string sqlStatement = fmt::format("select count(*) from MMS_IngestionJobOutput where ingestionJobKey = {}", ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			ingestionJobOutputsCount = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return ingestionJobOutputsCount;
}

json MMSEngineDBFacade::getIngestionJobsStatus(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int start, int rows, string label, bool labelLike,
	/* bool startAndEndIngestionDatePresent, */
	string startIngestionDate, string endIngestionDate, string startScheduleDate, string ingestionType, string configurationLabel,
	string outputChannelLabel, int64_t recordingCode, bool broadcastIngestionJobKeyNotNull, string jsonParametersCondition, bool asc, string status,
	bool dependencyInfo,
	bool ingestionJobOutputs, // added because output could be thousands of entries
	bool fromMaster
)
{
	json statusListRoot;

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
		string field;

		{
			json requestParametersRoot;

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

			field = "labelLike";
			requestParametersRoot[field] = labelLike;

			field = "status";
			requestParametersRoot[field] = status;

			if (startIngestionDate != "")
			{
				field = "startIngestionDate";
				requestParametersRoot[field] = startIngestionDate;
			}
			if (endIngestionDate != "")
			{
				field = "endIngestionDate";
				requestParametersRoot[field] = endIngestionDate;
			}
			if (startScheduleDate != "")
			{
				field = "startScheduleDate";
				requestParametersRoot[field] = startScheduleDate;
			}

			if (ingestionType != "")
			{
				field = "ingestionType";
				requestParametersRoot[field] = ingestionType;
			}

			if (configurationLabel != "")
			{
				field = "configurationLabel";
				requestParametersRoot[field] = configurationLabel;
			}

			if (outputChannelLabel != "")
			{
				field = "outputChannelLabel";
				requestParametersRoot[field] = outputChannelLabel;
			}

			if (recordingCode != -1)
			{
				field = "recordingCode";
				requestParametersRoot[field] = recordingCode;
			}

			{
				field = "broadcastIngestionJobKeyNotNull";
				requestParametersRoot[field] = broadcastIngestionJobKeyNotNull;
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

		string sqlWhere = "where ir.ingestionRootKey = ij.ingestionRootKey ";
		sqlWhere += fmt::format("and ir.workspaceKey = {} ", workspace->_workspaceKey);
		if (ingestionJobKey != -1)
			sqlWhere += fmt::format("and ij.ingestionJobKey = {} ", ingestionJobKey);
		if (label != "")
		{
			// LOWER was used because the column is using utf8_bin that is case sensitive
			if (labelLike)
				sqlWhere += fmt::format("and LOWER(ij.label) like LOWER({}) ", trans.quote("%" + label + "%"));
			else
				sqlWhere += fmt::format("and LOWER(ij.label) = LOWER({}) ", trans.quote(label));
		}
		/*
		if (startAndEndIngestionDatePresent)
			sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate
		<= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		*/
		if (startIngestionDate != "")
			sqlWhere += fmt::format("and ir.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startIngestionDate));
		if (endIngestionDate != "")
			sqlWhere += fmt::format("and ir.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endIngestionDate));
		if (startScheduleDate != "")
			sqlWhere +=
				fmt::format("and ij.scheduleStart_virtual >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startScheduleDate));
		if (ingestionType != "")
			sqlWhere += fmt::format("and ij.ingestionType = {} ", trans.quote(ingestionType));
		if (configurationLabel != "")
			sqlWhere += fmt::format("and ij.configurationLabel_virtual = {} ", trans.quote(configurationLabel));
		if (outputChannelLabel != "")
			sqlWhere += fmt::format("and ij.outputChannelLabel_virtual = {} ", trans.quote(outputChannelLabel));
		if (recordingCode != -1)
			sqlWhere += fmt::format("and ij.recordingCode_virtual = {} ", recordingCode);
		if (broadcastIngestionJobKeyNotNull)
			sqlWhere += ("and ij.broadcastIngestionJobKey_virtual is not null ");
		if (jsonParametersCondition != "")
			sqlWhere += fmt::format("and {} ", jsonParametersCondition);
		if (status == "completed")
			sqlWhere += ("and ij.status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', "
						 "'SourceCopingInProgress', 'SourceUploadingInProgress', 'EncodingQueued') "); // like 'End_%' "
		else if (status == "notCompleted")
			sqlWhere += ("and ij.status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
						 "'SourceUploadingInProgress', 'EncodingQueued') "); // not like 'End_%' "

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		json ingestionJobsRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select ij.ingestionRootKey, ij.ingestionJobKey, ij.label, "
				"ij.ingestionType, ij.metaDataContent, ij.processorMMS, "
				"to_char(ij.processingStartingFrom, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as processingStartingFrom, "
				"to_char(ij.startProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as startProcessing, "
				"to_char(ij.endProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as endProcessing, "
				"case when ij.startProcessing IS NULL then NOW() at time zone 'utc' else ij.startProcessing end as newStartProcessing, "
				"case when ij.endProcessing IS NULL then NOW() at time zone 'utc' else ij.endProcessing end as newEndProcessing, "
				"ij.downloadingProgress, ij.uploadingProgress, "
				"ij.status, ij.errorMessage from MMS_IngestionRoot ir, MMS_IngestionJob ij {} "
				"order by newStartProcessing {}, newEndProcessing "
				"limit {} offset {}",
				sqlWhere, asc ? "asc" : "desc", rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			chrono::milliseconds internalSqlDuration(0);
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json ingestionJobRoot = getIngestionJobRoot(workspace, row, dependencyInfo, ingestionJobOutputs, conn, trans);
				internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);

				ingestionJobsRoot.push_back(ingestionJobRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
			);
		}

		field = "ingestionJobs";
		responseRoot[field] = ingestionJobsRoot;

		field = "response";
		statusListRoot[field] = responseRoot;

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

	return statusListRoot;
}

json MMSEngineDBFacade::getIngestionJobRoot(
	shared_ptr<Workspace> workspace, row &row,
	bool dependencyInfo,	  // added for performance issue
	bool ingestionJobOutputs, // added because output could be thousands of entries
	shared_ptr<PostgresConnection> conn, nontransaction &trans
)
{
	json ingestionJobRoot;

	try
	{
		int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();

		string field = "ingestionType";
		ingestionJobRoot[field] = row["ingestionType"].as<string>();
		IngestionType ingestionType = toIngestionType(row["ingestionType"].as<string>());

		field = "ingestionJobKey";
		ingestionJobRoot[field] = ingestionJobKey;

		field = "status";
		ingestionJobRoot[field] = row["status"].as<string>();
		IngestionStatus ingestionStatus = toIngestionStatus(row["status"].as<string>());

		field = "metaDataContent";
		ingestionJobRoot[field] = row["metaDataContent"].as<string>();

		field = "processorMMS";
		if (row["processorMMS"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
			ingestionJobRoot[field] = row["processorMMS"].as<string>();

		field = "label";
		if (row["label"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
			ingestionJobRoot[field] = row["label"].as<string>();

		if (dependencyInfo)
		{
			tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> ingestionJobToBeManagedInfo =
				isIngestionJobToBeManaged(ingestionJobKey, workspace->_workspaceKey, ingestionStatus, ingestionType, conn, &trans);

			bool ingestionJobToBeManaged;
			int64_t dependOnIngestionJobKey;
			int dependOnSuccess;
			IngestionStatus ingestionStatusDependency;

			tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency) = ingestionJobToBeManagedInfo;

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

		json mediaItemsRoot = json::array();
		if (ingestionJobOutputs)
		{
			string sqlStatement = fmt::format(
				"select mediaItemKey, physicalPathKey, position from MMS_IngestionJobOutput "
				"where ingestionJobKey = {} order by position",
				ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json mediaItemRoot;

				field = "mediaItemKey";
				int64_t mediaItemKey = row["mediaItemKey"].as<int64_t>();
				mediaItemRoot[field] = mediaItemKey;

				field = "physicalPathKey";
				int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();
				mediaItemRoot[field] = physicalPathKey;

				field = "position";
				mediaItemRoot[field] = row["position"].as<int>();

				mediaItemsRoot.push_back(mediaItemRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}
		field = "mediaItems";
		ingestionJobRoot[field] = mediaItemsRoot;

		field = "processingStartingFrom";
		ingestionJobRoot[field] = row["processingStartingFrom"].as<string>();

		field = "startProcessing";
		if (row["startProcessing"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
			ingestionJobRoot[field] = row["startProcessing"].as<string>();

		field = "endProcessing";
		if (row["endProcessing"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
			ingestionJobRoot[field] = row["endProcessing"].as<string>();

		// if (ingestionType == IngestionType::AddContent)
		{
			field = "downloadingProgress";
			if (row["downloadingProgress"].is_null())
				ingestionJobRoot[field] = nullptr;
			else
				ingestionJobRoot[field] = row["downloadingProgress"].as<float>();
		}

		// if (ingestionType == IngestionType::AddContent)
		{
			field = "uploadingProgress";
			if (row["uploadingProgress"].is_null())
				ingestionJobRoot[field] = nullptr;
			else
				ingestionJobRoot[field] = (int)row["uploadingProgress"].as<float>();
		}

		field = "ingestionRootKey";
		ingestionJobRoot[field] = row["ingestionRootKey"].as<int64_t>();

		field = "errorMessage";
		if (row["errorMessage"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
		{
			int maxErrorMessageLength = 2000;

			string errorMessage = row["errorMessage"].as<string>();
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
				ingestionJobRoot[field] = false;
			}
		}

		switch (ingestionType)
		{
		case IngestionType::Encode:
		case IngestionType::OverlayImageOnVideo:
		case IngestionType::OverlayTextOnVideo:
		case IngestionType::PeriodicalFrames:
		case IngestionType::IFrames:
		case IngestionType::MotionJPEGByPeriodicalFrames:
		case IngestionType::MotionJPEGByIFrames:
		case IngestionType::Slideshow:
		case IngestionType::FaceRecognition:
		case IngestionType::FaceIdentification:
		case IngestionType::LiveRecorder:
		case IngestionType::VideoSpeed:
		case IngestionType::PictureInPicture:
		case IngestionType::IntroOutroOverlay:
		case IngestionType::LiveProxy:
		case IngestionType::VODProxy:
		case IngestionType::LiveGrid:
		case IngestionType::Countdown:
		case IngestionType::Cut:
		case IngestionType::AddSilentAudio:

			// in case of LiveRecorder and HighAvailability true, we will have 2 encodingJobs, one for the main and one for the backup,
			//	we will take the main one. In case HighAvailability is false, we still have the main field set to true
			// 2021-05-12: HighAvailability true is not used anymore
			/*
			if (ingestionType == IngestionType::LiveRecorder)
				sqlStatement =
					"select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
					"DATE_FORMAT(convert_tz(encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
					"DATE_FORMAT(convert_tz(encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
					"processorMMS, encoderKey, encodingPid, failuresNumber from MMS_EncodingJob where ingestionJobKey = ? "
					"and JSON_EXTRACT(parameters, '$.main') = true "
					;
			else
			*/
			string sqlStatement = fmt::format(
				"select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
				"to_char(encodingJobStart, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobStart, "
				"to_char(encodingJobEnd, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobEnd, "
				"processorMMS, encoderKey, encodingPid, failuresNumber from MMS_EncodingJob "
				"where ingestionJobKey = {} ",
				ingestionJobKey
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
				json encodingJobRoot;

				int64_t encodingJobKey = res[0]["encodingJobKey"].as<int64_t>();

				field = "ownedByCurrentWorkspace";
				encodingJobRoot[field] = true;

				field = "encodingJobKey";
				encodingJobRoot[field] = encodingJobKey;

				field = "ingestionJobKey";
				encodingJobRoot[field] = ingestionJobKey;

				field = "type";
				encodingJobRoot[field] = res[0]["type"].as<string>();

				{
					string parameters = res[0]["parameters"].as<string>();

					json parametersRoot;
					if (parameters != "")
						parametersRoot = JSONUtils::toJson(parameters);

					field = "parameters";
					encodingJobRoot[field] = parametersRoot;
				}

				field = "status";
				encodingJobRoot[field] = res[0]["status"].as<string>();
				EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(res[0]["status"].as<string>());

				field = "encodingPriority";
				encodingJobRoot[field] = toString(static_cast<EncodingPriority>(res[0]["encodingPriority"].as<int>()));

				field = "encodingPriorityCode";
				encodingJobRoot[field] = res[0]["encodingPriority"].as<int>();

				field = "maxEncodingPriorityCode";
				encodingJobRoot[field] = workspace->_maxEncodingPriority;

				field = "progress";
				if (res[0]["encodingProgress"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = res[0]["encodingProgress"].as<float>();

				field = "start";
				if (encodingStatus == EncodingStatus::ToBeProcessed)
					encodingJobRoot[field] = nullptr;
				else
				{
					if (res[0]["encodingJobStart"].is_null())
						encodingJobRoot[field] = nullptr;
					else
						encodingJobRoot[field] = static_cast<string>(res[0]["encodingJobStart"].as<string>());
				}

				field = "end";
				if (res[0]["encodingJobEnd"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = res[0]["encodingJobEnd"].as<string>();

				field = "processorMMS";
				if (res[0]["processorMMS"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = res[0]["processorMMS"].as<string>();

				field = "encoderKey";
				if (res[0]["encoderKey"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = res[0]["encoderKey"].as<int64_t>();

				field = "encodingPid";
				if (res[0]["encodingPid"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = res[0]["encodingPid"].as<int64_t>();

				field = "failuresNumber";
				encodingJobRoot[field] = res[0]["failuresNumber"].as<int>();

				field = "encodingJob";
				ingestionJobRoot[field] = encodingJobRoot;
			}
		}
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

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}

	return ingestionJobRoot;
}

void MMSEngineDBFacade::checkWorkspaceStorageAndMaxIngestionNumber(int64_t workspaceKey)
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
		int maxIngestionsNumber;
		int currentIngestionsNumber;
		EncodingPeriod encodingPeriod;
		string periodUtcStartDateTime;
		string periodUtcEndDateTime;

		{
			string sqlStatement = fmt::format(
				"select c.maxIngestionsNumber, cmi.currentIngestionsNumber, c.encodingPeriod, "
				"to_char(cmi.startDateTime, 'YYYY-MM-DD HH24:MI:SS') as utcStartDateTime, "
				"to_char(cmi.endDateTime, 'YYYY-MM-DD HH24:MI:SS') as utcEndDateTime "
				"from MMS_Workspace c, MMS_WorkspaceMoreInfo cmi "
				"where c.workspaceKey = cmi.workspaceKey and c.workspaceKey = {}",
				workspaceKey
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
				maxIngestionsNumber = res[0]["maxIngestionsNumber"].as<int>();
				currentIngestionsNumber = res[0]["currentIngestionsNumber"].as<int>();
				encodingPeriod = toEncodingPeriod(res[0]["encodingPeriod"].as<string>());
				periodUtcStartDateTime = res[0]["utcStartDateTime"].as<string>();
				periodUtcEndDateTime = res[0]["utcEndDateTime"].as<string>();
			}
			else
			{
				string errorMessage = __FILEREF__ + "Workspace is not present/configured" + ", workspaceKey: " + to_string(workspaceKey) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			_logger->info(
				__FILEREF__ + "@SQL statistics@" + ", sqlStatement: " + sqlStatement + ", workspaceKey: " + to_string(workspaceKey) +
				", res.size: " + to_string(res.size()) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()) + "@"
			);
		}

		// check maxStorage first
		{
			int64_t workSpaceUsageInBytes;
			int64_t maxStorageInMB;

			pair<int64_t, int64_t> workSpaceUsageDetails = getWorkspaceUsage(conn, trans, workspaceKey);
			tie(workSpaceUsageInBytes, maxStorageInMB) = workSpaceUsageDetails;

			int64_t totalSizeInMB = workSpaceUsageInBytes / 1000000;
			if (totalSizeInMB >= maxStorageInMB)
			{
				string errorMessage = __FILEREF__ + "Reached the max storage dedicated for your Workspace" +
									  ", maxStorageInMB: " + to_string(maxStorageInMB) + ", totalSizeInMB: " + to_string(totalSizeInMB) +
									  ". It is needed to increase Workspace capacity.";
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool ingestionsAllowed = true;
		bool periodExpired = false;
		char newPeriodUtcStartDateTime[64];
		char newPeriodUtcEndDateTime[64];

		{
			char strUtcDateTimeNow[64];
			tm tmDateTimeNow;
			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcTimeNow = chrono::system_clock::to_time_t(now);
			gmtime_r(&utcTimeNow, &tmDateTimeNow);

			sprintf(
				strUtcDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, tmDateTimeNow.tm_mday,
				tmDateTimeNow.tm_hour, tmDateTimeNow.tm_min, tmDateTimeNow.tm_sec
			);

			if (periodUtcStartDateTime.compare(strUtcDateTimeNow) <= 0 && periodUtcEndDateTime.compare(strUtcDateTimeNow) >= 0)
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
					sprintf(
						newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
						tmDateTimeNow.tm_mday,
						0, // tmDateTimeNow. tm_hour,
						0, // tmDateTimeNow. tm_min,
						0  // tmDateTimeNow. tm_sec
					);
					sprintf(
						newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
						tmDateTimeNow.tm_mday,
						23, // tmCurrentDateTime. tm_hour,
						59, // tmCurrentDateTime. tm_min,
						59	// tmCurrentDateTime. tm_sec
					);
				}
				else if (encodingPeriod == EncodingPeriod::Weekly)
				{
					// from monday to sunday
					// monday
					{
						int daysToHavePreviousMonday;

						if (tmDateTimeNow.tm_wday == 0) // Sunday
							daysToHavePreviousMonday = 6;
						else
							daysToHavePreviousMonday = tmDateTimeNow.tm_wday - 1;

						chrono::system_clock::time_point mondayOfCurrentWeek;
						if (daysToHavePreviousMonday != 0)
						{
							chrono::duration<int, ratio<60 * 60 * 24>> days(daysToHavePreviousMonday);
							mondayOfCurrentWeek = now - days;
						}
						else
							mondayOfCurrentWeek = now;

						tm tmMondayOfCurrentWeek;
						time_t utcTimeMondayOfCurrentWeek = chrono::system_clock::to_time_t(mondayOfCurrentWeek);
						gmtime_r(&utcTimeMondayOfCurrentWeek, &tmMondayOfCurrentWeek);

						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmMondayOfCurrentWeek.tm_year + 1900,
							tmMondayOfCurrentWeek.tm_mon + 1, tmMondayOfCurrentWeek.tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
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
							chrono::duration<int, ratio<60 * 60 * 24>> days(daysToHaveNextSunday);
							sundayOfCurrentWeek = now + days;
						}
						else
							sundayOfCurrentWeek = now;

						tm tmSundayOfCurrentWeek;
						time_t utcTimeSundayOfCurrentWeek = chrono::system_clock::to_time_t(sundayOfCurrentWeek);
						gmtime_r(&utcTimeSundayOfCurrentWeek, &tmSundayOfCurrentWeek);

						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmSundayOfCurrentWeek.tm_year + 1900,
							tmSundayOfCurrentWeek.tm_mon + 1, tmSundayOfCurrentWeek.tm_mday,
							23, // tmSundayOfCurrentWeek. tm_hour,
							59, // tmSundayOfCurrentWeek. tm_min,
							59	// tmSundayOfCurrentWeek. tm_sec
						);
					}
				}
				else if (encodingPeriod == EncodingPeriod::Monthly)
				{
					// first day of the month
					{
						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
							1, // tmDateTimeNow. tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
							0  // tmDateTimeNow. tm_sec
						);
					}

					// last day of the month
					{
						tm tmLastDayOfCurrentMonth = tmDateTimeNow;

						tmLastDayOfCurrentMonth.tm_mday = 1;

						// Next month 0=Jan
						if (tmLastDayOfCurrentMonth.tm_mon == 11) // Dec
						{
							tmLastDayOfCurrentMonth.tm_mon = 0;
							tmLastDayOfCurrentMonth.tm_year++;
						}
						else
						{
							tmLastDayOfCurrentMonth.tm_mon++;
						}

						// Get the first day of the next month
						time_t utcTimeLastDayOfCurrentMonth = mktime(&tmLastDayOfCurrentMonth);

						// Subtract 1 day
						utcTimeLastDayOfCurrentMonth -= 86400;

						// Convert back to date and time
						gmtime_r(&utcTimeLastDayOfCurrentMonth, &tmLastDayOfCurrentMonth);

						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmLastDayOfCurrentMonth.tm_year + 1900,
							tmLastDayOfCurrentMonth.tm_mon + 1, tmLastDayOfCurrentMonth.tm_mday,
							23, // tmDateTimeNow. tm_hour,
							59, // tmDateTimeNow. tm_min,
							59	// tmDateTimeNow. tm_sec
						);
					}
				}
				else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
				{
					// first day of the year
					{
						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900,
							1, // tmDateTimeNow. tm_mon + 1,
							1, // tmDateTimeNow. tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
							0  // tmDateTimeNow. tm_sec
						);
					}

					// last day of the month
					{
						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900,
							12, // tmDateTimeNow. tm_mon + 1,
							31, // tmDateTimeNow. tm_mday,
							23, // tmDateTimeNow. tm_hour,
							59, // tmDateTimeNow. tm_min,
							59	// tmDateTimeNow. tm_sec
						);
					}
				}
			}
		}

		if (periodExpired)
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_WorkspaceMoreInfo set currentIngestionsNumber = 0, "
				"startDateTime = to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"endDateTime = to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS') "
				"where workspaceKey = {} returning 1) select count(*) from rows",
				trans.quote(newPeriodUtcStartDateTime), trans.quote(newPeriodUtcEndDateTime), workspaceKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done" + ", newPeriodUtcStartDateTime: " + newPeriodUtcStartDateTime +
									  ", newPeriodUtcEndDateTime: " + newPeriodUtcEndDateTime + ", workspaceKey: " + to_string(workspaceKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!ingestionsAllowed)
		{
			string errorMessage = __FILEREF__ + "Reached the max number of Ingestions in your period" +
								  ", maxIngestionsNumber: " + to_string(maxIngestionsNumber) + ", encodingPeriod: " + toString(encodingPeriod) +
								  ". It is needed to increase Workspace capacity.";
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
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

void MMSEngineDBFacade::fixIngestionJobsHavingWrongStatus()
{
	_logger->info(__FILEREF__ + "fixIngestionJobsHavingWrongStatus");

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		long totalRowsUpdated = 0;
		int maxRetriesOnError = 2;
		int currentRetriesOnError = 0;
		bool toBeExecutedAgain = true;
		while (toBeExecutedAgain)
		{
			try
			{
				// Scenarios: EncodingJob in final status but IngestionJob not in final status
				//	This is independently by the specific instance of mms-engine (because in this scenario
				//	often the processor field is empty) but someone has to do it
				//	This scenario may happen in case the mms-engine is shutdown not in friendly way
				int toleranceInHours = 4;
				string sqlStatement = fmt::format(
					"select ij.ingestionJobKey, ej.encodingJobKey, "
					"ij.status as ingestionJobStatus, ej.status as encodingJobStatus "
					"from MMS_IngestionJob ij, MMS_EncodingJob ej "
					"where ij.ingestionJobKey = ej.ingestionJobKey "
					// like non va bene per motivi di performance
					"and ij.status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
					"'SourceUploadingInProgress', 'EncodingQueued') "		// not like 'End_%' "
					"and ej.status not in ('ToBeProcessed', 'Processing') " // like 'End_%'"
					"and ej.encodingJobEnd < NOW() at time zone 'utc' - INTERVAL '{} hours'",
					toleranceInHours
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.exec(sqlStatement);
				for (auto row : res)
				{
					int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
					int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();
					string ingestionJobStatus = row["ingestionJobStatus"].as<string>();
					string encodingJobStatus = row["encodingJobStatus"].as<string>();

					{
						string errorMessage = string("Found IngestionJob having wrong status") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobStatus: " + ingestionJobStatus +
											  ", encodingJobStatus: " + encodingJobStatus;
						_logger->error(__FILEREF__ + errorMessage);

						updateIngestionJob(conn, &trans, ingestionJobKey, IngestionStatus::End_CanceledByMMS, errorMessage);

						totalRowsUpdated++;
					}
				}
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);

				toBeExecutedAgain = false;
			}
			catch (sql_error const &e)
			{
				// Deadlock!!!
				SPDLOG_ERROR(
					"SQL exception"
					", query: {}"
					", exceptionMessage: {}"
					", conn: {}",
					e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
				);

				currentRetriesOnError++;
				if (currentRetriesOnError >= maxRetriesOnError)
					throw e;

				{
					int secondsBetweenRetries = 15;
					_logger->info(
						__FILEREF__ + "fixIngestionJobsHavingWrongStatus failed, " + "waiting before to try again" +
						", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
						", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
		}

		_logger->info(__FILEREF__ + "fixIngestionJobsHavingWrongStatus " + ", totalRowsUpdated: " + to_string(totalRowsUpdated));

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

void MMSEngineDBFacade::updateIngestionJob_LiveRecorder(
	int64_t workspaceKey, int64_t ingestionJobKey, bool ingestionJobLabelModified, string newIngestionJobLabel, bool channelLabelModified,
	string newChannelLabel, bool recordingPeriodStartModified, string newRecordingPeriodStart, bool recordingPeriodEndModified,
	string newRecordingPeriodEnd, bool recordingVirtualVODModified, bool newRecordingVirtualVOD, bool admin
)
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
		if (ingestionJobLabelModified || channelLabelModified || recordingPeriodStartModified || recordingPeriodEndModified)
		{
			string setSQL;

			if (ingestionJobLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("label = {}", trans.quote(newIngestionJobLabel));
			}

			if (channelLabelModified || recordingPeriodStartModified || recordingPeriodEndModified ||
				(recordingVirtualVODModified && newRecordingVirtualVOD))
			{
				if (setSQL != "")
					setSQL += ", ";

				setSQL += "metaDataContent = ";
				if (recordingPeriodStartModified)
					setSQL += "jsonb_set(";
				if (recordingPeriodEndModified)
					setSQL += "jsonb_set(";
				if (recordingVirtualVODModified && newRecordingVirtualVOD)
					setSQL += "jsonb_set(";
				if (channelLabelModified)
					setSQL += fmt::format("jsonb_set(metaDataContent, {{configurationLabel}}, jsonb {})", trans.quote("\"" + newChannelLabel + "\""));
				if (recordingVirtualVODModified && newRecordingVirtualVOD)
					setSQL += fmt::format(", '{{liveRecorderVirtualVOD}}', json '{}')", trans.quote("\"" + newRecordingPeriodStart + "\""));
				if (recordingPeriodEndModified)
					setSQL += fmt::format(", {{schedule,end}}, jsonb {})", trans.quote("\"" + newRecordingPeriodEnd + "\""));
				if (recordingPeriodStartModified)
					setSQL += fmt::format(", {{schedule,start}}, jsonb {})", trans.quote("\"" + newRecordingPeriodStart + "\""));
			}

			/*
			if (channelLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("metaDataContent = jsonb_set(metaDataContent, {{configurationLabel}}, jsonb {})",
					trans.quote("\"" + newChannelLabel + "\""));
			}

			if (recordingPeriodStartModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("metaDataContent = jsonb_set(metaDataContent, {{schedule,start}}, jsonb {})",
					trans.quote("\"" + newRecordingPeriodStart + "\""));
			}

			if (recordingPeriodEndModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("metaDataContent = jsonb_set(metaDataContent, {{schedule,end}}, jsonb {})",
					trans.quote("\"" + newRecordingPeriodEnd + "\""));
			}

			if (recordingVirtualVODModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				if (newRecordingVirtualVOD)
					setSQL += "metaDataContent = jsonb_set(metaDataContent, '{{liveRecorderVirtualVOD}}', json '{}')";
				else
					setSQL += "metaDataContent = metaDataContent - 'liveRecorderVirtualVOD'";
			}
			*/

			setSQL = "set " + setSQL + " ";

			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				setSQL, ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
			if (recordingVirtualVODModified && !newRecordingVirtualVOD)
			{
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_IngestionJob "
					"set metaDataContent = metaDataContent - 'liveRecorderVirtualVOD' "
					"where ingestionJobKey = {} returning 1) select count(*) from rows",
					ingestionJobKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
				SPDLOG_INFO(
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
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
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
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
