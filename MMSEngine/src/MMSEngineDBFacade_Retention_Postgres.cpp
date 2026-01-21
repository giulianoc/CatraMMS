
#include "MMSEngineDBFacade.h"
#include "spdlog/fmt/fmt.h"
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
using namespace pqxx;

void MMSEngineDBFacade::retentionOfIngestionData()
{
	_logger->info(__FILEREF__ + "retentionOfIngestionData");

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		{
			_logger->info(__FILEREF__ + "retentionOfIngestionData. IngestionRoot");
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			// we will remove by steps to avoid error because of transaction log overflow
			int sqlLimit = 1000;
			int totalRowsRemoved = 0;
			bool moreRowsToBeRemoved = true;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeRemoved && currentRetriesOnError < maxRetriesOnError)
			{
				try
				{
					// 2022-01-30: we cannot remove any OLD ingestionroot/ingestionjob/encodingjob
					//			because we have the sheduled jobs (recording, proxy, ...) that
					//			can be scheduled to be run on the future
					//			For this reason I added the status condition
					// scenarios anomalous:
					//	- encoding is in a final state but ingestion is not: we already have the
					//		fixIngestionJobsHavingWrongStatus method
					//	- ingestion is in a final state but encoding is not: we already have the
					//		fixEncodingJobsHavingWrongStatus method
					//	In postgres non esiste la delete con il limit, per questo motivo bisogna fare la select nested
					string sqlStatement = std::format(
						"delete from MMS_IngestionRoot where ingestionRootKey in "
						"(select ingestionRootKey from MMS_IngestionRoot "
						"where ingestionDate < NOW() at time zone 'utc' - INTERVAL '{} days' "
						"and status like 'Completed%' limit {}) ",
						_ingestionWorkflowCompletedRetentionInDays, sqlLimit
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
					totalRowsRemoved += rowsUpdated;
					if (rowsUpdated == 0)
						moreRowsToBeRemoved = false;

					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					LOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), trans.connection->getConnectionId()
					);

					currentRetriesOnError++;

					int secondsBetweenRetries = 15;
					_logger->info(
						__FILEREF__ + "retentionOfIngestionData. IngestionRoot failed, " + "waiting before to try again" +
						", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
						", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionRoot" + ", totalRowsRemoved: " + to_string(totalRowsRemoved) +
				", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}

		// AddContent IngestionJobs taking too time to download/move/copy/upload the content are set to failed
		/* 2025-04-18: commentata perchè inclusa nella select successiva
		{
			LOG_INFO("retentionOfIngestionData. AddContent IngestionJobs taking too time to be completed");
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			int sqlLimit = 1000;
			long totalRowsUpdated = 0;
			bool moreRowsToBeUpdated = true;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeUpdated && currentRetriesOnError < maxRetriesOnError)
			{
				try
				{
					IngestionStatus newIngestionStatus = IngestionStatus::End_IngestionFailure;
					string errorMessage = "Set to Failure by MMS because of too old";

					// 2025-03-29: scenario: il file è stato trasferito nella directory per l'ingestion (quindi sourceBinaryTransferred è true),
					// 	Per qualche motivo l'ingestion fallisce (ad es. è arrivato il retention dei file in quella directory e l'engine fallisce
					// 	perchè non riesce a copiare il file in MMSRepository), l'IngestionJob (AddContent) rimane per sempre non completato
					// 	Per questo motivo ho eliminato il controllo sourceBinaryTransferred = false, cioé tutti gli AddContent Jobs, anche quelli
					//  trasferiti, avranno un retention
					string sqlStatement = std::format(
						"select ingestionJobKey from MMS_IngestionJob "
						"where status in ({}, {}, {}, {}) " // and sourceBinaryTransferred = false "
						"and startProcessing + INTERVAL '{} hours' <= NOW() at time zone 'utc' "
						"limit {}",
						trans.transaction->quote(toString(IngestionStatus::SourceDownloadingInProgress)),
						trans.transaction->quote(toString(IngestionStatus::SourceMovingInProgress)),
						trans.transaction->quote(toString(IngestionStatus::SourceCopingInProgress)),
						trans.transaction->quote(toString(IngestionStatus::SourceUploadingInProgress)),
						_addContentIngestionJobsNotCompletedRetentionInDays * 24, sqlLimit
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
						{
							LOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", IngestionStatus: {}"
								", errorMessage: {}"
								", processorMMS: ",
								ingestionJobKey, toString(newIngestionStatus), errorMessage
							);
							updateIngestionJob(trans, ingestionJobKey, newIngestionStatus, errorMessage);
							totalRowsUpdated++;
						}
					}
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@"
						", res.size: {}"
						", totalRowsUpdated: {}",
						sqlStatement, trans.connection->getConnectionId(), elapsed, res.size(), totalRowsUpdated
					);

					if (res.size() == 0)
						moreRowsToBeUpdated = false;
					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					LOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), trans.connection->getConnectionId()
					);

					currentRetriesOnError++;

					{
						int secondsBetweenRetries = 15;
						_logger->info(
							__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time failed, " + "waiting before to try again" +
							", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
							", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
						);
						this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
					}
				}
			}

			_logger->info(
				__FILEREF__ +
				"retentionOfIngestionData. IngestionJobs taking too time "
				"to download/move/copy/upload the content" +
				", totalRowsUpdated: " + to_string(totalRowsUpdated) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}
		*/

		// IngestionJobs without encodingJob taking too time to be completed are set to failed
		// Comprende anche gli AddContent rimasti incompleti, indipendentemente se sia stati trasferiti o meno (sourceBinaryTransferred).
		// Nota che i casi in cui IngestionJob sia in stato finale ed encoding no o viceversa sono già gestiti dai metodi fix*JobsHavingWrongStatus
		{
			LOG_INFO("retentionOfIngestionData. IngestionJobs without EncodingJob taking too time to be completed");
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			int sqlLimit = 1000;
			long totalRowsUpdated = 0;
			bool moreRowsToBeUpdated = true;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeUpdated && currentRetriesOnError < maxRetriesOnError)
			{
				try
				{
					IngestionStatus newIngestionStatus = IngestionStatus::End_IngestionFailure;
					string errorMessage = "Set to Failure by MMS because of too old";

					// Si poteva eseguire una left join tra MMS_IngestionJob e MMS_EncodingJob ma, poichè abbiamo un indice su ej.ingestionJobKey,
					// la query sotto dovrebbe essere piu efficiente rispetto alla left join
					string sqlStatement = std::format(
						"select ij.ingestionJobKey from MMS_IngestionJob ij "
						"where ij.status not like 'End_%' "
						"and ij.processingStartingFrom + INTERVAL '{} hours' <= NOW() at time zone 'utc' "
						"and not exists (select 1 from MMS_EncodingJob ej where ij.ingestionJobKey = ej.ingestionJobKey) "
						"limit {}",
						(_doNotManageIngestionsOlderThanDays + 1) * 24, sqlLimit
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
						{
							LOG_INFO(
								"Update IngestionJob"
								", ingestionJobKey: {}"
								", IngestionStatus: {}"
								", errorMessage: {}"
								", processorMMS: ",
								ingestionJobKey, toString(newIngestionStatus), errorMessage
							);
							updateIngestionJob(trans, ingestionJobKey, newIngestionStatus, errorMessage);
							totalRowsUpdated++;
						}
					}
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@"
						", res.size: {}"
						", totalRowsUpdated: {}",
						sqlStatement, trans.connection->getConnectionId(), elapsed, res.size(), totalRowsUpdated
					);

					if (res.size() == 0)
						moreRowsToBeUpdated = false;
					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					LOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), trans.connection->getConnectionId()
					);

					currentRetriesOnError++;

					{
						int secondsBetweenRetries = 15;
						_logger->info(
							__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time failed, " + "waiting before to try again" +
							", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
							", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
						);
						this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
					}
				}
			}

			_logger->info(
				__FILEREF__ +
				"retentionOfIngestionData. IngestionJobs taking too time "
				"to download/move/copy/upload the content" +
				", totalRowsUpdated: " + to_string(totalRowsUpdated) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}

		{
			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionJobs not completed " + "(state Start_TaskQueued) and too old to be considered " +
				"by MMSEngineDBFacade::getIngestionsToBeManaged"
			);
			chrono::system_clock::time_point startRetention = chrono::system_clock::now();

			int sqlLimit = 1000;
			long totalRowsUpdated = 0;
			bool moreRowsToBeUpdated;
			int maxRetriesOnError = 2;
			int currentRetriesOnError = 0;
			while (moreRowsToBeUpdated && currentRetriesOnError < maxRetriesOnError)
			{
				try
				{
					// 2021-07-17: In this scenario the IngestionJobs would remain infinite time:
					string sqlStatement = std::format(
						"select ingestionJobKey from MMS_IngestionJob "
						"where status = {} and NOW() at time zone 'utc' > processingStartingFrom + INTERVAL '{} days' "
						"limit {}",
						trans.transaction->quote(toString(IngestionStatus::Start_TaskQueued)), _doNotManageIngestionsOlderThanDays, sqlLimit
					);
					// "where (processorMMS is NULL or processorMMS = ?) "
					// "and status = ? and NOW() > DATE_ADD(processingStartingFrom, INTERVAL ? DAY)";
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();

						string errorMessage = "Canceled by MMS because not completed and too old";

						_logger->info(
							__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
							", IngestionStatus: " + "End_CanceledByMMS" + ", errorMessage: " + errorMessage
						);
						updateIngestionJob(ingestionJobKey, MMSEngineDBFacade::IngestionStatus::End_CanceledByMMS, errorMessage);
						totalRowsUpdated++;
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

					if (res.size() == 0)
						moreRowsToBeUpdated = false;
					currentRetriesOnError = 0;
				}
				catch (sql_error const &e)
				{
					// Deadlock!!!
					LOG_ERROR(
						"SQL exception"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), trans.connection->getConnectionId()
					);

					currentRetriesOnError++;

					{
						int secondsBetweenRetries = 15;
						_logger->info(
							__FILEREF__ + "retentionOfIngestionData. IngestionJobs taking too time failed, " + "waiting before to try again" +
							", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
							", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
						);
						this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
					}
				}
			}

			_logger->info(
				__FILEREF__ + "retentionOfIngestionData. IngestionJobs not completed " + "(state Start_TaskQueued) and too old to be considered " +
				"by MMSEngineDBFacade::getIngestionsToBeManaged" + ", totalRowsUpdated: " + to_string(totalRowsUpdated) + ", elapsed (millisecs): @" +
				to_string(chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startRetention).count()) + "@"
			);
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::retentionOfStatisticData()
{
	LOG_INFO("retentionOfStatisticData");

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
		// check if next partition already exist and, if not, create it
		{
			string partition_start;
			string partition_end;
			string partitionName;
			{
				// 2022-05-01: changed from one to two months because, the first of each
				//	month, until this procedure do not run, it would not work
				chrono::duration<int, ratio<60 * 60 * 24 * 32>> one_month(1);

				// char strDateTime[64];
				string strDateTime;
				tm tmDateTime;
				time_t utcTime;
				chrono::system_clock::time_point today = chrono::system_clock::now();

				chrono::system_clock::time_point nextMonth = today + one_month;
				utcTime = chrono::system_clock::to_time_t(nextMonth);
				localtime_r(&utcTime, &tmDateTime);
				// sprintf(strDateTime, "%04d-%02d-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}-{:0>2}-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partition_start = strDateTime;

				// sprintf(strDateTime, "%04d_%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}_{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partitionName = std::format("requeststatistic_{}", strDateTime);

				chrono::system_clock::time_point nextNextMonth = nextMonth + one_month;
				utcTime = chrono::system_clock::to_time_t(nextNextMonth);
				localtime_r(&utcTime, &tmDateTime);
				// sprintf(strDateTime, "%04d-%02d-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}-{:0>2}-01", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partition_end = strDateTime;
			}

			/*
			SELECT
				nmsp_parent.nspname AS parent_schema,
				parent.relname      AS parent,
				nmsp_child.nspname  AS child_schema,
				child.relname       AS child
			FROM pg_inherits
				JOIN pg_class parent            ON pg_inherits.inhparent = parent.oid
				JOIN pg_class child             ON pg_inherits.inhrelid   = child.oid
				JOIN pg_namespace nmsp_parent   ON nmsp_parent.oid  = parent.relnamespace
				JOIN pg_namespace nmsp_child    ON nmsp_child.oid   = child.relnamespace
			WHERE parent.relname='mms_requeststatistic';
			*/
			string sqlStatement = std::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.transaction->quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.transaction->exec1(sqlStatement)[0].as<int>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (count == 0)
			{
				string sqlStatement = std::format(
					"CREATE TABLE {} PARTITION OF MMS_RequestStatistic "
					"FOR VALUES FROM ({}) TO ({}) ",
					partitionName, trans.transaction->quote(partition_start), trans.transaction->quote(partition_end)
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

		// check if a partition has to be removed because "expired", if yes, remove it
		{
			string partitionName;
			{
				chrono::duration<int, ratio<60 * 60 * 24 * 31>> retentionMonths(_statisticRetentionInMonths);

				chrono::system_clock::time_point today = chrono::system_clock::now();
				chrono::system_clock::time_point retention = today - retentionMonths;
				time_t utcTime_retention = chrono::system_clock::to_time_t(retention);

				// char strDateTime[64];
				string strDateTime;
				tm tmDateTime;

				localtime_r(&utcTime_retention, &tmDateTime);

				// sprintf(strDateTime, "%04d_%02d", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				strDateTime = std::format("{:0>4}_{:0>2}", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1);
				partitionName = std::format("requeststatistic_{}", strDateTime);
			}

			string sqlStatement = std::format(
				"select count(*) "
				"FROM pg_inherits "
				"JOIN pg_class parent ON pg_inherits.inhparent = parent.oid "
				"JOIN pg_class child ON pg_inherits.inhrelid   = child.oid "
				"JOIN pg_namespace nmsp_parent ON nmsp_parent.oid  = parent.relnamespace "
				"JOIN pg_namespace nmsp_child ON nmsp_child.oid   = child.relnamespace "
				"WHERE parent.relname='mms_requeststatistic' "
				"and child.relname = {} ",
				trans.transaction->quote(partitionName)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int count = trans.transaction->exec1(sqlStatement)[0].as<int>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (count > 0)
			{
				string sqlStatement = std::format("DROP TABLE {}", partitionName);
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
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}

void MMSEngineDBFacade::retentionOfDeliveryAuthorization()
{
	_logger->info(__FILEREF__ + "retentionOfDeliveryAuthorization");
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
					string sqlStatement = std::format(
						"delete from MMS_DeliveryAuthorization where deliveryAuthorizationKey in "
						"(select deliveryAuthorizationKey from MMS_DeliveryAuthorization "
						"where (authorizationTimestamp + INTERVAL '1 second' * (ttlInSeconds + {})) < NOW() at time zone 'utc' limit {}) ",
						retention, maxToBeRemoved
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

					LOG_ERROR(
						"SQL exception, Deadlock!!!"
						", query: {}"
						", exceptionMessage: {}"
						", conn: {}",
						e.query(), e.what(), trans.connection->getConnectionId()
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
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		if (se != nullptr)
			LOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else
			LOG_ERROR(
				"query failed"
				", exception: {}"
				", conn: {}",
				e.what(), trans.connection->getConnectionId()
			);

		trans.setAbort();

		throw;
	}
}
