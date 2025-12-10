
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "StringUtils.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <utility>

void MMSEngineDBFacade::getIngestionsToBeManaged(
	vector<tuple<int64_t, string, shared_ptr<Workspace>, string, string, IngestionType, IngestionStatus>> &ingestionsToBeManaged, string processorMMS,
	int maxIngestionJobs, int timeBeforeToPrepareResourcesInMinutes, bool onlyTasksNotInvolvingMMSEngineThreads
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();
		if (startPoint - _lastConnectionStatsReport >= chrono::seconds(_dbConnectionPoolStatsReportPeriodInSeconds))
		{
			_lastConnectionStatsReport = chrono::system_clock::now();

			DBConnectionPoolStats dbConnectionPoolStats = _masterPostgresConnectionPool->get_stats();

			SPDLOG_INFO(
				"DB connection pool stats"
				", _poolSize: {}"
				", _borrowedSize: {}",
				dbConnectionPoolStats._poolSize, dbConnectionPoolStats._borrowedSize
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

			SPDLOG_INFO(
				"getIngestionsToBeManaged"
				", initialGetIngestionJobsCurrentIndex: {}",
				initialGetIngestionJobsCurrentIndex
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
				string sqlStatement = std::format("select ij.ingestionRootKey, ij.ingestionJobKey, ij.label, "
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
					sqlStatement += std::format("and ij.ingestionType in ({}) ", tasksNotInvolvingMMSEngineThreadsList);
				}
				sqlStatement += std::format(
					// "and (ij.status = {} "
					// "or (ij.status in ({}, {}, {}, {}) and ij.sourceBinaryTransferred = true)) "
					"and toBeManaged_virtual "
					"and ij.processingStartingFrom <= NOW() at time zone 'utc' "
					"and NOW() at time zone 'utc' <= ij.processingStartingFrom + INTERVAL '{} days' "
					"and scheduleStart_virtual < (NOW() at time zone 'utc' + INTERVAL '{} minutes') "
					"order by ij.priority asc, ij.processingStartingFrom asc "
					"limit {} offset {} for update skip locked",
					// trans.transaction->quote(toString(IngestionStatus::Start_TaskQueued)),
					// trans.transaction->quote(toString(IngestionStatus::SourceDownloadingInProgress)),
					// trans.transaction->quote(toString(IngestionStatus::SourceMovingInProgress)),
					// trans.transaction->quote(toString(IngestionStatus::SourceCopingInProgress)),
					// trans.transaction->quote(toString(IngestionStatus::SourceUploadingInProgress)),
					_doNotManageIngestionsOlderThanDays, timeBeforeToPrepareResourcesInMinutes, mysqlRowCount, _getIngestionJobsCurrentIndex
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				chrono::milliseconds internalSqlDuration(0);
				result res = trans.transaction->exec(sqlStatement);
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

					SPDLOG_INFO(
						"getIngestionsToBeManaged (result set loop)"
						", ingestionJobKey: {}"
						", ingestionJobLabel: {}"
						", initialGetIngestionJobsCurrentIndex: {}"
						", resultSetIndex: {}/{}",
						ingestionJobKey, ingestionJobLabel, initialGetIngestionJobsCurrentIndex, resultSetIndex, res.size()
					);
					resultSetIndex++;

					auto [workspaceKey, ingestionDate] = workflowQuery_WorkspaceKeyIngestionDate(ingestionRootKey, false);

					tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> ingestionJobToBeManagedInfo =
						isIngestionJobToBeManaged(ingestionJobKey, workspaceKey, ingestionStatus, ingestionType, trans);

					bool ingestionJobToBeManaged;
					int64_t dependOnIngestionJobKey;
					int dependOnSuccess;
					IngestionStatus ingestionStatusDependency;

					tie(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency) = ingestionJobToBeManagedInfo;

					if (ingestionJobToBeManaged)
					{
						SPDLOG_INFO(
							"Adding jobs to be processed"
							", ingestionJobKey: {}"
							", ingestionStatus: {}",
							ingestionJobKey, toString(ingestionStatus)
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
						SPDLOG_INFO(
							"Ingestion job cannot be processed"
							", ingestionJobKey: {}"
							", ingestionStatus: {}"
							", dependOnIngestionJobKey: {}"
							", dependOnSuccess: {}"
							", ingestionStatusDependency: {}",
							ingestionJobKey, toString(ingestionStatus), dependOnIngestionJobKey, dependOnSuccess, toString(ingestionStatusDependency)
						);
					}
				}
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				SQLQUERYLOG(
					"getIngestionsToBeManaged", elapsed,
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@getIngestionsToBeManaged@",
					sqlStatement, trans.connection->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
				);

				SPDLOG_INFO(
					"getIngestionsToBeManaged"
					", _getIngestionJobsCurrentIndex: {}"
					", res.size: {}"
					", ingestionsToBeManaged.size(): {}"
					", moreRows: {}",
					_getIngestionJobsCurrentIndex, res.size(), ingestionsToBeManaged.size(), moreRows
				);

				if (res.size() == 0)
					_getIngestionJobsCurrentIndex = 0;
			}
			// if (ingestionsToBeManaged.size() < maxIngestionJobs)
			// 	_getIngestionJobsCurrentIndex = 0;

			pointAfterNotLive = chrono::system_clock::now();

			SPDLOG_INFO(
				"getIngestionsToBeManaged (exit)"
				", _getIngestionJobsCurrentIndex: {}"
				", ingestionsToBeManaged.size(): {}"
				", moreRows: {}"
				", onlyTasksNotInvolvingMMSEngineThreads: {}"
				", select not live elapsed (millisecs): {}",
				_getIngestionJobsCurrentIndex, ingestionsToBeManaged.size(), moreRows, onlyTasksNotInvolvingMMSEngineThreads,
				chrono::duration_cast<chrono::milliseconds>(pointAfterNotLive - pointAfterLive).count()
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

				sqlStatement = std::format(
					"update MMS_IngestionJob set processorMMS = {} "
					"where ingestionJobKey = {} ",
					trans.transaction->quote(processorMMS), ingestionJobKey
				);
			}
			else
			{
				sqlStatement = std::format(
					"update MMS_IngestionJob set startProcessing = NOW() at time zone 'utc', "
					"processorMMS = {} where ingestionJobKey = {} ",
					trans.transaction->quote(processorMMS), ingestionJobKey
				);
			}
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
					", processorMMS: {}"
					", ingestionJobKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					processorMMS, ingestionJobKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		SPDLOG_INFO(
			"getIngestionsToBeManaged statistics"
			", total elapsed (millisecs): {}"
			", select live elapsed (millisecs): {}"
			", select not live elapsed (millisecs): {}"
			", processing entries elapsed (millisecs): {}"
			", ingestionsToBeManaged.size: {}"
			", maxIngestionJobs: {}"
			", liveProxyToBeIngested: {}"
			", liveRecorderToBeIngested: "
			", othersToBeIngested: {}",
			chrono::duration_cast<chrono::milliseconds>(endPoint - startPoint).count(),
			chrono::duration_cast<chrono::milliseconds>(pointAfterLive - startPoint).count(),
			chrono::duration_cast<chrono::milliseconds>(pointAfterNotLive - pointAfterLive).count(),
			chrono::duration_cast<chrono::milliseconds>(endPoint - pointAfterNotLive).count(), ingestionsToBeManaged.size(), maxIngestionJobs,
			liveProxyToBeIngested, liveRecorderToBeIngested, othersToBeIngested
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

tuple<bool, int64_t, int, MMSEngineDBFacade::IngestionStatus> MMSEngineDBFacade::isIngestionJobToBeManaged(
	int64_t ingestionJobKey, int64_t workspaceKey, IngestionStatus ingestionStatus, IngestionType ingestionType, PostgresConnTrans &trans,
	chrono::milliseconds *sqlDuration
)
{
	bool ingestionJobToBeManaged = true;
	int64_t dependOnIngestionJobKey = -1;
	int dependOnSuccess = -1;
	IngestionStatus ingestionStatusDependency;

	try
	{
		bool atLeastOneDependencyRowFound = false;

		string sqlStatement = std::format(
			"select dependOnIngestionJobKey, dependOnSuccess "
			"from MMS_IngestionJobDependency "
			"where ingestionJobKey = {} order by orderNumber asc",
			ingestionJobKey
		);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans.transaction->exec(sqlStatement);
		// 2019-10-05: GroupOfTasks has always to be executed once the dependencies are in a final state.
		//	This is because the manageIngestionJobStatusUpdate do not update the GroupOfTasks with a state
		//	like End_NotToBeExecuted
		chrono::milliseconds internalSqlDuration(0);
		for (auto row : res)
		{
			if (!atLeastOneDependencyRowFound)
				atLeastOneDependencyRowFound = true;

			if (!row["dependOnIngestionJobKey"].is_null())
			{
				dependOnIngestionJobKey = row["dependOnIngestionJobKey"].as<int64_t>();
				dependOnSuccess = row["dependOnSuccess"].as<int>();

				string sqlStatement = std::format("select status from MMS_IngestionJob where ingestionJobKey = {}", dependOnIngestionJobKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				internalSqlDuration += chrono::milliseconds(elapsed);
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
					string sStatus = res[0]["status"].as<string>();

					// info(__FILEREF__ + "Dependency for the IngestionJob"
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
					SPDLOG_INFO(
						"Dependency for the IngestionJob"
						", ingestionJobKey: {}"
						", dependOnIngestionJobKey: {}"
						", dependOnSuccess: {}"
						", status: no row",
						ingestionJobKey, dependOnIngestionJobKey, dependOnSuccess
					);
				}
				/*
				SPDLOG_INFO(
					"@SQL statistics@"
					", sqlStatement: {}"
					", dependOnIngestionJobKey: {}"
					", res.size: {}"
					", elapsed (millisecs): @{}@",
					sqlStatement, dependOnIngestionJobKey, res.size(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				*/
			}
			else
			{
				SPDLOG_INFO(
					"Dependency for the IngestionJob"
					", ingestionJobKey: {}"
					", dependOnIngestionJobKey: null",
					ingestionJobKey
				);
			}
		}
		long elapsed = chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count();
		if (sqlDuration != nullptr)
			*sqlDuration = chrono::milliseconds(elapsed);
		SQLQUERYLOG(
			"default", elapsed,
			"SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, trans.connection->getConnectionId(), elapsed
		);

		if (!atLeastOneDependencyRowFound)
		{
			// this is not possible, even an ingestionJob without dependency has a row
			// (with dependOnIngestionJobKey NULL)

			SPDLOG_ERROR(
				"No dependency Row for the IngestionJob"
				", ingestionJobKey: {}"
				", workspaceKey: {}"
				", ingestionStatus: {}"
				", ingestionType: {}",
				ingestionJobKey, workspaceKey, static_cast<int>(ingestionStatus), static_cast<int>(ingestionType)
			);
			ingestionJobToBeManaged = false;
		}

		return make_tuple(ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency);
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

int64_t MMSEngineDBFacade::addIngestionJob(
	PostgresConnTrans &trans, int64_t workspaceKey, int64_t ingestionRootKey, string label, string metadataContent,
	MMSEngineDBFacade::IngestionType ingestionType, string processingStartingFrom, vector<int64_t> dependOnIngestionJobKeys, int dependOnSuccess,
	vector<int64_t> waitForGlobalIngestionJobKeys
)
{
	int64_t ingestionJobKey;

	try
	{
		{
			string sqlStatement = std::format("select c.enabled, c.workspaceType from MMS_Workspace c where c.workspaceKey = {}", workspaceKey);
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
				bool enabled = res[0]["enabled"].as<bool>();
				int workspaceType = res[0]["workspaceType"].as<int>();

				if (!enabled)
				{
					string errorMessage = std::format(
						"Workspace is not enabled"
						", workspaceKey: {}"
						", sqlStatement: {}",
						workspaceKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				else if (workspaceType != static_cast<int>(WorkspaceType::IngestionAndDelivery) &&
						 workspaceType != static_cast<int>(WorkspaceType::EncodingOnly))
				{
					string errorMessage = std::format(
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
				string errorMessage = std::format(
					"Workspace is not present/configured"
					", workspaceKey: {}"
					", sqlStatement: {}",
					workspaceKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		IngestionStatus ingestionStatus;
		string errorMessage;
		{
			{
				string sqlStatement = std::format(
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
					ingestionRootKey, trans.transaction->quote(label), trans.transaction->quote(metadataContent),
					trans.transaction->quote(toString(ingestionType)), getIngestionTypePriority(ingestionType),
					trans.transaction->quote(processingStartingFrom),
					trans.transaction->quote(toString(MMSEngineDBFacade::IngestionStatus::Start_TaskQueued))
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				ingestionJobKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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
				int orderNumber = 0;
				bool referenceOutputDependency = false;
				if (dependOnIngestionJobKeys.size() == 0)
				{
					addIngestionJobDependency(trans, ingestionJobKey, dependOnSuccess, -1, orderNumber, referenceOutputDependency);
					orderNumber++;
				}
				else
				{
					for (int64_t dependOnIngestionJobKey : dependOnIngestionJobKeys)
					{
						addIngestionJobDependency(
							trans, ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, orderNumber, referenceOutputDependency
						);

						orderNumber++;
					}
				}

				{
					int waitForDependOnSuccess = -1; // OnComplete
					for (int64_t dependOnIngestionJobKey : waitForGlobalIngestionJobKeys)
					{
						addIngestionJobDependency(
							trans, ingestionJobKey, waitForDependOnSuccess, dependOnIngestionJobKey, orderNumber, referenceOutputDependency
						);

						orderNumber++;
					}
				}
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
	PostgresConnTrans &trans, int64_t ingestionJobKey, int dependOnSuccess, int64_t dependOnIngestionJobKey, int orderNumber,
	bool referenceOutputDependency
)
{
	try
	{
		int localOrderNumber = 0;
		if (orderNumber == -1)
		{
			string sqlStatement = std::format(
				"select max(orderNumber) as maxOrderNumber from MMS_IngestionJobDependency "
				"where ingestionJobKey = {}",
				ingestionJobKey
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
				if (res[0]["maxOrderNumber"].is_null())
					localOrderNumber = 0;
				else
					localOrderNumber = res[0]["maxOrderNumber"].as<int>() + 1;
			}
		}
		else
		{
			localOrderNumber = orderNumber;
		}

		{
			string sqlStatement = std::format(
				"insert into MMS_IngestionJobDependency (ingestionJobDependencyKey,"
				"ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey, "
				"orderNumber, referenceOutputDependency) values ("
				"DEFAULT, {}, {}, {}, {}, {})",
				ingestionJobKey, dependOnSuccess, dependOnIngestionJobKey == -1 ? "null" : to_string(dependOnIngestionJobKey), localOrderNumber,
				referenceOutputDependency ? 1 : 0
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

		throw;
	}
}

void MMSEngineDBFacade::changeIngestionJobDependency(int64_t previousDependOnIngestionJobKey, int64_t newDependOnIngestionJobKey)
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
			string sqlStatement = std::format(
				"update MMS_IngestionJobDependency set dependOnIngestionJobKey = {} "
				"where dependOnIngestionJobKey = {} ",
				newDependOnIngestionJobKey, previousDependOnIngestionJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", newDependOnIngestionJobKey: " + to_string(newDependOnIngestionJobKey) +
									  ", previousDependOnIngestionJobKey: " + to_string(previousDependOnIngestionJobKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
		}

		SPDLOG_INFO(
			"MMS_IngestionJobDependency updated successful"
			", newDependOnIngestionJobKey: {}"
			", previousDependOnIngestionJobKey: {}",
			newDependOnIngestionJobKey, previousDependOnIngestionJobKey
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

void MMSEngineDBFacade::updateIngestionJobMetadataContent(int64_t ingestionJobKey, string metadataContent)
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
		updateIngestionJobMetadataContent(trans, ingestionJobKey, metadataContent);
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

void MMSEngineDBFacade::updateIngestionJobMetadataContent(PostgresConnTrans &trans, int64_t ingestionJobKey, string metadataContent)
{
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_IngestionJob set metadataContent = {} "
				"where ingestionJobKey = {} ",
				trans.transaction->quote(metadataContent), ingestionJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", metadataContent: " + metadataContent +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
		}

		SPDLOG_INFO(
			"IngestionJob updated successful"
			", metadataContent: {}"
			", ingestionJobKey: {}",
			metadataContent, ingestionJobKey
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

		throw;
	}
}

void MMSEngineDBFacade::updateIngestionJobParentGroupOfTasks(
	PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t parentGroupOfTasksIngestionJobKey
)
{
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_IngestionJob set parentGroupOfTasksIngestionJobKey = {} "
				"where ingestionJobKey = {} ",
				parentGroupOfTasksIngestionJobKey, ingestionJobKey
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
					", parentGroupOfTasksIngestionJobKey: {}"
					", ingestionJobKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					parentGroupOfTasksIngestionJobKey, ingestionJobKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		SPDLOG_INFO(
			"IngestionJob updated successful"
			", parentGroupOfTasksIngestionJobKey: {}"
			", ingestionJobKey: {}",
			parentGroupOfTasksIngestionJobKey, ingestionJobKey
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

		throw;
	}
}

void MMSEngineDBFacade::updateIngestionJob(int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage, string processorMMS)
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
		updateIngestionJob(trans, ingestionJobKey, newIngestionStatus, errorMessage, processorMMS);
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

void MMSEngineDBFacade::updateIngestionJob(
	PostgresConnTrans &trans, int64_t ingestionJobKey, IngestionStatus newIngestionStatus, string errorMessage, string processorMMS
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
						processorMMSUpdate = std::format("processorMMS = null, ");
					else
						processorMMSUpdate = std::format("processorMMS = {}, ", trans.transaction->quote(processorMMS));
				}

				if (errorMessageForSQL == "")
					sqlStatement = std::format(
						"update MMS_IngestionJob set status = {}, "
						"{} endProcessing = NOW() at time zone 'utc' "
						"where ingestionJobKey = {} ",
						trans.transaction->quote(toString(newIngestionStatus)), processorMMSUpdate, ingestionJobKey
					);
				else
					sqlStatement = std::format(
						"update MMS_IngestionJob set status = {}, "
						"errorMessage = SUBSTRING({} || '\n' || coalesce(errorMessage, ''), 1, 1024 * 20), "
						"{} endProcessing = NOW() at time zone 'utc' "
						"where ingestionJobKey = {} ",
						trans.transaction->quote(toString(newIngestionStatus)), trans.transaction->quote(errorMessageForSQL), processorMMSUpdate,
						ingestionJobKey
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
					string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
										  ", errorMessageForSQL: " + errorMessageForSQL + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					SPDLOG_ERROR(errorMessage);

					// it is not so important to block the continuation of this method
					// Also the exception caused a crash of the process
					// throw runtime_error(errorMessage);
				}
				*/
			}
			else
			{
				string sqlStatement;
				string processorMMSUpdate;
				if (processorMMS != "noToBeUpdated")
				{
					if (processorMMS == "")
						processorMMSUpdate = std::format(", processorMMS = null ");
					else
						processorMMSUpdate = std::format(", processorMMS = {} ", trans.transaction->quote(processorMMS));
				}

				if (errorMessageForSQL == "")
					sqlStatement = std::format(
						"update MMS_IngestionJob set status = {} "
						"{} "
						"where ingestionJobKey = {} ",
						trans.transaction->quote(toString(newIngestionStatus)), processorMMSUpdate, ingestionJobKey
					);
				else
					sqlStatement = std::format(
						"update MMS_IngestionJob set status = {}, "
						"errorMessage = SUBSTRING({} || '\n' || coalesce(errorMessage, ''), 1, 1024 * 20) "
						"{} "
						"where ingestionJobKey = {} ",
						trans.transaction->quote(toString(newIngestionStatus)), trans.transaction->quote(errorMessageForSQL), processorMMSUpdate,
						ingestionJobKey
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
					string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
										  ", errorMessageForSQL: " + errorMessageForSQL + ", ingestionJobKey: " + to_string(ingestionJobKey) +
										  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
					SPDLOG_ERROR(errorMessage);

					// it is not so important to block the continuation of this method
					// Also the exception caused a crash of the process
					// throw runtime_error(errorMessage);
				}
				*/
			}

			bool updateIngestionRootStatus = true;
			manageIngestionJobStatusUpdate(ingestionJobKey, newIngestionStatus, updateIngestionRootStatus, trans);

			SPDLOG_INFO(
				"IngestionJob updated successful"
				", newIngestionStatus: {}"
				", ingestionJobKey: {}"
				", processorMMS: {}",
				toString(newIngestionStatus), ingestionJobKey, processorMMS
			);

			updateToBeTriedAgain = false;
		}
		catch (exception const &e)
		{
			sql_error const *se = dynamic_cast<sql_error const *>(&e);
			if (se != nullptr)
			{
				// in caso di Postgres non so ancora la parola da cercare nell'errore che indica un deadlock,
				// per cui forzo un retry in attesa di avere l'errore e gestire meglio
				// if (exceptionMessage.find("Deadlock") != string::npos
				// 	&& retriesNumber < maxRetriesNumber)
				SPDLOG_WARN(
					"SQL exception (Deadlock), waiting before to try again"
					", ingestionJobKey: {}"
					", sqlStatement: {}"
					", exceptionMessage: {}"
					", retriesNumber: {}"
					", maxRetriesNumber: {}"
					", secondsBetweenRetries: {}",
					ingestionJobKey, se->query(), e.what(), retriesNumber, maxRetriesNumber, secondsBetweenRetries
				);

				this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
			}
			else
			{
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

void MMSEngineDBFacade::appendIngestionJobErrorMessages(int64_t ingestionJobKey, const json& newErrorMessagesRoot)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		/*
		{
			string strToBeReplaced = "FFMpeg";
			string strToReplace = "XXX";
			if (errorMessageForSQL.find(strToBeReplaced) != string::npos)
				errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		}
		*/

		string newErrorMessages = getPostgresArray(newErrorMessagesRoot, true, trans);
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"UPDATE MMS_IngestionJob "
				"SET errorMessages = COALESCE(errorMessages, ARRAY[]::text[]) || {} "
				"where ingestionJobKey = {} "
				"and status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') ", // not like 'End_%' "
				newErrorMessages, ingestionJobKey
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
			// if (rowsUpdated != 1)
			// {
			//	string errorMessage = __FILEREF__ + "no update was done" + ", errorMessageForSQL: " + errorMessageForSQL +
			//						  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
			//						  ", sqlStatement: " + sqlStatement;
			//	warn(errorMessage);

				// throw runtime_exception(errorMessage);
			// }
			// else
			// {
			//	info(__FILEREF__ + "IngestionJob updated successful" + ", ingestionJobKey: " + to_string(ingestionJobKey));
			// }
		}
	}
	catch (exception const &e)
	{
		auto const *se = dynamic_cast<sql_error const *>(&e);
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
/*
void MMSEngineDBFacade::updateIngestionJobErrorMessages(int64_t ingestionJobKey, string errorMessages)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// string errorMessageForSQL;
		// if (errorMessage.length() >= 1024)
		// 	errorMessageForSQL = errorMessage.substr(0, 1024);
		// else
		// 	errorMessageForSQL = errorMessage;
		// {
		// 	string strToBeReplaced = "FFMpeg";
		// 	string strToReplace = "XXX";
		// 	if (errorMessageForSQL.find(strToBeReplaced) != string::npos)
		// 		errorMessageForSQL.replace(errorMessageForSQL.find(strToBeReplaced), strToBeReplaced.length(), strToReplace);
		// }
		// if (errorMessageForSQL != "")
		{
			string errorMessagesForSQL = StringUtils::replaceAll(errorMessages, "FFMpeg", "XXX");

			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"update MMS_IngestionJob "
				"set errorMessage = {} "
				"where ingestionJobKey = {} "
				"and status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') ", // not like 'End_%' "
				trans.transaction->quote(errorMessagesForSQL), ingestionJobKey
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
			// if (rowsUpdated != 1)
			// {
			// 	string errorMessage = __FILEREF__ + "no update was done" + ", errorMessageForSQL: " + errorMessageForSQL +
			// 						  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
			// 						  ", sqlStatement: " + sqlStatement;
			// 	warn(errorMessage);

				// throw runtime_exception(errorMessage);
			// }
			// else
			// {
			// 	info(__FILEREF__ + "IngestionJob updated successful" + ", ingestionJobKey: " + to_string(ingestionJobKey));
			// }
		}
	}
	catch (exception const &e)
	{
		auto se = dynamic_cast<sql_error const *>(&e);
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
*/

void MMSEngineDBFacade::manageIngestionJobStatusUpdate(
	int64_t ingestionJobKey, IngestionStatus newIngestionStatus, bool updateIngestionRootStatus, PostgresConnTrans &trans
)
{
	try
	{
		SPDLOG_INFO(
			"manageIngestionJobStatusUpdate"
			", ingestionJobKey: {}"
			", newIngestionStatus: {}"
			", updateIngestionRootStatus: {}",
			ingestionJobKey, toString(newIngestionStatus), updateIngestionRootStatus
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
					// error(__FILEREF__ + "select"
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
						sqlStatement = std::format(
							"select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey {} and ijd.dependOnSuccess = {}",
							ingestionJobKeysToFindDependencies.find(",") != string::npos ? std::format("in ({})", ingestionJobKeysToFindDependencies)
																						 : std::format("= {}", ingestionJobKeysToFindDependencies),
							dependOnSuccess
						);
					}
					else
					{
						sqlStatement = std::format(
							"select ijd.ingestionJobKey "
							"from MMS_IngestionJob ij, MMS_IngestionJobDependency ijd where "
							"ij.ingestionJobKey = ijd.ingestionJobKey and ij.ingestionType != 'GroupOfTasks' "
							"and ijd.referenceOutputDependency != 1 "
							"and ijd.dependOnIngestionJobKey {}",
							ingestionJobKeysToFindDependencies.find(",") != string::npos ? std::format("in ({})", ingestionJobKeysToFindDependencies)
																						 : std::format("= {}", ingestionJobKeysToFindDependencies)
						);
					}
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					bool dependenciesFound = false;
					ingestionJobKeysToFindDependencies = "";
					for (auto row : res)
					{
						dependenciesFound = true;

						if (hierarchicalIngestionJobKeysDependencies == "")
							hierarchicalIngestionJobKeysDependencies = to_string(row["ingestionJobKey"].as<int64_t>());
						else
							hierarchicalIngestionJobKeysDependencies += std::format(", {}", row["ingestionJobKey"].as<int64_t>());

						if (ingestionJobKeysToFindDependencies == "")
							ingestionJobKeysToFindDependencies = to_string(row["ingestionJobKey"].as<int64_t>());
						else
							ingestionJobKeysToFindDependencies += std::format(", {}", row["ingestionJobKey"].as<int64_t>());
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

					// info(__FILEREF__ + "select result"
					// 	+ ", hierarchicalLevelIndex: " + to_string(hierarchicalLevelIndex)
					// 	+ ", hierarchicalIngestionJobKeysDependencies: " + hierarchicalIngestionJobKeysDependencies
					// );

					if (!dependenciesFound)
					{
						SPDLOG_INFO(
							"Finished to find dependencies"
							", hierarchicalLevelIndex: {}"
							", maxHierarchicalLevelsManaged: {}"
							", ingestionJobKey: {}",
							hierarchicalLevelIndex, maxHierarchicalLevelsManaged, ingestionJobKey
						);

						break;
					}
					else if (dependenciesFound && hierarchicalLevelIndex + 1 >= maxHierarchicalLevelsManaged)
					{
						SPDLOG_WARN(
							"after maxHierarchicalLevelsManaged we still found dependencies, so maxHierarchicalLevelsManaged has to be increased"
							", maxHierarchicalLevelsManaged: {}"
							", ingestionJobKey: {}"
							", sqlStatement: {}",
							maxHierarchicalLevelsManaged, ingestionJobKey, sqlStatement
						);
					}
				}
			}

			if (hierarchicalIngestionJobKeysDependencies != "")
			{
				SPDLOG_INFO(
					"manageIngestionJobStatusUpdate. update"
					", status: End_NotToBeExecuted"
					", ingestionJobKey: {}"
					", hierarchicalIngestionJobKeysDependencies: {}",
					ingestionJobKey, hierarchicalIngestionJobKeysDependencies
				);

				string sqlStatement = std::format(
					"update MMS_IngestionJob set status = {}, "
					"startProcessing = case when startProcessing IS NULL then NOW() at time zone 'utc' else startProcessing end, "
					"endProcessing = NOW() at time zone 'utc' where ingestionJobKey {} ",
					trans.transaction->quote(toString(IngestionStatus::End_NotToBeExecuted)),
					hierarchicalIngestionJobKeysDependencies.find(",") != string::npos
						? std::format("in ({})", hierarchicalIngestionJobKeysDependencies)
						: std::format("= {}", hierarchicalIngestionJobKeysDependencies)
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.transaction->exec0(sqlStatement);
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				SQLQUERYLOG(
					"manageIngestionJobStatusUpdate", elapsed,
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@manageIngestionJobStatusUpdate@",
					sqlStatement, trans.connection->getConnectionId(), elapsed
				);
			}
		}

		if (MMSEngineDBFacade::isIngestionStatusFinalState(newIngestionStatus))
		{
			int64_t ingestionRootKey;
			IngestionRootStatus currentIngestionRootStatus;

			{
				string sqlStatement = std::format(
					"select ir.ingestionRootKey, ir.status "
					"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
					"where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = {}",
					ingestionJobKey
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
					ingestionRootKey = res[0]["ingestionRootKey"].as<int64_t>();
					currentIngestionRootStatus = MMSEngineDBFacade::toIngestionRootStatus(res[0]["status"].as<string>());
				}
				else
				{
					string errorMessage = std::format(
						"IngestionJob is not found"
						", ingestionJobKey: {}"
						", sqlStatement: {}",
						ingestionJobKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
				SPDLOG_INFO(
					"@SQL statistics@"
					", sqlStatement: {}"
					", ingestionJobKey: {}"
					", res.size: {}"
					", elapsed (millisecs): @{}@",
					sqlStatement, ingestionJobKey, res.size(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			int successStatesCount = 0;
			int failureStatesCount = 0;
			int intermediateStatesCount = 0;

			{
				string sqlStatement = std::format("select status from MMS_IngestionJob where ingestionRootKey = {}", ingestionRootKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
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

			SPDLOG_INFO(
				"Job status"
				", ingestionRootKey: {}"
				", intermediateStatesCount: {}"
				", successStatesCount: {}"
				", failureStatesCount: {}",
				ingestionRootKey, intermediateStatesCount, successStatesCount, failureStatesCount
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
				string sqlStatement = std::format(
					"update MMS_IngestionRoot set lastUpdate = NOW() at time zone 'utc', status = {} "
					"where ingestionRootKey = {} ",
					trans.transaction->quote(toString(newIngestionRootStatus)), ingestionRootKey
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
						", ingestionRootKey: {}"
						", rowsUpdated: {}"
						", sqlStatement: {}",
						ingestionRootKey, rowsUpdated, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
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

void MMSEngineDBFacade::setNotToBeExecutedStartingFromBecauseChunkNotSelected(int64_t ingestionJobKey, string processorMMS)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	work trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, true);
	try
	{
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

		SPDLOG_INFO(
			"Update IngestionJob"
			", ingestionJobKey: {}"
			", IngestionStatus: {}"
			", errorMessage: "
			", processorMMS: ",
			ingestionJobKey, toString(MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected)
		);
		updateIngestionJob(
			trans, ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_NotToBeExecuted_ChunkNotSelected,
			"", // errorMessage,
			""	// processorMMS
		);

		// to set 'not to be executed' to the tasks depending from ingestionJobKey,, we will call manageIngestionJobStatusUpdate
		// simulating the IngestionJob failed, that cause the setting to 'not to be executed'
		// for the onSuccess tasks

		bool updateIngestionRootStatus = false;
		manageIngestionJobStatusUpdate(ingestionJobKey, IngestionStatus::End_IngestionFailure, updateIngestionRootStatus, trans);

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		SPDLOG_INFO(
			"setNotToBeExecutedStartingFrom statistics"
			", elapsed (millisecs): {}",
			chrono::duration_cast<chrono::milliseconds>(endPoint - startPoint).count()
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

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress(int64_t ingestionJobKey, double downloadingPercentage)
{
	bool canceledByUser = false;

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
				"update MMS_IngestionJob set downloadingProgress = {} "
				"where ingestionJobKey = {} ",
				downloadingPercentage, ingestionJobKey
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
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", downloadingPercentage: " + to_string(downloadingPercentage) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
		}

		{
			string sqlStatement = std::format("select status from MMS_IngestionJob where ingestionJobKey = {}", ingestionJobKey);
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
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());

				if (ingestionStatus == IngestionStatus::End_CanceledByUser)
					canceledByUser = true;
			}
			else
			{
				string errorMessage = std::format(
					"IngestionJob is not found"
					", ingestionJobKey: {}"
					", sqlStatement: {}",
					ingestionJobKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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

	return canceledByUser;
}

bool MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress(int64_t ingestionJobKey, double uploadingPercentage)
{
	bool toBeCancelled = false;

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
				"update MMS_IngestionJob set uploadingProgress = {} "
				"where ingestionJobKey = {} ",
				uploadingPercentage, ingestionJobKey
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
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", uploadingPercentage: " + to_string(uploadingPercentage) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
		}

		{
			string sqlStatement = std::format("select status from MMS_IngestionJob where ingestionJobKey = {}", ingestionJobKey);
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
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());

				if (ingestionStatus == IngestionStatus::End_CanceledByUser)
					toBeCancelled = true;
			}
			else
			{
				string errorMessage = std::format(
					"IngestionJob is not found"
					", ingestionJobKey: {}"
					", sqlStatement: {}",
					ingestionJobKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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

	return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred(int64_t ingestionJobKey, bool sourceBinaryTransferred)
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
			string sqlStatement;
			if (sourceBinaryTransferred)
				sqlStatement = std::format(
					"update MMS_IngestionJob set sourceBinaryTransferred = {}, "
					"uploadingProgress = 100 "
					"where ingestionJobKey = {} ",
					sourceBinaryTransferred, ingestionJobKey
				);
			else
				sqlStatement = std::format(
					"update MMS_IngestionJob set sourceBinaryTransferred = {} "
					"where ingestionJobKey = {} ",
					sourceBinaryTransferred, ingestionJobKey
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
				// we tried to update a value but the same value was already in the table,
				// in this case rowsUpdated will be 0
				string errorMessage = __FILEREF__ + "no update was done" + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred) +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
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

string MMSEngineDBFacade::ingestionRoot_columnAsString(int64_t workspaceKey, string columnName, int64_t ingestionRootKey, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_ingestionroot:.{}", columnName);
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
		string requestedColumn = std::format("mms_ingestionjob:ij.{}", columnName);
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
		string requestedColumn = std::format("mms_ingestionjob:ij.{}", columnName);
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
			where += std::format("ir.ingestionRootKey = ij.ingestionRootKey and ir.workspaceKey = {} ", workspaceKey);
			if (ingestionJobKey != -1)
				where += std::format("{} ij.ingestionJobKey = {} ", where.size() > 0 ? "and" : "", ingestionJobKey);
			if (label != "")
				where += std::format("{} ij.label = {} ", where.size() > 0 ? "and" : "", trans.transaction->quote(label));

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
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			sqlResultSet = _postgresHelper.buildResult(res);
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (empty(res) && ingestionJobKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
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
		{
			// il chiamante decidera se loggarlo come error
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
			);
		}
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

void MMSEngineDBFacade::addIngestionJobOutput(int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey)
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
		addIngestionJobOutput(trans, ingestionJobKey, mediaItemKey, physicalPathKey, sourceIngestionJobKey);
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

void MMSEngineDBFacade::addIngestionJobOutput(
	PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t sourceIngestionJobKey
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
				std::format("select max(position) as newPosition from MMS_IngestionJobOutput where ingestionJobKey = {}", ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			if (!empty(res))
			{
				if (!res[0]["newPosition"].is_null())
					newPosition = res[0]["newPosition"].as<int>() + 1;
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

		{
			string sqlStatement = std::format(
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey, position) values ("
				"{}, {}, {}, {})",
				ingestionJobKey, mediaItemKey, physicalPathKey, newPosition
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

		if (sourceIngestionJobKey != -1)
		{
			newPosition = 0;
			{
				string sqlStatement =
					std::format("select max(position) as newPosition from MMS_IngestionJobOutput where ingestionJobKey = {}", sourceIngestionJobKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
				if (!empty(res))
				{
					if (!res[0]["newPosition"].is_null())
						newPosition = res[0]["newPosition"].as<int>() + 1;
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

			string sqlStatement = std::format(
				"insert into MMS_IngestionJobOutput (ingestionJobKey, mediaItemKey, physicalPathKey, position) values ("
				"{}, {}, {}, {})",
				sourceIngestionJobKey, mediaItemKey, physicalPathKey, newPosition
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

		throw;
	}
}

long MMSEngineDBFacade::getIngestionJobOutputsCount(int64_t ingestionJobKey, bool fromMaster)
{
	long ingestionJobOutputsCount = -1;

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
		{
			string sqlStatement = std::format("select count(*) from MMS_IngestionJobOutput where ingestionJobKey = {}", ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			ingestionJobOutputsCount = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

			if (!startIngestionDate.empty())
			{
				field = "startIngestionDate";
				requestParametersRoot[field] = startIngestionDate;
			}
			if (!endIngestionDate.empty())
			{
				field = "endIngestionDate";
				requestParametersRoot[field] = endIngestionDate;
			}
			if (!startScheduleDate.empty())
			{
				field = "startScheduleDate";
				requestParametersRoot[field] = startScheduleDate;
			}

			if (!ingestionType.empty())
			{
				field = "ingestionType";
				requestParametersRoot[field] = ingestionType;
			}

			if (!configurationLabel.empty())
			{
				field = "configurationLabel";
				requestParametersRoot[field] = configurationLabel;
			}

			if (!outputChannelLabel.empty())
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

			if (!jsonParametersCondition.empty())
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
		sqlWhere += std::format("and ir.workspaceKey = {} ", workspace->_workspaceKey);
		if (ingestionJobKey != -1)
			sqlWhere += std::format("and ij.ingestionJobKey = {} ", ingestionJobKey);
		if (!label.empty())
		{
			// LOWER was used because the column is using utf8_bin that is case sensitive
			if (labelLike)
				sqlWhere += std::format("and LOWER(ij.label) like LOWER({}) ", trans.transaction->quote("%" + label + "%"));
			else
				sqlWhere += std::format("and LOWER(ij.label) = LOWER({}) ", trans.transaction->quote(label));
		}
		/*
		if (startAndEndIngestionDatePresent)
			sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate
		<= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		*/
		if (!startIngestionDate.empty())
			sqlWhere += std::format(
				"and ir.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startIngestionDate)
			);
		if (!endIngestionDate.empty())
			sqlWhere += std::format(
				"and ir.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endIngestionDate)
			);
		if (!startScheduleDate.empty())
			sqlWhere += std::format(
				"and ij.scheduleStart_virtual >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startScheduleDate)
			);
		if (!ingestionType.empty())
			sqlWhere += std::format("and ij.ingestionType = {} ", trans.transaction->quote(ingestionType));
		if (!configurationLabel.empty())
			sqlWhere += std::format("and ij.configurationLabel_virtual = {} ", trans.transaction->quote(configurationLabel));
		if (!outputChannelLabel.empty())
			sqlWhere += std::format("and ij.outputChannelLabel_virtual = {} ", trans.transaction->quote(outputChannelLabel));
		if (recordingCode != -1)
			sqlWhere += std::format("and ij.recordingCode_virtual = {} ", recordingCode);
		if (broadcastIngestionJobKeyNotNull)
			sqlWhere += ("and ij.broadcastIngestionJobKey_virtual is not null ");
		if (!jsonParametersCondition.empty())
			sqlWhere += std::format("and {} ", jsonParametersCondition);
		if (status == "completed")
			sqlWhere += ("and ij.status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', "
						 "'SourceCopingInProgress', 'SourceUploadingInProgress', 'EncodingQueued') "); // like 'End_%' "
		else if (status == "notCompleted")
			sqlWhere += ("and ij.status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
						 "'SourceUploadingInProgress', 'EncodingQueued') "); // not like 'End_%' "

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij {}", sqlWhere);
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

		json ingestionJobsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select ij.ingestionRootKey, ij.ingestionJobKey, ij.label, "
				"ij.ingestionType, ij.metaDataContent, ij.processorMMS, "
				"to_char(ij.processingStartingFrom, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as processingStartingFrom, "
				"to_char(ij.startProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as startProcessing, "
				"to_char(ij.endProcessing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as endProcessing, "
				"case when ij.startProcessing IS NULL then ir.ingestionDate else ij.startProcessing end as newStartProcessing, "
				"case when ij.endProcessing IS NULL then ir.ingestionDate else ij.endProcessing end as newEndProcessing, "
				"ij.downloadingProgress, ij.uploadingProgress, "
				"ij.status, ij.errorMessages from MMS_IngestionRoot ir, MMS_IngestionJob ij {} "
				"order by newStartProcessing {}, newEndProcessing "
				"limit {} offset {}",
				sqlWhere, asc ? "asc" : "desc", rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			chrono::milliseconds internalSqlDuration(0);
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json ingestionJobRoot = getIngestionJobRoot(workspace, row, dependencyInfo, ingestionJobOutputs, trans);
				internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);

				ingestionJobsRoot.push_back(ingestionJobRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"getIngestionJobs", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
			);
		}

		field = "ingestionJobs";
		responseRoot[field] = ingestionJobsRoot;

		field = "response";
		statusListRoot[field] = responseRoot;
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

	return statusListRoot;
}

json MMSEngineDBFacade::getIngestionJobRoot(
	const shared_ptr<Workspace>& workspace, row &row,
	bool dependencyInfo,	  // added for performance issue
	bool ingestionJobOutputs, // added because output could be thousands of entries
	PostgresConnTrans &trans, chrono::milliseconds *sqlDuration
)
{
	json ingestionJobRoot;

	try
	{
		auto ingestionJobKey = row["ingestionJobKey"].as<int64_t>();

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

		if (sqlDuration != nullptr)
			*sqlDuration = chrono::milliseconds(0);
		if (dependencyInfo)
		{
			chrono::milliseconds localSqlDuration;
			auto [ingestionJobToBeManaged, dependOnIngestionJobKey, dependOnSuccess, ingestionStatusDependency] =
				isIngestionJobToBeManaged(ingestionJobKey, workspace->_workspaceKey, ingestionStatus, ingestionType, trans, &localSqlDuration);
			if (sqlDuration != nullptr)
				*sqlDuration += localSqlDuration;

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
			string sqlStatement = std::format(
				"select mediaItemKey, physicalPathKey, position from MMS_IngestionJobOutput "
				"where ingestionJobKey = {} order by position",
				ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				json mediaItemRoot;

				field = "mediaItemKey";
				auto mediaItemKey = row["mediaItemKey"].as<int64_t>();
				mediaItemRoot[field] = mediaItemKey;

				field = "physicalPathKey";
				auto physicalPathKey = row["physicalPathKey"].as<int64_t>();
				mediaItemRoot[field] = physicalPathKey;

				field = "position";
				mediaItemRoot[field] = row["position"].as<int>();

				mediaItemsRoot.push_back(mediaItemRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			if (sqlDuration != nullptr)
				*sqlDuration += chrono::milliseconds(elapsed);
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
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

		field = "errorMessages";
		if (row["errorMessages"].is_null())
			ingestionJobRoot[field] = nullptr;
		else
		{
			json errorMessagesRoot = json::array();

			auto array = row["errorMessage"].as_array();
			pair<array_parser::juncture, string> elem;
			do
			{
				elem = array.get_next();
				if (elem.first == array_parser::juncture::string_value)
					errorMessagesRoot.push_back(elem.second);
			} while (elem.first != array_parser::juncture::done);

			ingestionJobRoot[field] = errorMessagesRoot;

			/*
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
			*/
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
		{
			string sqlStatement = std::format(
				"select encodingJobKey, type, parameters, status, encodingProgress, encodingPriority, "
				"to_char(encodingJobStart, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobStart, "
				"to_char(encodingJobEnd, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobEnd, "
				"processorMMS, encoderKey, encodingPid, failuresNumber from MMS_EncodingJob "
				"where ingestionJobKey = {} ",
				ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			if (sqlDuration != nullptr)
				*sqlDuration += chrono::milliseconds(elapsed);
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
		break;
		default:;
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

	return ingestionJobRoot;
}

void MMSEngineDBFacade::checkWorkspaceStorageAndMaxIngestionNumber(int64_t workspaceKey)
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
		int maxIngestionsNumber;
		int currentIngestionsNumber;
		EncodingPeriod encodingPeriod;
		string periodUtcStartDateTime;
		string periodUtcEndDateTime;

		{
			string sqlStatement = std::format(
				"select c.maxIngestionsNumber, cmi.currentIngestionsNumber, c.encodingPeriod, "
				"to_char(cmi.startDateTime, 'YYYY-MM-DD HH24:MI:SS') as utcStartDateTime, "
				"to_char(cmi.endDateTime, 'YYYY-MM-DD HH24:MI:SS') as utcEndDateTime "
				"from MMS_Workspace c, MMS_WorkspaceMoreInfo cmi "
				"where c.workspaceKey = cmi.workspaceKey and c.workspaceKey = {}",
				workspaceKey
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
				maxIngestionsNumber = res[0]["maxIngestionsNumber"].as<int>();
				currentIngestionsNumber = res[0]["currentIngestionsNumber"].as<int>();
				encodingPeriod = toEncodingPeriod(res[0]["encodingPeriod"].as<string>());
				periodUtcStartDateTime = res[0]["utcStartDateTime"].as<string>();
				periodUtcEndDateTime = res[0]["utcEndDateTime"].as<string>();
			}
			else
			{
				string errorMessage = std::format(
					"Workspace is not present/configured"
					", workspaceKey: {}"
					", sqlStatement: {}",
					workspaceKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		// check maxStorage first
		{
			int64_t workSpaceUsageInBytes;
			int64_t maxStorageInMB;

			pair<int64_t, int64_t> workSpaceUsageDetails = getWorkspaceUsage(trans, workspaceKey);
			tie(workSpaceUsageInBytes, maxStorageInMB) = workSpaceUsageDetails;

			int64_t totalSizeInMB = workSpaceUsageInBytes / 1000000;
			if (totalSizeInMB >= maxStorageInMB)
			{
				string errorMessage = std::format(
					"Reached the max storage dedicated for your Workspace"
					", maxStorageInMB: {}"
					", totalSizeInMB: {}"
					". It is needed to increase Workspace capacity.",
					maxStorageInMB, totalSizeInMB
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		bool ingestionsAllowed = true;
		bool periodExpired = false;
		// char newPeriodUtcStartDateTime[64];
		string newPeriodUtcStartDateTime;
		// char newPeriodUtcEndDateTime[64];
		string newPeriodUtcEndDateTime;

		{
			// char strUtcDateTimeNow[64];
			string strUtcDateTimeNow;
			tm tmDateTimeNow;
			chrono::system_clock::time_point now = chrono::system_clock::now();
			time_t utcTimeNow = chrono::system_clock::to_time_t(now);
			gmtime_r(&utcTimeNow, &tmDateTimeNow);

			/*
			sprintf(
				strUtcDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, tmDateTimeNow.tm_mday,
				tmDateTimeNow.tm_hour, tmDateTimeNow.tm_min, tmDateTimeNow.tm_sec
			);
			*/
			strUtcDateTimeNow = std::format(
				"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, tmDateTimeNow.tm_mday,
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
					/*
					sprintf(
						newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
						tmDateTimeNow.tm_mday,
						0, // tmDateTimeNow. tm_hour,
						0, // tmDateTimeNow. tm_min,
						0  // tmDateTimeNow. tm_sec
					);
					*/
					newPeriodUtcStartDateTime = std::format(
						"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, tmDateTimeNow.tm_mday, 0,
						0, 0
					);
					/*
					sprintf(
						newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
						tmDateTimeNow.tm_mday,
						23, // tmCurrentDateTime. tm_hour,
						59, // tmCurrentDateTime. tm_min,
						59	// tmCurrentDateTime. tm_sec
					);
					*/
					newPeriodUtcEndDateTime = std::format(
						"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, tmDateTimeNow.tm_mday,
						23, 59, 59
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

						/*
						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmMondayOfCurrentWeek.tm_year + 1900,
							tmMondayOfCurrentWeek.tm_mon + 1, tmMondayOfCurrentWeek.tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
							0  // tmDateTimeNow. tm_sec
						);
						*/
						newPeriodUtcStartDateTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmMondayOfCurrentWeek.tm_year + 1900, tmMondayOfCurrentWeek.tm_mon + 1,
							tmMondayOfCurrentWeek.tm_mday, 0, 0, 0
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

						/*
						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmSundayOfCurrentWeek.tm_year + 1900,
							tmSundayOfCurrentWeek.tm_mon + 1, tmSundayOfCurrentWeek.tm_mday,
							23, // tmSundayOfCurrentWeek. tm_hour,
							59, // tmSundayOfCurrentWeek. tm_min,
							59	// tmSundayOfCurrentWeek. tm_sec
						);
						*/
						newPeriodUtcEndDateTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmSundayOfCurrentWeek.tm_year + 1900, tmSundayOfCurrentWeek.tm_mon + 1,
							tmSundayOfCurrentWeek.tm_mday, 23, 59, 59
						);
					}
				}
				else if (encodingPeriod == EncodingPeriod::Monthly)
				{
					// first day of the month
					{
						/*
						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1,
							1, // tmDateTimeNow. tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
							0  // tmDateTimeNow. tm_sec
						);
						*/
						newPeriodUtcStartDateTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, tmDateTimeNow.tm_mon + 1, 1, 0, 0, 0
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

						/*
						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmLastDayOfCurrentMonth.tm_year + 1900,
							tmLastDayOfCurrentMonth.tm_mon + 1, tmLastDayOfCurrentMonth.tm_mday,
							23, // tmDateTimeNow. tm_hour,
							59, // tmDateTimeNow. tm_min,
							59	// tmDateTimeNow. tm_sec
						);
						*/
						newPeriodUtcEndDateTime = std::format(
							"{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmLastDayOfCurrentMonth.tm_year + 1900, tmLastDayOfCurrentMonth.tm_mon + 1,
							tmLastDayOfCurrentMonth.tm_mday, 23, 59, 59
						);
					}
				}
				else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
				{
					// first day of the year
					{
						/*
						sprintf(
							newPeriodUtcStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900,
							1, // tmDateTimeNow. tm_mon + 1,
							1, // tmDateTimeNow. tm_mday,
							0, // tmDateTimeNow. tm_hour,
							0, // tmDateTimeNow. tm_min,
							0  // tmDateTimeNow. tm_sec
						);
						*/
						newPeriodUtcStartDateTime =
							std::format("{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, 1, 1, 0, 0, 0);
					}

					// last day of the month
					{
						/*
						sprintf(
							newPeriodUtcEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d", tmDateTimeNow.tm_year + 1900,
							12, // tmDateTimeNow. tm_mon + 1,
							31, // tmDateTimeNow. tm_mday,
							23, // tmDateTimeNow. tm_hour,
							59, // tmDateTimeNow. tm_min,
							59	// tmDateTimeNow. tm_sec
						);
						*/
						newPeriodUtcEndDateTime =
							std::format("{:0>4}-{:0>2}-{:0>2} {:0>2}:{:0>2}:{:0>2}", tmDateTimeNow.tm_year + 1900, 12, 31, 23, 59, 59);
					}
				}
			}
		}

		if (periodExpired)
		{
			string sqlStatement = std::format(
				"update MMS_WorkspaceMoreInfo set currentIngestionsNumber = 0, "
				"startDateTime = to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"endDateTime = to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS') "
				"where workspaceKey = {} ",
				trans.transaction->quote(newPeriodUtcStartDateTime), trans.transaction->quote(newPeriodUtcEndDateTime), workspaceKey
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
					", newPeriodUtcStartDateTime: {}"
					", newPeriodUtcEndDateTime: {}"
					", workspaceKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					newPeriodUtcStartDateTime, newPeriodUtcEndDateTime, workspaceKey, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!ingestionsAllowed)
		{
			string errorMessage = std::format(
				"Reached the max number of Ingestions in your period"
				", maxIngestionsNumber: {}"
				", encodingPeriod: {}"
				". It is needed to increase Workspace capacity.",
				maxIngestionsNumber, toString(encodingPeriod)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
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

void MMSEngineDBFacade::fixIngestionJobsHavingWrongStatus()
{
	SPDLOG_INFO("fixIngestionJobsHavingWrongStatus");

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
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
				string sqlStatement = std::format(
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
				result res = trans.transaction->exec(sqlStatement);
				for (auto row : res)
				{
					int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
					int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();
					string ingestionJobStatus = row["ingestionJobStatus"].as<string>();
					string encodingJobStatus = row["encodingJobStatus"].as<string>();

					{
						string errorMessage = std::format(
							"Found IngestionJob having wrong status"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", ingestionJobStatus: {}"
							", encodingJobStatus: {}",
							ingestionJobKey, encodingJobKey, ingestionJobStatus, encodingJobStatus
						);
						SPDLOG_ERROR(errorMessage);

						updateIngestionJob(trans, ingestionJobKey, IngestionStatus::End_CanceledByMMS, errorMessage);

						totalRowsUpdated++;
					}
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
					e.query(), e.what(), trans.connection->getConnectionId()
				);

				currentRetriesOnError++;
				if (currentRetriesOnError >= maxRetriesOnError)
					throw e;

				{
					int secondsBetweenRetries = 15;
					SPDLOG_INFO(
						"fixIngestionJobsHavingWrongStatus failed, waiting before to try again"
						", currentRetriesOnError: {}"
						", maxRetriesOnError: {}"
						", secondsBetweenRetries: {}",
						currentRetriesOnError, maxRetriesOnError, secondsBetweenRetries
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
		}

		SPDLOG_INFO(
			"fixIngestionJobsHavingWrongStatus "
			", totalRowsUpdated: {}",
			totalRowsUpdated
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

void MMSEngineDBFacade::updateIngestionJob_LiveRecorder(
	int64_t workspaceKey, int64_t ingestionJobKey, bool ingestionJobLabelModified, string newIngestionJobLabel, bool channelLabelModified,
	string newChannelLabel, bool recordingPeriodStartModified, string newRecordingPeriodStart, bool recordingPeriodEndModified,
	string newRecordingPeriodEnd, bool recordingVirtualVODModified, bool newRecordingVirtualVOD, bool admin
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
		if (ingestionJobLabelModified || channelLabelModified || recordingPeriodStartModified || recordingPeriodEndModified)
		{
			string setSQL;

			if (ingestionJobLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("label = {}", trans.transaction->quote(newIngestionJobLabel));
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
					setSQL += std::format(
						"jsonb_set(metaDataContent, {{configurationLabel}}, jsonb {})", trans.transaction->quote("\"" + newChannelLabel + "\"")
					);
				if (recordingVirtualVODModified && newRecordingVirtualVOD)
					setSQL +=
						std::format(", '{{liveRecorderVirtualVOD}}', json '{}')", trans.transaction->quote("\"" + newRecordingPeriodStart + "\""));
				if (recordingPeriodEndModified)
					setSQL += std::format(", {{schedule,end}}, jsonb {})", trans.transaction->quote("\"" + newRecordingPeriodEnd + "\""));
				if (recordingPeriodStartModified)
					setSQL += std::format(", {{schedule,start}}, jsonb {})", trans.transaction->quote("\"" + newRecordingPeriodStart + "\""));
			}

			/*
			if (channelLabelModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("metaDataContent = jsonb_set(metaDataContent, {{configurationLabel}}, jsonb {})",
					trans.transaction->quote("\"" + newChannelLabel + "\""));
			}

			if (recordingPeriodStartModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("metaDataContent = jsonb_set(metaDataContent, {{schedule,start}}, jsonb {})",
					trans.transaction->quote("\"" + newRecordingPeriodStart + "\""));
			}

			if (recordingPeriodEndModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("metaDataContent = jsonb_set(metaDataContent, {{schedule,end}}, jsonb {})",
					trans.transaction->quote("\"" + newRecordingPeriodEnd + "\""));
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

			string sqlStatement = std::format(
				"update MMS_IngestionJob {} "
				"where ingestionJobKey = {} ",
				setSQL, ingestionJobKey
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
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", newTitle: " + newTitle
						+ ", newUserData: " + newUserData
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
			if (recordingVirtualVODModified && !newRecordingVirtualVOD)
			{
				string sqlStatement = std::format(
					"update MMS_IngestionJob "
					"set metaDataContent = metaDataContent - 'liveRecorderVirtualVOD' "
					"where ingestionJobKey = {} ",
					ingestionJobKey
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
				warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
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
