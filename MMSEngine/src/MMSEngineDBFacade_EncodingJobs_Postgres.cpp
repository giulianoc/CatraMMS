
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "PersistenceLock.h"
#include "spdlog/spdlog.h"
#include <regex>
#include <utility>

void MMSEngineDBFacade::getToBeProcessedEncodingJobs(
	string processorMMS, vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &encodingItems, int timeBeforeToPrepareResourcesInMinutes,
	int maxEncodingsNumber
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

		int liveProxyToBeEncoded = 0;
		int liveRecorderToBeEncoded = 0;
		int othersToBeEncoded = 0;

		encodingItems.clear();

		int initialGetEncodingJobsCurrentIndex = _getEncodingJobsCurrentIndex;

		_logger->info(
			__FILEREF__ + "getToBeProcessedEncodingJobs" + ", initialGetEncodingJobsCurrentIndex: " + to_string(initialGetEncodingJobsCurrentIndex)
		);

		bool stillRows = true;
		while (encodingItems.size() < maxEncodingsNumber && stillRows)
		{
			_logger->info(
				__FILEREF__ + "getToBeProcessedEncodingJobs (before select)" +
				", _getEncodingJobsCurrentIndex: " + to_string(_getEncodingJobsCurrentIndex)
			);
			// 2022-01-06: I wanted to have this select running in parallel among all the engines.
			//		For this reason, I have to use 'select for update'.
			//		At the same time, I had to remove the join because a join would lock everything
			//		Without join, if the select got i.e. 20 rows, all the other rows are not locked
			//		and can be got from the other engines
			// 2023-02-07: added skip locked. Questa opzione è importante perchè evita che la select
			//	attenda eventuali lock di altri engine. Considera che un lock su una riga causa anche
			//	il lock di tutte le righe toccate dalla transazione
			// 2023-11-04: One useful syntax is "SELECT … FOR UPDATE SKIP LOCKED."
			//	This syntax is particularly useful in situations where multiple transactions are trying
			//	to update the same set of rows simultaneously. It locks the selected rows but skips
			//	over any rows already locked by other transactions,
			//	thereby reducing the likelihood of deadlocks.
			string sqlStatement = fmt::format(
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName, "
				"ej.utcScheduleStart_virtual "
				"from MMS_EncodingJob ej "
				"where ej.processorMMS is null "
				"and ej.status = {} and ej.encodingJobStart <= NOW() at time zone 'utc' "
				"and (ej.utcScheduleStart_virtual is null or "
				"ej.utcScheduleStart_virtual - (extract(epoch from (NOW() at time zone 'utc'))) < {} * 60) "
				"order by ej.typePriority asc, ej.utcScheduleStart_virtual asc, "
				"ej.encodingPriority desc, ej.creationDate asc, ej.failuresNumber asc "
				"limit {} offset {} for update skip locked",
				trans.quote(MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)), timeBeforeToPrepareResourcesInMinutes, maxEncodingsNumber,
				_getEncodingJobsCurrentIndex
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);

			_getEncodingJobsCurrentIndex += maxEncodingsNumber;
			if (res.size() != maxEncodingsNumber)
				stillRows = false;

			_logger->info(
				__FILEREF__ + "getToBeProcessedEncodingJobs (after select)" + ", _getEncodingJobsCurrentIndex: " +
				to_string(_getEncodingJobsCurrentIndex) + ", encodingResultSet->rowsCount: " + to_string(res.size())
			);
			int resultSetIndex = 0;
			for (auto row : res)
			{
				int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();

				shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem = make_shared<MMSEngineDBFacade::EncodingItem>();

				encodingItem->_encodingJobKey = encodingJobKey;
				encodingItem->_ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
				string encodingType = row["type"].as<string>();
				encodingItem->_encodingType = toEncodingType(encodingType);
				encodingItem->_encodingPriority = static_cast<EncodingPriority>(row["encodingPriority"].as<int>());
				if (row["encoderKey"].is_null())
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = row["encoderKey"].as<int64_t>();
				if (row["stagingEncodedAssetPathName"].is_null())
					encodingItem->_stagingEncodedAssetPathName = "";
				else
					encodingItem->_stagingEncodedAssetPathName = row["stagingEncodedAssetPathName"].as<string>();

				string encodingParameters = row["parameters"].as<string>();

				_logger->info(
					__FILEREF__ + "getToBeProcessedEncodingJobs (resultSet loop)" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", encodingType: " + encodingType + ", initialGetEncodingJobsCurrentIndex: " + to_string(initialGetEncodingJobsCurrentIndex) +
					", resultSetIndex: " + to_string(resultSetIndex) + "/" + to_string(res.size())
				);
				resultSetIndex++;

				{
					string sqlStatement = fmt::format(
						"select ir.workspaceKey "
						"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
						"where ir.ingestionRootKey = ij.ingestionRootKey "
						"and ij.ingestionJobKey = {}",
						encodingItem->_ingestionJobKey
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
						encodingItem->_workspace = getWorkspace(res[0]["workspaceKey"].as<int64_t>());
					else
					{
						string errorMessage = __FILEREF__ + "select failed, no row returned" +
											  ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey) + ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						// in case an encoding job row generate an error, we have to make it to Failed
						// otherwise we will indefinitely get this error
						{
							_logger->info(
								__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
								", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
							string sqlStatement = fmt::format(
								"WITH rows AS (update MMS_EncodingJob set status = {}, "
								"encodingJobEnd = NOW() at time zone 'utc' "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
							SPDLOG_INFO(
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, conn->getConnectionId(),
								chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
							);
						}

						continue;
						// throw runtime_error(errorMessage);
					}
				}
				_logger->info(
					__FILEREF__ + "getToBeProcessedEncodingJobs (after workspaceKey)" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", encodingType: " + encodingType
				);

				{
					string sIngestionJobStatus = toString(ingestionJob_Status(
						encodingItem->_workspace->_workspaceKey, encodingItem->_ingestionJobKey,
						// 2022-12-18: probable the ingestionJob is added recently, let's set true
						true
					));

					string prefix = "End_";
					if (sIngestionJobStatus.starts_with(prefix))
					{
						SPDLOG_ERROR(
							"Found EncodingJob with wrong status"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", ingestionJobStatus: {}",
							encodingItem->_ingestionJobKey, encodingJobKey, sIngestionJobStatus
						);

						// 2122-01-05: updateEncodingJob is not used because the row is already locked
						// updateEncodingJob (
						// 	encodingJobKey,
						// 	EncodingError::CanceledByMMS,
						// 	false,  // isIngestionJobFinished: this field is not used by updateEncodingJob
						// 	ingestionJobKey,
						// 	errorMessage
						// );
						{
							_logger->info(
								__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
								", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_CanceledByMMS)
							);
							string sqlStatement = fmt::format(
								"WITH rows AS (update MMS_EncodingJob set status = {}, "
								"encodingJobEnd = NOW() at time zone 'utc' "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_CanceledByMMS)), encodingItem->_encodingJobKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
							SPDLOG_INFO(
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, conn->getConnectionId(),
								chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
							);
						}

						continue;
					}
				}
				SPDLOG_INFO(
					"getToBeProcessedEncodingJobs (after check ingestionStatus)"
					", encodingJobKey: {}"
					", encodingType: {}",
					encodingJobKey, encodingType
				);

				try
				{
					encodingItem->_encodingParametersRoot = JSONUtils::toJson(encodingParameters);
				}
				catch (runtime_error e)
				{
					SPDLOG_ERROR(e.what());

					// in case an encoding job row generate an error, we have to make it to Failed
					// otherwise we will indefinitely get this error
					{
						SPDLOG_INFO(
							"EncodingJob update"
							", encodingJobKey: {}"
							", status: {}",
							encodingItem->_encodingJobKey, MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
						);
						string sqlStatement = fmt::format(
							"WITH rows AS (update MMS_EncodingJob set status = {} "
							"where encodingJobKey = {} returning 1) select count(*) from rows",
							trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
						SPDLOG_INFO(
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
					}

					continue;
					// throw runtime_error(errorMessage);
				}

				SPDLOG_INFO(
					"getToBeProcessedEncodingJobs (after encodingParameters)"
					", encodingJobKey: {}"
					", encodingType: {}",
					encodingJobKey, encodingType
				);

				// encodingItem->_ingestedParametersRoot
				{
					string sqlStatement =
						fmt::format("select metaDataContent from MMS_IngestionJob where ingestionJobKey = {}", encodingItem->_ingestionJobKey);
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
					{
						string ingestionParameters = res[0]["metaDataContent"].as<string>();

						try
						{
							encodingItem->_ingestedParametersRoot = JSONUtils::toJson(ingestionParameters);
						}
						catch (runtime_error &e)
						{
							_logger->error(e.what());

							// in case an encoding job row generate an error, we have to make it to Failed
							// otherwise we will indefinitely get this error
							{
								_logger->info(
									__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
									", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								);
								string sqlStatement = fmt::format(
									"WITH rows AS (update MMS_EncodingJob set status = {} "
									"where encodingJobKey = {} returning 1) select count(*) from rows",
									trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey
								);
								chrono::system_clock::time_point startSql = chrono::system_clock::now();
								int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
								SPDLOG_INFO(
									"SQL statement"
									", sqlStatement: @{}@"
									", getConnectionId: @{}@"
									", elapsed (millisecs): @{}@",
									sqlStatement, conn->getConnectionId(),
									chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
								);
							}

							continue;
							// throw runtime_error(errorMessage);
						}
					}
					else
					{
						string errorMessage = __FILEREF__ + "select failed, no row returned" +
											  ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey) +
											  ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						// in case an encoding job row generate an error, we have to make it to Failed
						// otherwise we will indefinitely get this error
						{
							_logger->info(
								__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
								", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
							string sqlStatement = fmt::format(
								"WITH rows AS (update MMS_EncodingJob set status = {} "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
							SPDLOG_INFO(
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, conn->getConnectionId(),
								chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
							);
						}

						continue;
						// throw runtime_error(errorMessage);
					}
				}
				_logger->info(
					__FILEREF__ + "getToBeProcessedEncodingJobs" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", encodingType: " + encodingType
				);

				encodingItems.push_back(encodingItem);
				othersToBeEncoded++;

				{
					_logger->info(
						__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
						", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing) + ", processorMMS: " +
						processorMMS
						// 2021-08-22: scenario:
						//	1. the encoding is selected here to be run
						//	2. we have a long queue of encodings and it will not be run
						//		for about 6 hours
						//	3. After 6 hours the encoding finally starts but,
						//		since the encodingJobStart field is not updated,
						//		it seems like the duration of the encoding is 6 hours + real duration.
						//		Also the encoding duration estimates will be wrong.
						// To solve this scenario, we added encodingJobStart in the update command
						// because:
						//	1. once the encoding was selected from the above select, it means
						//		it is the time to be processed
						//	2. even if we update the encodingJobStart field to NOW() and
						//		it will not be run because of the queue, the encoding will continue
						//		to be retrieved from the above select because the condition
						//		ej.encodingJobStart <= NOW() continue to be true
						+ ", encodingJobStart: " + "NOW() at time zone 'utc'"
					);
					string sqlStatement;
					if (!row["utcScheduleStart_virtual"].is_null())
						sqlStatement = fmt::format(
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = {} "
							"where encodingJobKey = {} and processorMMS is null "
							"returning 1) select count(*) from rows",
							trans.quote(toString(EncodingStatus::Processing)), trans.quote(processorMMS), encodingItem->_encodingJobKey
						);
					else
						sqlStatement = fmt::format(
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = {}"
							", encodingJobStart = NOW() at time zone 'utc' "
							"where encodingJobKey = {} and processorMMS is null "
							"returning 1) select count(*) from rows",
							trans.quote(toString(EncodingStatus::Processing)), trans.quote(processorMMS), encodingItem->_encodingJobKey
						);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done" + ", processorMMS: " + processorMMS +
											  ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
											  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}
				_logger->info(
					__FILEREF__ + "getToBeProcessedEncodingJobs (after encodingJob update)" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", encodingType: " + encodingType
				);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		if (encodingItems.size() < maxEncodingsNumber)
			_getEncodingJobsCurrentIndex = 0;

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(
			__FILEREF__ + "getToBeProcessedEncodingJobs statistics" + ", encodingItems.size: " + to_string(encodingItems.size()) +
			", maxEncodingsNumber: " + to_string(maxEncodingsNumber) + ", liveProxyToBeEncoded: " + to_string(liveProxyToBeEncoded) +
			", liveRecorderToBeEncoded: " + to_string(liveRecorderToBeEncoded) + ", othersToBeEncoded: " + to_string(othersToBeEncoded) +
			", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
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

void MMSEngineDBFacade::recoverEncodingsNotCompleted(string processorMMS, vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &encodingItems)
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
		chrono::system_clock::time_point startPoint = chrono::system_clock::now();

		encodingItems.clear();

		SPDLOG_INFO(
			"getProcessingEncodingJobsOfProcessorMMS"
			", processorMMS: {}",
			processorMMS
		);

		/*
		 2024-12-16: nota che abbiamo processorMMS sia in IngestionJob che in EncodingJobs
			Infatti potrebbe essere che un 'engine' gestisce l'IngestionJob ma un'altro 'engine'
			si occupa di gestire l'EncodingJob
		 */
		{
			string sqlStatement = fmt::format(
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName, "
				"ej.utcScheduleStart_virtual "
				"from MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ij.ingestionJobKey = ej.ingestionJobKey "
				"and ej.processorMMS = {} and ij.status = {} and ej.status = {} ",
				trans.quote(processorMMS), trans.quote(MMSEngineDBFacade::toString(IngestionStatus::EncodingQueued)),
				trans.quote(MMSEngineDBFacade::toString(EncodingStatus::Processing))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();

				shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem = make_shared<MMSEngineDBFacade::EncodingItem>();

				encodingItem->_encodingJobKey = encodingJobKey;
				encodingItem->_ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
				string encodingType = row["type"].as<string>();
				encodingItem->_encodingType = toEncodingType(encodingType);
				encodingItem->_encodingPriority = static_cast<EncodingPriority>(row["encodingPriority"].as<int>());
				if (row["encoderKey"].is_null())
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = row["encoderKey"].as<int64_t>();
				if (row["stagingEncodedAssetPathName"].is_null())
					encodingItem->_stagingEncodedAssetPathName = "";
				else
					encodingItem->_stagingEncodedAssetPathName = row["stagingEncodedAssetPathName"].as<string>();

				string encodingParameters = row["parameters"].as<string>();

				{
					string sqlStatement = fmt::format(
						"select ir.workspaceKey "
						"from MMS_IngestionRoot ir, MMS_IngestionJob ij "
						"where ir.ingestionRootKey = ij.ingestionRootKey "
						"and ij.ingestionJobKey = {}",
						encodingItem->_ingestionJobKey
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
					if (empty(res))
					{
						SPDLOG_ERROR(
							"select failed, no row returned"
							", ingestionJobKey: {}"
							", sqlStatement: {}",
							encodingItem->_ingestionJobKey, sqlStatement
						);

						continue;
					}
					encodingItem->_workspace = getWorkspace(res[0]["workspaceKey"].as<int64_t>());
				}

				try
				{
					encodingItem->_encodingParametersRoot = JSONUtils::toJson(encodingParameters);

					encodingItem->_ingestedParametersRoot = ingestionJob_columnAsJson(
						encodingItem->_workspace->_workspaceKey, "metaDataContent", encodingItem->_ingestionJobKey,
						// 2022-12-18: probable the ingestionJob is added recently, let's set true
						true
					);
				}
				catch (runtime_error e)
				{
					SPDLOG_ERROR(e.what());

					continue;
				}

				encodingItems.push_back(encodingItem);
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

		chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		SPDLOG_INFO(
			"recoverProcessingEncodingJobs statistics"
			", encodingItems.size: {}"
			", elapsed (secs): {}",
			encodingItems.size(), chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()
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

int MMSEngineDBFacade::updateEncodingJob(
	int64_t encodingJobKey, EncodingError encodingError, bool isIngestionJobFinished, int64_t ingestionJobKey, string ingestionErrorMessage,
	bool forceEncodingToBeFailed
)
{
	int encodingFailureNumber;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	bool updateToBeTriedAgain = true;
	int retriesNumber = 0;
	int maxRetriesNumber = 3;
	int secondsBetweenRetries = 5;
	while (updateToBeTriedAgain && retriesNumber < maxRetriesNumber)
	{
		retriesNumber++;

		shared_ptr<PostgresConnection> conn = nullptr;

		conn = connectionPool->borrow();
		// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
		// Se questo non dovesse essere vero, unborrow non sarà chiamata
		// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
		work trans{*(conn->_sqlConnection)};

		encodingFailureNumber = -1;

		try
		{
			EncodingStatus newEncodingStatus;
			if (encodingError == EncodingError::PunctualError)
			{
				string type;
				{
					string sqlStatement = fmt::format(
						"select type, failuresNumber from MMS_EncodingJob "
						"where encodingJobKey = {}",
						encodingJobKey
					);
					// "where encodingJobKey = ? for update";
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
					{
						type = res[0]["type"].as<string>();
						encodingFailureNumber = res[0]["failuresNumber"].as<int>();
					}
					else
					{
						string errorMessage = __FILEREF__ + "EncodingJob not found" + ", EncodingJobKey: " + to_string(encodingJobKey) +
											  ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				{
					string sqlStatement;
					// in case of LiveRecorder there is no more retries since it already run up
					//		to the end of the recording
					if (forceEncodingToBeFailed || encodingFailureNumber + 1 >= _maxEncodingFailures)
					{
						newEncodingStatus = EncodingStatus::End_Failed;

						_logger->info(
							__FILEREF__ + "update EncodingJob" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
							", encodingFailureNumber: " + to_string(encodingFailureNumber) + ", encodingJobKey: " + to_string(encodingJobKey)
						);
						sqlStatement = fmt::format(
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, failuresNumber = {}, "
							"encodingProgress = NULL where encodingJobKey = {} and status = {} "
							"returning 1) select count(*) from rows",
							trans.quote(toString(newEncodingStatus)), encodingFailureNumber, encodingJobKey,
							trans.quote(toString(EncodingStatus::Processing))
						);
					}
					else
					{
						newEncodingStatus = EncodingStatus::ToBeProcessed;
						encodingFailureNumber++;

						sqlStatement = fmt::format(
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, encoderKey = NULL, "
							"failuresNumber = {}, encodingProgress = NULL where encodingJobKey = {} "
							"and status = {} returning 1) select count(*) from rows",
							trans.quote(toString(newEncodingStatus)), encodingFailureNumber, encodingJobKey,
							trans.quote(toString(EncodingStatus::Processing))
						);
					}
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int64_t>();
					SPDLOG_INFO(
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done" +
											  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
											  ", sqlStatement: " + sqlStatement;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingFailureNumber: " + to_string(encodingFailureNumber) + ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::MaxCapacityReached || encodingError == EncodingError::ErrorBeforeEncoding)
			{
				newEncodingStatus = EncodingStatus::ToBeProcessed;

				_logger->info(
					__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", status: " + MMSEngineDBFacade::toString(newEncodingStatus) + ", processorMMS: " + "NULL" + ", encoderKey = NULL" +
					", encodingProgress: " + "NULL"
				);
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encoderKey = NULL, encodingProgress = NULL "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey, trans.quote(toString(EncodingStatus::Processing))
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
										  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::KilledByUser)
			{
				newEncodingStatus = EncodingStatus::End_KilledByUser;

				_logger->info(
					__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", status: " +
					MMSEngineDBFacade::toString(newEncodingStatus) + ", processorMMS: " + "NULL" + ", encodingJobEnd: " + "NOW() at time zone 'utc'"
				);
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey, trans.quote(toString(EncodingStatus::Processing))
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
										  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::CanceledByUser)
			{
				newEncodingStatus = EncodingStatus::End_CanceledByUser;

				_logger->info(
					__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", status: " +
					MMSEngineDBFacade::toString(newEncodingStatus) + ", processorMMS: " + "NULL" + ", encodingJobEnd: " + "NOW() at time zone 'utc'"
				);
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey, trans.quote(toString(EncodingStatus::ToBeProcessed))
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
										  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::CanceledByMMS)
			{
				newEncodingStatus = EncodingStatus::End_CanceledByMMS;

				_logger->info(
					__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", status: " +
					MMSEngineDBFacade::toString(newEncodingStatus) + ", processorMMS: " + "NULL" + ", encodingJobEnd: " + "NOW() at time zone 'utc'"
				);
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey
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
										  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else // success
			{
				newEncodingStatus = EncodingStatus::End_Success;

				_logger->info(
					__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) +
					", status: " + MMSEngineDBFacade::toString(newEncodingStatus) + ", processorMMS: " + "NULL" +
					", encodingJobEnd: " + "NOW() at time zone 'utc'" + ", encodingProgress: " + "100"
				);
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc', encodingProgress = 100 "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey, trans.quote(toString(EncodingStatus::Processing))
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
										  ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus) +
										  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
										  ", sqlStatement: " + sqlStatement;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				_logger->info(
					__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);
			}

			if (newEncodingStatus == EncodingStatus::End_Success)
			{
				// 2021-08-27:
				//	We are in EncoderVideoAudioProxy.cpp,
				//	In case it was added just a new encoding profile to a media item,
				//		isIngestionJobFinished will be true and the ingestion job status has to be updated
				//	In case it was created a new media item (i.e: OverlayImageOnVideo), the file was generated but
				//		the media item has still to be ingested. In this case the ingestion job status does NOT to be
				//		updated because it will be updated when the file will be ingested
				//		(inside the handleLocalAssetIngestionEvent method)
				if (isIngestionJobFinished && ingestionJobKey != -1)
				{
					IngestionStatus newIngestionStatus = IngestionStatus::End_TaskSuccess;

					string errorMessage;
					string processorMMS;
					_logger->info(
						__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
					);
					updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
				}
			}
			else if (newEncodingStatus == EncodingStatus::End_Failed && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_IngestionFailure;
				// string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(ingestionStatus) + ", physicalPathKey: " + to_string(physicalPathKey) +
					", errorMessage: " + ingestionErrorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, ingestionStatus, ingestionErrorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_KilledByUser && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByUser;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(ingestionStatus) + ", physicalPathKey: " + to_string(physicalPathKey) +
					", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_CanceledByUser && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByUser;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(ingestionStatus) + ", physicalPathKey: " + to_string(physicalPathKey) +
					", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_CanceledByMMS && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByMMS;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(ingestionStatus) + ", physicalPathKey: " + to_string(physicalPathKey) +
					", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}

			updateToBeTriedAgain = false;

			trans.commit();
			connectionPool->unborrow(conn);
			conn = nullptr;
		}
		catch (sql_error const &e)
		{
			// in caso di mysql avevamo una gestione di retry in caso di "Lock wait timeout exceeded"
			// Bisogna riportarla anche in Postgres?

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

	return encodingFailureNumber;
}

void MMSEngineDBFacade::updateIngestionAndEncodingLiveRecordingPeriod(
	int64_t ingestionJobKey, int64_t encodingJobKey, time_t utcRecordingPeriodStart, time_t utcRecordingPeriodEnd
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
		{
			_logger->info(
				__FILEREF__ + "IngestionJob update" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", JSON_SET...utcScheduleStart: " +
				to_string(utcRecordingPeriodStart) + ", JSON_SET....utcScheduleEnd: " + to_string(utcRecordingPeriodEnd)
			);
			// "RecordingPeriod" : { "AutoRenew" : true, "End" : "2020-05-10T02:00:00Z", "Start" : "2020-05-03T02:00:00Z" }
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set "
				"metaDataContent = jsonb_set(metaDataContent, '{{schedule,start}}', ('\"' || to_char(to_timestamp({}), 'YYYY-MM-DD') || 'T' || "
				"to_char(to_timestamp({}), 'HH24:MI:SS') || 'Z\"')::jsonb), "
				"metaDataContent = jsonb_set(metaDataContent, '{{schedule,end}}', ('\"' || to_char(to_timestamp({}), 'YYYY-MM-DD') || 'T' || "
				"to_char(to_timestamp({}), 'HH24:MI:SS') || 'Z\"')::jsonb) "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				utcRecordingPeriodStart, utcRecordingPeriodStart, utcRecordingPeriodEnd, utcRecordingPeriodEnd, ingestionJobKey
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
				// 2020-05-10: in case of 'high availability', this update will be done two times
				//	For this reason it is a warn below and no exception is raised
				string errorMessage =
					__FILEREF__ + "no ingestion update was done" + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart) +
					", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd) + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		{
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", JSON_SET...utcScheduleStart: " +
				to_string(utcRecordingPeriodStart) + ", JSON_SET....utcScheduleEnd: " + to_string(utcRecordingPeriodEnd)
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set encodingJobStart = NOW() at time zone 'utc', "
				"parameters = jsonb_set("
				"jsonb_set(parameters, '{{utcScheduleStart}}', jsonb '{}'), "
				"'{{utcScheduleEnd}}', jsonb '{}') "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				utcRecordingPeriodStart, utcRecordingPeriodEnd, encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart) +
									  ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
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

void MMSEngineDBFacade::updateEncodingJobPriority(shared_ptr<Workspace> workspace, int64_t encodingJobKey, EncodingPriority newEncodingPriority)
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
		EncodingStatus currentEncodingStatus;
		EncodingPriority currentEncodingPriority;
		{
			string sqlStatement = fmt::format(
				"select status, encodingPriority from MMS_EncodingJob "
				// "where encodingJobKey = ?";
				"where encodingJobKey = {}",
				encodingJobKey
			); // for update";
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
				currentEncodingStatus = toEncodingStatus(res[0]["status"].as<string>());
				currentEncodingPriority = static_cast<EncodingPriority>(res[0]["encodingPriority"].as<int>());
			}
			else
			{
				string errorMessage =
					__FILEREF__ + "EncodingJob not found" + ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (currentEncodingStatus != EncodingStatus::ToBeProcessed)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot change EncodingPriority because of his status" +
									  ", currentEncodingStatus: " + toString(currentEncodingStatus) +
									  ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (currentEncodingPriority == newEncodingPriority)
			{
				string errorMessage = __FILEREF__ + "EncodingJob has already the same status" +
									  ", currentEncodingStatus: " + toString(currentEncodingStatus) +
									  ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (static_cast<int>(currentEncodingPriority) > workspace->_maxEncodingPriority)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot be changed to an higher priority" +
									  ", currentEncodingPriority: " + toString(currentEncodingPriority) +
									  ", maxEncodingPriority: " + toString(static_cast<EncodingPriority>(workspace->_maxEncodingPriority)) +
									  ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", encodingPriority: " + to_string(static_cast<int>(newEncodingPriority))
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set encodingPriority = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				static_cast<int>(newEncodingPriority), encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", newEncodingPriority: " + toString(newEncodingPriority) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "EncodingJob updated successful" + ", newEncodingPriority: " + toString(newEncodingPriority) +
			", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingJobTryAgain(shared_ptr<Workspace> workspace, int64_t encodingJobKey)
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
		EncodingStatus currentEncodingStatus;
		int64_t ingestionJobKey;
		{
			string sqlStatement = fmt::format(
				"select status, ingestionJobKey from MMS_EncodingJob "
				// "where encodingJobKey = ?";
				"where encodingJobKey = {}",
				encodingJobKey
			); // for update";
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
				currentEncodingStatus = toEncodingStatus(res[0]["status"].as<string>());
				ingestionJobKey = res[0]["ingestionJobKey"].as<int64_t>();
			}
			else
			{
				string errorMessage =
					__FILEREF__ + "EncodingJob not found" + ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (currentEncodingStatus != EncodingStatus::End_Failed)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot be encoded again because of his status" +
									  ", currentEncodingStatus: " + toString(currentEncodingStatus) +
									  ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		EncodingStatus newEncodingStatus = EncodingStatus::ToBeProcessed;
		{
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set status = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(toString(newEncodingStatus)), encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", newEncodingStatus: " + toString(newEncodingStatus) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		_logger->info(
			__FILEREF__ + "EncodingJob updated successful" + ", newEncodingStatus: " + toString(newEncodingStatus) +
			", encodingJobKey: " + to_string(encodingJobKey)
		);

		IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;
		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set status = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(toString(newIngestionStatus)), ingestionJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", newEncodingStatus: " + toString(newEncodingStatus) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		_logger->info(
			__FILEREF__ + "IngestionJob updated successful" + ", newIngestionStatus: " + toString(newIngestionStatus) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
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

void MMSEngineDBFacade::forceCancelEncodingJob(int64_t ingestionJobKey)
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
			EncodingStatus encodingStatus = EncodingStatus::End_CanceledByUser;
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set status = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(toString(encodingStatus)), ingestionJobKey
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
				// 2020-05-24: It is not an error, so just comment next log
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingJobProgress(int64_t encodingJobKey, double encodingPercentage)
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
			/* 2020-05-24: commented because already logged by the calling method
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingProgress: " + to_string(encodingPercentage)
				);
			*/
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set encodingProgress = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				encodingPercentage, encodingJobKey
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
				// because encodingPercentage was already the same in the table
				// 2020-05-24: It is not an error, so just comment next log
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);
				*/

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

void MMSEngineDBFacade::updateEncodingRealTimeInfo(
	int64_t encodingJobKey, int encodingPid, long realTimeFrameRate, double realTimeBitRate, long numberOfRestartBecauseOfFailure
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
		{
			/* 2020-05-24: commented because already logged by the calling method
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingProgress: " + to_string(encodingPercentage)
				);
			*/
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set encodingPid = {}, realTimeFrameRate = {}, realTimeBitRate = {}, "
				"numberOfRestartBecauseOfFailure = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				encodingPid == -1 ? "null" : to_string(encodingPid), realTimeFrameRate == -1 ? "null" : to_string(realTimeFrameRate),
				realTimeBitRate == -1.0 ? "null" : to_string(realTimeBitRate), numberOfRestartBecauseOfFailure, encodingJobKey
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
				// because encodingPercentage was already the same in the table
				// 2020-05-24: It is not an error, so just comment next log
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);
				*/

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

bool MMSEngineDBFacade::updateEncodingJobFailuresNumber(int64_t encodingJobKey, long failuresNumber)
{
	bool isKilled;

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
				"select COALESCE(isKilled, false) as isKilled "
				"from MMS_EncodingJob "
				"where encodingJobKey = {}",
				encodingJobKey
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
				isKilled = res[0]["isKilled"].as<bool>();
			}
			else
			{
				string errorMessage =
					__FILEREF__ + "EncodingJob not found" + ", EncodingJobKey: " + to_string(encodingJobKey) + ", sqlStatement: " + sqlStatement;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) +
				", failuresNumber: " + to_string(failuresNumber)
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set failuresNumber = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				failuresNumber, encodingJobKey
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
				// in case it is alyways failing, it will be already 1
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);
				*/

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

	return isKilled;
}

void MMSEngineDBFacade::updateEncodingJobIsKilled(int64_t encodingJobKey, bool isKilled)
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
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", isKilled: " + to_string(isKilled)
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set isKilled = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				isKilled, encodingJobKey
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
				// in case it is alyways failing, it will be already 1
				/*
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", encodingPercentage: " + to_string(encodingPercentage)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);
				*/

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

void MMSEngineDBFacade::updateEncodingJobTranscoder(int64_t encodingJobKey, int64_t encoderKey, string stagingEncodedAssetPathName)
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
			_logger->info(
				__FILEREF__ + "EncodingJob update" + ", encodingJobKey: " + to_string(encodingJobKey) + ", encoderKey: " + to_string(encoderKey)
			);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set encoderKey = {}, "
				"stagingEncodedAssetPathName = {} where encodingJobKey = {} returning 1) select count(*) from rows",
				encoderKey, trans.quote(stagingEncodedAssetPathName), encodingJobKey
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
				// probable because encodingPercentage was already the same in the table
				string errorMessage = __FILEREF__ + "no update was done" + ", encoderKey: " + to_string(encoderKey) +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
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

void MMSEngineDBFacade::updateEncodingJobParameters(int64_t encodingJobKey, string parameters)
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
				"WITH rows AS (update MMS_EncodingJob set parameters = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(parameters), encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", parameters: " + parameters +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "EncodingJob updated successful" + ", parameters: " + parameters + ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateOutputRtmpAndPlaURL(int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, string rtmpURL, string playURL)
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
		// PlayUrl in MMS_IngestionJob per il play del canale
		{
			string path_playUrl = fmt::format("{{outputs,{},playUrl}}", outputIndex);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set "
				"metaDataContent = jsonb_set(metaDataContent, {}, jsonb {}) "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_playUrl), trans.quote("\"" + playURL + "\""), ingestionJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", playURL: " + playURL +
									  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		{
			string path_playUrl = fmt::format("{{outputsRoot,{},playUrl}}", outputIndex);
			string path_rtmpUrl = fmt::format("{{outputsRoot,{},rtmpUrl}}", outputIndex);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set "
				"parameters = jsonb_set("
				"jsonb_set(parameters, {}, jsonb {}), "
				"{}, jsonb {}) "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_playUrl), trans.quote("\"" + playURL + "\""), trans.quote(path_rtmpUrl), trans.quote("\"" + rtmpURL + "\""),
				encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", playURL: " + playURL + ", rtmpURL: " + rtmpURL +
									  ", encodingJobKey: " + to_string(encodingJobKey) + ", rowsUpdated: " + to_string(rowsUpdated) +
									  ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "EncodingJob updated successful" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", playURL: " + playURL + ", rtmpURL: " + rtmpURL
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

void MMSEngineDBFacade::updateOutputHLSDetails(
	int64_t ingestionJobKey, int64_t encodingJobKey, int outputIndex, int64_t deliveryCode, int segmentDurationInSeconds, int playlistEntriesNumber,
	string manifestDirectoryPath, string manifestFileName, string otherOutputOptions
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
		// PlayUrl in MMS_IngestionJob per il play del canale
		{
			// 2023-02-16: in caso di HLSChannel, non serve aggiornare il campo playURL in MMS_IngestionJob
			//	ma è sufficiente che ci sia il deliveryCode.
			//	Nello scenario di LiveRecording e monitor/virtualVOD, Outputs[outputIndex] non esiste.
			// Per questo motivo abbiamo IF nel SQL
			// 2023-08-03: in MMSEngineService.cpp ho aggiunto outputs in MMS_IngestionJob nel caso di monitor/virtualVOD
			//	perchè penso IF nel sql statement sotto non funzionava in alcuni casi (quando outputs non esisteva)
			//	Per cui ho semplificato il comando sotto
			string path_deliveryCode = fmt::format("{{outputs,{},deliveryCode}}", outputIndex);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_IngestionJob set "
				"metaDataContent = jsonb_set(metaDataContent, {}, jsonb {}) "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_deliveryCode), trans.quote(to_string(deliveryCode)), ingestionJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		{
			string path_deliveryCode = fmt::format("{{outputsRoot,{},deliveryCode}}", outputIndex);
			string path_segmentDuration = fmt::format("{{outputsRoot,{},segmentDurationInSeconds}}", outputIndex);
			string path_playlistEntries = fmt::format("{{outputsRoot,{},playlistEntriesNumber}}", outputIndex);
			string path_manifestDirectoryPath = fmt::format("{{outputsRoot,{},manifestDirectoryPath}}", outputIndex);
			string path_manifestFileName = fmt::format("{{outputsRoot,{},manifestFileName}}", outputIndex);
			string path_otherOutputOptions = fmt::format("{{outputsRoot,{},otherOutputOptions}}", outputIndex);
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_EncodingJob set "
				"parameters = ",
				trans.quote(path_deliveryCode), trans.quote(to_string(deliveryCode))
			);
			sqlStatement += "jsonb_set(";
			sqlStatement += "jsonb_set(";
			sqlStatement += "jsonb_set(";
			if (playlistEntriesNumber != -1)
				sqlStatement += "jsonb_set(";
			if (segmentDurationInSeconds != -1)
				sqlStatement += "jsonb_set(";
			sqlStatement +=
				fmt::format("jsonb_set(parameters, {}, jsonb {}), ", trans.quote(path_deliveryCode), trans.quote(to_string(deliveryCode)));
			if (segmentDurationInSeconds != -1)
				sqlStatement += fmt::format("{}, jsonb {}), ", trans.quote(path_segmentDuration), trans.quote(to_string(segmentDurationInSeconds)));
			if (playlistEntriesNumber != -1)
				sqlStatement += fmt::format("{}, jsonb {}), ", trans.quote(path_playlistEntries), trans.quote(to_string(playlistEntriesNumber)));
			sqlStatement += fmt::format(
				"{}, jsonb {}), "
				"{}, jsonb {}), "
				"{}, jsonb {}) "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_manifestDirectoryPath), trans.quote("\"" + manifestDirectoryPath + "\""), trans.quote(path_manifestFileName),
				trans.quote("\"" + manifestFileName + "\""), trans.quote(path_otherOutputOptions), trans.quote("\"" + otherOutputOptions + "\""),
				encodingJobKey
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
				string errorMessage = __FILEREF__ + "no update was done" + ", deliveryCode: " + to_string(deliveryCode) +
									  ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds) +
									  ", playlistEntriesNumber: " + to_string(playlistEntriesNumber) +
									  ", manifestDirectoryPath: " + manifestDirectoryPath + ", manifestFileName: " + manifestFileName +
									  ", otherOutputOptions: " + otherOutputOptions + ", encodingJobKey: " + to_string(encodingJobKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
			}
		}

		_logger->info(
			__FILEREF__ + "EncodingJob updated successful" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", encodingJobKey: " + to_string(encodingJobKey) + ", deliveryCode: " + to_string(deliveryCode) +
			", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds) + ", playlistEntriesNumber: " + to_string(playlistEntriesNumber) +
			", manifestDirectoryPath: " + manifestDirectoryPath + ", manifestFileName: " + manifestFileName +
			", otherOutputOptions: " + otherOutputOptions
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

pair<int64_t, int64_t> MMSEngineDBFacade::encodingJob_EncodingJobKeyEncoderKey(int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encodingjob:.encodingJobKey", "mms_encodingjob:.encoderKey"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, -1, ingestionJobKey, fromMaster);

		int64_t encodingJobKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<int64_t>(-1) : -1;
		int64_t encoderKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][1].as<int64_t>(-1) : -1;

		return make_pair(encodingJobKey, encoderKey);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			ingestionJobKey, fromMaster
		);

		throw e;
	}
}

tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus>
MMSEngineDBFacade::encodingJob_IngestionJobKeyTypeEncoderKeyStatus(int64_t encodingJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {
			"mms_encodingjob:.ingestionJobKey", "mms_encodingjob:.type", "mms_encodingjob:.encoderKey", "mms_encodingjob:.status"
		};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, encodingJobKey, -1, fromMaster);

		int64_t ingestionJobKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<int64_t>(-1) : -1;
		string type = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][1].as<string>(string("")) : "";
		int64_t encoderKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][2].as<int64_t>(-1) : -1;
		EncodingStatus status =
			sqlResultSet->size() > 0 ? toEncodingStatus((*sqlResultSet)[0][3].as<string>(string(""))) : EncodingStatus::Processing;

		return make_tuple(ingestionJobKey, type, encoderKey, status);
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", encodingJobKey: {}"
			", exceptionMessage: {}",
			encodingJobKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encodingJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encodingJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encodingJobKey: {}"
			", fromMaster: {}",
			encodingJobKey, fromMaster
		);

		throw e;
	}
}

tuple<int64_t, int64_t, json> MMSEngineDBFacade::encodingJob_EncodingJobKeyEncoderKeyParameters(int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encodingjob:.encodingJobKey", "mms_encodingjob:.encoderKey", "mms_encodingjob:.parameters"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, -1, ingestionJobKey, fromMaster);

		if (sqlResultSet->empty())
		{
			string errorMessage = fmt::format(
				"encodingJob not found"
				", ingestionJobKey: {}",
				ingestionJobKey
			);
			// abbiamo il log nel catch
			// SPDLOG_WARN(errorMessage);

			throw DBRecordNotFound(errorMessage);
		}

		int64_t encodingJobKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<int64_t>(-1) : -1;
		int64_t encoderKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][1].as<int64_t>(-1) : -1;
		json parametersRoot = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][2].as<json>(json()) : json();

		return make_tuple(encodingJobKey, encoderKey, parametersRoot);
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", ingestionJobKey: {}"
			", exceptionMessage: {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			ingestionJobKey, fromMaster
		);

		throw e;
	}
}

pair<int64_t, json> MMSEngineDBFacade::encodingJob_EncodingJobKeyParameters(int64_t ingestionJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encodingjob:.encodingJobKey", "mms_encodingjob:.parameters"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, -1, ingestionJobKey, fromMaster);

		if (sqlResultSet->empty())
		{
			string errorMessage = fmt::format(
				"encodingJob not found"
				", ingestionJobKey: {}",
				ingestionJobKey
			);
			// abbiamo il log nel catch
			// SPDLOG_WARN(errorMessage);

			throw DBRecordNotFound(errorMessage);
		}

		int64_t encodingJobKey = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<int64_t>(-1) : -1;
		json parametersRoot = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][1].as<json>(json()) : json();

		return make_pair(encodingJobKey, parametersRoot);
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", ingestionJobKey: {}"
			", exceptionMessage: {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", ingestionJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			ingestionJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", ingestionJobKey: {}"
			", fromMaster: {}",
			ingestionJobKey, fromMaster
		);

		throw e;
	}
}

json MMSEngineDBFacade::encodingJob_columnAsJson(string columnName, int64_t encodingJobKey, bool fromMaster)
{
	try
	{
		string requestedColumn = fmt::format("mms_encodingjob:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, encodingJobKey, -1, fromMaster);

		return sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<json>(json()) : json();
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", exceptionMessage: {}",
			e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encodingJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encodingJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encodingJobKey: {}"
			", fromMaster: {}",
			encodingJobKey, fromMaster
		);

		throw e;
	}
}

/*
json MMSEngineDBFacade::encodingJob_Parameters(int64_t encodingJobKey, bool fromMaster)
{
	try
	{
		vector<string> requestedColumns = {"mms_encodingjob:.parameters"};
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = encodingJobQuery(requestedColumns, encodingJobKey, -1, fromMaster);

		json parametersRoot = sqlResultSet->size() > 0 ? (*sqlResultSet)[0][0].as<json>(json()) : json();

		return parametersRoot;
	}
	catch (DBRecordNotFound &e)
	{
		// il chiamante decidera se loggarlo come error
		SPDLOG_WARN(
			"NotFound exception"
			", exceptionMessage: {}",
			e.what()
		);

		throw e;
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", encodingJobKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			encodingJobKey, fromMaster, e.what()
		);

		throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", encodingJobKey: {}"
			", fromMaster: {}",
			encodingJobKey, fromMaster
		);

		throw e;
	}
}
*/

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::encodingJobQuery(
	vector<string> &requestedColumns, int64_t encodingJobKey, int64_t ingestionJobKey, bool fromMaster, int startIndex, int rows, string orderBy,
	bool notFoundAsException
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
			if (encodingJobKey != -1)
				where += fmt::format("{} encodingJobKey = {} ", where.size() > 0 ? "and" : "", encodingJobKey);
			if (ingestionJobKey != -1)
				where += fmt::format("{} ingestionJobKey = {} ", where.size() > 0 ? "and" : "", ingestionJobKey);

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
				"from MMS_EncodingJob "
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

			if (empty(res) && encodingJobKey != -1 && notFoundAsException)
			{
				string errorMessage = fmt::format(
					"encodingJob not found"
					", encodingJobKey: {}",
					encodingJobKey
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

json MMSEngineDBFacade::getEncodingJobsStatus(
	shared_ptr<Workspace> workspace, int64_t encodingJobKey, int start, int rows,
	// bool startAndEndIngestionDatePresent,
	string startIngestionDate, string endIngestionDate,
	// bool startAndEndEncodingDatePresent,
	string startEncodingDate, string endEncodingDate, int64_t encoderKey,

	// 2021-01-29: next parameter is used ONLY if encoderKey != -1
	// The goal is the, if the user from a GUI asks for the encoding jobs of a specific encoder,
	// wants to know how the encoder is loaded and, to know that, he need to know also the encoding jobs
	// running on that encoder from other workflows.
	// So, if alsoEncodingJobsFromOtherWorkspaces is true and encoderKey != -1, we will send all the encodingJobs
	// running on that encoder.
	// In this case, for the one not belonging to the current workspace, we will not fill
	// the ingestionJobKey, so it is not possible to retrieve information by GUI like 'title media, ...'
	bool alsoEncodingJobsFromOtherWorkspaces,

	bool asc, string status, string types, bool fromMaster
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

			if (encodingJobKey != -1)
			{
				field = "encodingJobKey";
				requestParametersRoot[field] = encodingJobKey;
			}

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

			if (startEncodingDate != "")
			{
				field = "startEncodingDate";
				requestParametersRoot[field] = startEncodingDate;
			}
			if (endEncodingDate != "")
			{
				field = "endEncodingDate";
				requestParametersRoot[field] = endEncodingDate;
			}

			if (encoderKey != -1)
			{
				field = "encoderKey";
				requestParametersRoot[field] = encoderKey;
			}

			field = "alsoEncodingJobsFromOtherWorkspaces";
			requestParametersRoot[field] = alsoEncodingJobsFromOtherWorkspaces;

			field = "status";
			requestParametersRoot[field] = status;

			if (types != "")
			{
				field = "types";
				requestParametersRoot[field] = types;
			}

			field = "requestParameters";
			statusListRoot[field] = requestParametersRoot;
		}

		// manage types
		vector<string> vTypes;
		string typesArgument;
		if (types != "")
		{
			stringstream ss(types);
			string type;
			char delim = ','; // types comma separator
			while (getline(ss, type, delim))
			{
				if (!type.empty())
				{
					vTypes.push_back(type);
					if (typesArgument == "")
						typesArgument = trans.quote(type);
					else
						typesArgument += (", " + trans.quote(type));
				}
			}
		}

		string sqlWhere = string("where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey ");
		if (alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
			;
		else
			sqlWhere += fmt::format("and ir.workspaceKey = {} ", workspace->_workspaceKey);
		if (encodingJobKey != -1)
			sqlWhere += fmt::format("and ej.encodingJobKey = {} ", encodingJobKey);
		// if (startAndEndIngestionDatePresent)
		//     sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and
		//     ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (startIngestionDate != "")
			sqlWhere += fmt::format("and ir.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startIngestionDate));
		if (endIngestionDate != "")
			sqlWhere += fmt::format("and ir.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endIngestionDate));
		// if (startAndEndEncodingDatePresent)
		//     sqlWhere += ("and ej.encodingJobStart >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and
		//     ej.encodingJobStart <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
		if (startEncodingDate != "")
			sqlWhere += fmt::format("and ej.encodingJobStart >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(startEncodingDate));
		if (endEncodingDate != "")
			sqlWhere += fmt::format("and ej.encodingJobStart <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.quote(endEncodingDate));
		if (encoderKey != -1)
			sqlWhere += fmt::format("and ej.encoderKey = {} ", encoderKey);
		if (status == "All")
			;
		else if (status == "Completed")											   // like non va bene per motivi di performance
			sqlWhere += ("and ej.status not in ('ToBeProcessed', 'Processing') "); // like 'End_%' ");
		else if (status == "Processing")
			sqlWhere += ("and ej.status = 'Processing' ");
		else if (status == "ToBeProcessed")
			sqlWhere += ("and ej.status = 'ToBeProcessed' ");
		if (types != "")
		{
			if (vTypes.size() == 1)
				sqlWhere += fmt::format("and ej.type = {} ", trans.quote(types));
			else
				sqlWhere += ("and ej.type in (" + typesArgument + ")");
		}

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej {}", sqlWhere);
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

		json encodingJobsRoot = json::array();
		{
			string sqlStatement = fmt::format(
				"select ir.workspaceKey, ej.encodingJobKey, ij.ingestionJobKey, ej.type, ej.parameters, "
				"ej.status, ej.encodingProgress, ej.processorMMS, ej.encoderKey, ej.encodingPid, "
				"ej.realTimeFrameRate, ej.realTimeBitRate, ej.numberOfRestartBecauseOfFailure, ej.failuresNumber, ej.encodingPriority, "
				"to_char(ej.encodingJobStart, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobStart, "
				"to_char(ej.encodingJobEnd, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobEnd, "
				"case when ij.startProcessing IS NULL then NOW() at time zone 'utc' else ij.startProcessing end as newStartProcessing, "
				"case when ij.endProcessing IS NULL then NOW() at time zone 'utc' else ij.endProcessing end as newEndProcessing "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej {} "
				"order by newStartProcessing {}, newEndProcessing {} "
				"limit {} offset {}",
				sqlWhere, asc ? "asc" : "desc", asc ? "asc " : "desc", rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json encodingJobRoot;

				int64_t workspaceKey = row["workspaceKey"].as<int64_t>();

				bool ownedByCurrentWorkspace;
				if (alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
				{
					if (workspaceKey == workspace->_workspaceKey)
						ownedByCurrentWorkspace = true;
					else
						ownedByCurrentWorkspace = false;
				}
				else
					ownedByCurrentWorkspace = true;

				field = "ownedByCurrentWorkspace";
				encodingJobRoot[field] = ownedByCurrentWorkspace;

				int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();

				field = "encodingJobKey";
				encodingJobRoot[field] = encodingJobKey;

				// if (ownedByCurrentWorkspace)
				{
					field = "ingestionJobKey";
					encodingJobRoot[field] = row["ingestionJobKey"].as<int64_t>();
				}
				/*
				else
				{
					// see comment above (2021-01-29)

					field = "ingestionJobKey";
					encodingJobRoot[field] = nullptr;
				}
				*/

				field = "type";
				encodingJobRoot[field] = row["type"].as<string>();

				// if (ownedByCurrentWorkspace)
				{
					string parameters = row["parameters"].as<string>();

					json parametersRoot;
					if (parameters != "")
						parametersRoot = JSONUtils::toJson(parameters);

					field = "parameters";
					encodingJobRoot[field] = parametersRoot;
				}
				/*
				else
				{
					field = "parameters";
					encodingJobRoot[field] = nullptr;
				}
				*/

				field = "status";
				encodingJobRoot[field] = row["status"].as<string>();
				EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(row["status"].as<string>());

				field = "progress";
				if (row["encodingProgress"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = row["encodingProgress"].as<float>();

				field = "start";
				if (encodingStatus == EncodingStatus::ToBeProcessed)
					encodingJobRoot[field] = nullptr;
				else
				{
					if (row["encodingJobStart"].is_null())
						encodingJobRoot[field] = nullptr;
					else
						encodingJobRoot[field] = row["encodingJobStart"].as<string>();
				}

				field = "end";
				if (row["encodingJobEnd"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = row["encodingJobEnd"].as<string>();

				field = "processorMMS";
				if (row["processorMMS"].is_null())
					encodingJobRoot[field] = nullptr;
				else
					encodingJobRoot[field] = row["processorMMS"].as<string>();

				field = "encoderKey";
				if (row["encoderKey"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = row["encoderKey"].as<int64_t>();

				field = "encodingPid";
				if (row["encodingPid"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = row["encodingPid"].as<int64_t>();

				field = "realTimeFrameRate";
				if (row["realTimeFrameRate"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = row["realTimeFrameRate"].as<int64_t>();

				field = "realTimeBitRate";
				if (row["realTimeBitRate"].is_null())
					encodingJobRoot[field] = -1.0;
				else
					encodingJobRoot[field] = row["realTimeBitRate"].as<float>();

				field = "numberOfRestartBecauseOfFailure";
				if (row["numberOfRestartBecauseOfFailure"].is_null())
					encodingJobRoot[field] = -1;
				else
					encodingJobRoot[field] = row["numberOfRestartBecauseOfFailure"].as<int64_t>();

				field = "failuresNumber";
				encodingJobRoot[field] = row["failuresNumber"].as<int>();

				field = "encodingPriority";
				encodingJobRoot[field] = toString(static_cast<EncodingPriority>(row["encodingPriority"].as<int>()));

				field = "encodingPriorityCode";
				encodingJobRoot[field] = row["encodingPriority"].as<int>();

				field = "maxEncodingPriorityCode";
				encodingJobRoot[field] = workspace->_maxEncodingPriority;

				encodingJobsRoot.push_back(encodingJobRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "encodingJobs";
		responseRoot[field] = encodingJobsRoot;

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

void MMSEngineDBFacade::fixEncodingJobsHavingWrongStatus()
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
		long totalRowsUpdated = 0;
		int maxRetriesOnError = 2;
		int currentRetriesOnError = 0;
		bool toBeExecutedAgain = true;
		while (toBeExecutedAgain)
		{
			try
			{
				// Scenarios: IngestionJob in final status but EncodingJob not in final status
				//	This is independently by the specific instance of mms-engine (because in this scenario
				//	often the processor field is empty) but someone has to do it
				//	This scenario may happen in case the mms-engine is shutdown not in friendly way
				string sqlStatement =
					"select ij.ingestionJobKey, ej.encodingJobKey, "
					"ij.status as ingestionJobStatus, ej.status as encodingJobStatus "
					"from MMS_IngestionJob ij, MMS_EncodingJob ej "
					"where ij.ingestionJobKey = ej.ingestionJobKey "
					// like non va bene per motivi di performance
					"and ij.status not in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
					"'SourceUploadingInProgress', 'EncodingQueued') "	 // like 'End_%' "
					"and ej.status in ('ToBeProcessed', 'Processing') "; // not like 'End_%'";
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.exec(sqlStatement);
				for (auto row : res)
				{
					int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
					int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();
					string ingestionJobStatus = row["ingestionJobStatus"].as<string>();
					string encodingJobStatus = row["encodingJobStatus"].as<string>();

					{
						string errorMessage = string("Found EncodingJob with wrong status") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
											  ", encodingJobKey: " + to_string(encodingJobKey) + ", ingestionJobStatus: " + ingestionJobStatus +
											  ", encodingJobStatus: " + encodingJobStatus;
						_logger->error(__FILEREF__ + errorMessage);

						updateEncodingJob(
							encodingJobKey, EncodingError::CanceledByMMS,
							false, // isIngestionJobFinished: this field is not used by updateEncodingJob
							ingestionJobKey, errorMessage
						);

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
				currentRetriesOnError++;
				if (currentRetriesOnError >= maxRetriesOnError)
					throw e;

				// Deadlock!!!
				SPDLOG_ERROR(
					"SQL exception"
					", query: {}"
					", exceptionMessage: {}"
					", conn: {}",
					e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
				);

				{
					int secondsBetweenRetries = 15;
					_logger->info(
						__FILEREF__ + "fixEncodingJobsHavingWrongStatus failed, " + "waiting before to try again" +
						", currentRetriesOnError: " + to_string(currentRetriesOnError) + ", maxRetriesOnError: " + to_string(maxRetriesOnError) +
						", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
		}

		_logger->info(__FILEREF__ + "fixEncodingJobsHavingWrongStatus " + ", totalRowsUpdated: " + to_string(totalRowsUpdated));

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

void MMSEngineDBFacade::addEncodingJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, MMSEngineDBFacade::ContentType contentType, EncodingPriority encodingPriority,
	int64_t encodingProfileKey, json encodingProfileDetailsRoot,

	json sourcesToBeEncodedRoot,

	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
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
		EncodingType encodingType;
		if (contentType == ContentType::Image)
			encodingType = EncodingType::EncodeImage;
		else
			encodingType = EncodingType::EncodeVideoAudio;

		string parameters;
		{
			json parametersRoot;

			string field = "contentType";
			parametersRoot[field] = MMSEngineDBFacade::toString(contentType);

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "sourcesToBeEncoded";
			parametersRoot[field] = sourcesToBeEncodedRoot;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, "
				"processorMMS, encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, "
				"creationDate) values ("
				"DEFAULT,         {},              {},   {},           {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL,             {}, "
				"NULL,         NULL,       NULL,        NULL,              NULL,            NULL,                            0,				NOW() at "
				"time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				regex_replace(sqlStatement, regex("\n"), " "), conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_OverlayImageOnVideoJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
	int64_t sourceVideoMediaItemKey, int64_t sourceVideoPhysicalPathKey, int64_t videoDurationInMilliSeconds, string mmsSourceVideoAssetPathName,
	string sourceVideoPhysicalDeliveryURL, string sourceVideoFileExtension, int64_t sourceImageMediaItemKey, int64_t sourceImagePhysicalPathKey,
	string mmsSourceImageAssetPathName, string sourceImagePhysicalDeliveryURL, string sourceVideoTranscoderStagingAssetPathName,
	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, EncodingPriority encodingPriority,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
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
		EncodingType encodingType = EncodingType::OverlayImageOnVideo;

		string parameters;
		{
			json parametersRoot;

			string field = "sourceVideoMediaItemKey";
			parametersRoot[field] = sourceVideoMediaItemKey;

			field = "sourceVideoPhysicalPathKey";
			parametersRoot[field] = sourceVideoPhysicalPathKey;

			field = "videoDurationInMilliSeconds";
			parametersRoot[field] = videoDurationInMilliSeconds;

			field = "mmsSourceVideoAssetPathName";
			parametersRoot[field] = mmsSourceVideoAssetPathName;

			field = "sourceVideoPhysicalDeliveryURL";
			parametersRoot[field] = sourceVideoPhysicalDeliveryURL;

			field = "sourceVideoFileExtension";
			parametersRoot[field] = sourceVideoFileExtension;

			field = "sourceImageMediaItemKey";
			parametersRoot[field] = sourceImageMediaItemKey;

			field = "sourceImagePhysicalPathKey";
			parametersRoot[field] = sourceImagePhysicalPathKey;

			field = "mmsSourceImageAssetPathName";
			parametersRoot[field] = mmsSourceImageAssetPathName;

			field = "sourceImagePhysicalDeliveryURL";
			parametersRoot[field] = sourceImagePhysicalDeliveryURL;

			field = "sourceVideoTranscoderStagingAssetPathName";
			parametersRoot[field] = sourceVideoTranscoderStagingAssetPathName;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},  {},		   {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_OverlayTextOnVideoJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority,

	int64_t encodingProfileKey, json encodingProfileDetailsRoot,

	string sourceAssetPathName, int64_t sourceDurationInMilliSeconds, string sourcePhysicalDeliveryURL, string sourceFileExtension,

	string sourceTranscoderStagingAssetPathName, string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
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
		EncodingType encodingType = EncodingType::OverlayTextOnVideo;

		string parameters;
		{
			json parametersRoot;

			string field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "sourceAssetPathName";
			parametersRoot[field] = sourceAssetPathName;

			field = "sourceDurationInMilliSeconds";
			parametersRoot[field] = sourceDurationInMilliSeconds;

			field = "sourcePhysicalDeliveryURL";
			parametersRoot[field] = sourcePhysicalDeliveryURL;

			field = "sourceFileExtension";
			parametersRoot[field] = sourceFileExtension;

			field = "sourceTranscoderStagingAssetPathName";
			parametersRoot[field] = sourceTranscoderStagingAssetPathName;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}
		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_GenerateFramesJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, EncodingPriority encodingPriority,

	string nfsImagesDirectory, string transcoderStagingImagesDirectory, string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName,
	string sourceAssetPathName, int64_t sourceVideoPhysicalPathKey, string sourceFileExtension, string sourceFileName,
	int64_t videoDurationInMilliSeconds, double startTimeInSeconds, int maxFramesNumber, string videoFilter, int periodInSeconds, bool mjpeg,
	int imageWidth, int imageHeight, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL
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
		EncodingType encodingType = EncodingType::GenerateFrames;

		string parameters;
		{
			json parametersRoot;

			string field = "ingestionJobKey";
			parametersRoot[field] = ingestionJobKey;

			field = "nfsImagesDirectory";
			parametersRoot[field] = nfsImagesDirectory;

			field = "transcoderStagingImagesDirectory";
			parametersRoot[field] = transcoderStagingImagesDirectory;

			field = "sourcePhysicalDeliveryURL";
			parametersRoot[field] = sourcePhysicalDeliveryURL;

			field = "sourceTranscoderStagingAssetPathName";
			parametersRoot[field] = sourceTranscoderStagingAssetPathName;

			field = "sourceAssetPathName";
			parametersRoot[field] = sourceAssetPathName;

			field = "sourceVideoPhysicalPathKey";
			parametersRoot[field] = sourceVideoPhysicalPathKey;

			field = "sourceFileExtension";
			parametersRoot[field] = sourceFileExtension;

			field = "sourceFileName";
			parametersRoot[field] = sourceFileName;

			field = "videoDurationInMilliSeconds";
			parametersRoot[field] = videoDurationInMilliSeconds;

			field = "startTimeInSeconds";
			parametersRoot[field] = startTimeInSeconds;

			field = "maxFramesNumber";
			parametersRoot[field] = maxFramesNumber;

			field = "videoFilter";
			parametersRoot[field] = videoFilter;

			field = "periodInSeconds";
			parametersRoot[field] = periodInSeconds;

			field = "mjpeg";
			parametersRoot[field] = mjpeg;

			field = "imageWidth";
			parametersRoot[field] = imageWidth;

			field = "imageHeight";
			parametersRoot[field] = imageHeight;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_SlideShowJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t encodingProfileKey, json encodingProfileDetailsRoot, string targetFileFormat,
	json imagesRoot, json audiosRoot, float shortestAudioDurationInSeconds, string encodedTranscoderStagingAssetPathName,
	string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
	EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::SlideShow;

		string parameters;
		{
			json parametersRoot;

			string field = "encodingProfileDetailsRoot";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "targetFileFormat";
			parametersRoot[field] = targetFileFormat;

			field = "imagesRoot";
			parametersRoot[field] = imagesRoot;

			field = "audiosRoot";
			parametersRoot[field] = audiosRoot;

			field = "shortestAudioDurationInSeconds";
			parametersRoot[field] = shortestAudioDurationInSeconds;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, "
				"parameters, encodingPriority, encodingJobStart, encodingJobEnd, "
				"encodingProgress, status, processorMMS, encoderKey, "
				"encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {}, "
				"{},          {},              NOW() at time zone 'utc', NULL, "
				"NULL,             {},      NULL,         NULL, "
				"NULL,        NULL,              NULL,            NULL,                             0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_FaceRecognitionJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourceVideoPhysicalPathKey,
	string sourcePhysicalPath, string faceRecognitionCascadeName, string faceRecognitionOutput, EncodingPriority encodingPriority,
	long initialFramesNumberToBeSkipped, bool oneFramePerSecond
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
		EncodingType encodingType = EncodingType::FaceRecognition;

		string parameters = string() + "{ " + "\"sourceMediaItemKey\": " + to_string(sourceMediaItemKey) +
							", \"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey) + ", \"sourcePhysicalPath\": \"" +
							sourcePhysicalPath + "\"" + ", \"faceRecognitionCascadeName\": \"" + faceRecognitionCascadeName + "\"" +
							", \"faceRecognitionOutput\": \"" + faceRecognitionOutput + "\"" +
							", \"initialFramesNumberToBeSkipped\": " + to_string(initialFramesNumberToBeSkipped) +
							", \"oneFramePerSecond\": " + to_string(oneFramePerSecond) + "} ";

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                             0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_FaceIdentificationJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string sourcePhysicalPath, string faceIdentificationCascadeName,
	string deepLearnedModelTagsCommaSeparated, EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::FaceIdentification;

		string parameters = string() + "{ " + "\"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\"" + ", \"faceIdentificationCascadeName\": \"" +
							faceIdentificationCascadeName + "\"" + ", \"deepLearnedModelTagsCommaSeparated\": " + deepLearnedModelTagsCommaSeparated +
							"" + "} ";

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_LiveRecorderJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, string ingestionJobLabel, string streamSourceType,

	string configurationLabel, int64_t confKey, string liveURL, string encodersPoolLabel, EncodingPriority encodingPriority,

	int pushListenTimeout, int64_t pushEncoderKey, json captureRoot, json tvRoot,

	bool monitorHLS, bool liveRecorderVirtualVOD, int monitorVirtualVODOutputRootIndex,

	json outputsRoot, json framesToBeDetectedRoot,

	string chunksTranscoderStagingContentsPath, string chunksNFSStagingContentsPath, string segmentListFileName, string recordedFileNamePrefix,
	string virtualVODStagingContentsPath, string virtualVODTranscoderStagingContentsPath, int64_t liveRecorderVirtualVODImageMediaItemKey,

	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL
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
		_logger->info(
			__FILEREF__ + "addEncoding_LiveRecorderJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", ingestionJobLabel: " + ingestionJobLabel + ", streamSourceType: " + streamSourceType + ", configurationLabel: " + configurationLabel +
			", confKey: " + to_string(confKey) + ", liveURL: " + liveURL + ", encodingPriority: " + toString(encodingPriority) +
			", monitorHLS: " + to_string(monitorHLS) + ", liveRecorderVirtualVOD: " + to_string(liveRecorderVirtualVOD) +
			", outputsRoot.size: " + (outputsRoot != nullptr ? to_string(outputsRoot.size()) : to_string(0))
		);

		{
			EncodingType encodingType = EncodingType::LiveRecorder;

			string parameters;
			{
				json parametersRoot;

				string field = "ingestionJobLabel";
				parametersRoot[field] = ingestionJobLabel;

				field = "streamSourceType";
				parametersRoot[field] = streamSourceType;

				field = "configurationLabel";
				parametersRoot[field] = configurationLabel;

				field = "confKey";
				parametersRoot[field] = confKey;

				field = "liveURL";
				parametersRoot[field] = liveURL;

				field = "encodersPoolLabel";
				parametersRoot[field] = encodersPoolLabel;

				field = "pushListenTimeout";
				parametersRoot[field] = pushListenTimeout;

				field = "pushEncoderKey";
				parametersRoot[field] = pushEncoderKey;

				field = "capture";
				parametersRoot[field] = captureRoot;

				field = "tv";
				parametersRoot[field] = tvRoot;

				field = "monitorHLS";
				parametersRoot[field] = monitorHLS;

				field = "liveRecorderVirtualVOD";
				parametersRoot[field] = liveRecorderVirtualVOD;

				field = "monitorVirtualVODOutputRootIndex";
				parametersRoot[field] = monitorVirtualVODOutputRootIndex;

				// 2023-08-03: ho provato a eliminare Root dalla label. Il problema è che non posso cambiare
				//	outputsRoot da tutti gli update di MMS_EncodingJob in quanto LiveProxy, Countdown, VODProxy
				//	usano outputsRoot. Questi ultimi non li posso cambiare perchè dovrei far ripartire tutti
				//	i canali di CiborTV
				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

				field = "framesToBeDetected";
				parametersRoot[field] = framesToBeDetectedRoot;

				field = "chunksTranscoderStagingContentsPath";
				parametersRoot[field] = chunksTranscoderStagingContentsPath;

				field = "chunksNFSStagingContentsPath";
				parametersRoot[field] = chunksNFSStagingContentsPath;

				field = "segmentListFileName";
				parametersRoot[field] = segmentListFileName;

				field = "recordedFileNamePrefix";
				parametersRoot[field] = recordedFileNamePrefix;

				field = "virtualVODStagingContentsPath";
				parametersRoot[field] = virtualVODStagingContentsPath;

				field = "virtualVODTranscoderStagingContentsPath";
				parametersRoot[field] = virtualVODTranscoderStagingContentsPath;

				field = "liveRecorderVirtualVODImageMediaItemKey";
				parametersRoot[field] = liveRecorderVirtualVODImageMediaItemKey;

				field = "mmsWorkflowIngestionURL";
				parametersRoot[field] = mmsWorkflowIngestionURL;

				field = "mmsBinaryIngestionURL";
				parametersRoot[field] = mmsBinaryIngestionURL;

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				/*
				int savedEncodingPriority = static_cast<int>(encodingPriority);
				if (savedEncodingPriority > workspace->_maxEncodingPriority)
				{
					_logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
						+ ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
						+ ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
					);

					savedEncodingPriority = workspace->_maxEncodingPriority;
				}
				*/
				// 2019-04-23: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format(
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) "
					"values ("
					"DEFAULT,        {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
					"'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
					savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		int64_t backupEncodingJobKey = -1;

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

void MMSEngineDBFacade::addEncoding_LiveProxyJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, string streamSourceType, int64_t utcProxyPeriodStart,
	// long maxAttemptsNumberInCaseOfErrors,
	long waitingSecondsBetweenAttemptsInCaseOfErrors, json outputsRoot, string mmsWorkflowIngestionURL
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
		_logger->info(
			__FILEREF__ + "addEncoding_LiveProxyJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", streamSourceType: " +
			streamSourceType + ", waitingSecondsBetweenAttemptsInCaseOfErrors: " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors) +
			", outputsRoot.size: " + to_string(outputsRoot.size())
		);

		{
			EncodingType encodingType = EncodingType::LiveProxy;

			string parameters;
			{
				json parametersRoot;

				string field = "inputsRoot";
				parametersRoot[field] = inputsRoot;

				field = "streamSourceType";
				parametersRoot[field] = streamSourceType;

				// field = "utcProxyPeriodStart";
				// parametersRoot[field] = utcProxyPeriodStart;
				field = "utcScheduleStart";
				parametersRoot[field] = utcProxyPeriodStart;

				// field = "maxAttemptsNumberInCaseOfErrors";
				// parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

				field = "mmsWorkflowIngestionURL";
				parametersRoot[field] = mmsWorkflowIngestionURL;

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format(
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) "
					"values ("
					"DEFAULT,        {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
					"'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
					savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

			// int64_t encodingJobKey = getLastInsertId(conn);

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_VODProxyJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, int64_t utcProxyPeriodStart, json outputsRoot,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors, string mmsWorkflowIngestionURL
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
		_logger->info(
			__FILEREF__ + "addEncoding_VODProxyJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
			", outputsRoot.size: " + to_string(outputsRoot.size())
		);

		{
			EncodingType encodingType = EncodingType::VODProxy;

			string parameters;
			{
				json parametersRoot;

				string field = "inputsRoot";
				parametersRoot[field] = inputsRoot;

				// field = "utcProxyPeriodStart";
				// parametersRoot[field] = utcProxyPeriodStart;
				field = "utcScheduleStart";
				parametersRoot[field] = utcProxyPeriodStart;

				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

				field = "mmsWorkflowIngestionURL";
				parametersRoot[field] = mmsWorkflowIngestionURL;

				parameters = JSONUtils::toString(parametersRoot);
			}

			// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
			//	will be managed as soon as possible
			int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

			{
				string sqlStatement = fmt::format(
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) "
					"values ("
					"DEFAULT,        {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
					"'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
					savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

			// int64_t encodingJobKey = getLastInsertId(conn);

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_CountdownJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputsRoot, int64_t utcProxyPeriodStart, json outputsRoot,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors, string mmsWorkflowIngestionURL
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
		{
			EncodingType encodingType = EncodingType::Countdown;

			string parameters;
			{
				json parametersRoot;

				string field = "inputsRoot";
				parametersRoot[field] = inputsRoot;

				// field = "utcProxyPeriodStart";
				// parametersRoot[field] = utcProxyPeriodStart;
				field = "utcScheduleStart";
				parametersRoot[field] = utcProxyPeriodStart;

				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

				field = "mmsWorkflowIngestionURL";
				parametersRoot[field] = mmsWorkflowIngestionURL;

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format(
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) "
					"values ("
					"DEFAULT,        {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
					"'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
					savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

			// int64_t encodingJobKey = getLastInsertId(conn);

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_LiveGridJob(shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json inputChannelsRoot, json outputsRoot)
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
		_logger->info(__FILEREF__ + "addEncoding_LiveGridJob" + ", ingestionJobKey: " + to_string(ingestionJobKey));

		{
			EncodingType encodingType = EncodingType::LiveGrid;

			string parameters;
			{
				json parametersRoot;

				string field;

				field = "inputChannels";
				parametersRoot[field] = inputChannelsRoot;

				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format(
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) "
					"values ("
					"DEFAULT,        {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
					"'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
					savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

			// int64_t encodingJobKey = getLastInsertId(conn);

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(
					__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
				);
				updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_VideoSpeed(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey, string sourceAssetPathName,
	int64_t sourceDurationInMilliSeconds, string sourceFileExtension, string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName,
	int64_t encodingProfileKey, json encodingProfileDetailsRoot, string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL, EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::VideoSpeed;

		string parameters;
		{
			json parametersRoot;

			string field;

			field = "sourceMediaItemKey";
			parametersRoot[field] = sourceMediaItemKey;

			field = "sourcePhysicalPathKey";
			parametersRoot[field] = sourcePhysicalPathKey;

			field = "sourceAssetPathName";
			parametersRoot[field] = sourceAssetPathName;

			field = "sourceDurationInMilliSeconds";
			parametersRoot[field] = sourceDurationInMilliSeconds;

			field = "sourceFileExtension";
			parametersRoot[field] = sourceFileExtension;

			field = "sourcePhysicalDeliveryURL";
			parametersRoot[field] = sourcePhysicalDeliveryURL;

			field = "sourceTranscoderStagingAssetPathName";
			parametersRoot[field] = sourceTranscoderStagingAssetPathName;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_AddSilentAudio(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, json sourcesRoot, int64_t encodingProfileKey, json encodingProfileDetailsRoot,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL, EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::AddSilentAudio;

		string parameters;
		{
			json parametersRoot;

			string field;

			field = "sources";
			parametersRoot[field] = sourcesRoot;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_PictureInPictureJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, int64_t mainSourceMediaItemKey, int64_t mainSourcePhysicalPathKey,
	string mainSourceAssetPathName, int64_t mainSourceDurationInMilliSeconds, string mainSourceFileExtension, string mainSourcePhysicalDeliveryURL,
	string mainSourceTranscoderStagingAssetPathName, int64_t overlaySourceMediaItemKey, int64_t overlaySourcePhysicalPathKey,
	string overlaySourceAssetPathName, int64_t overlaySourceDurationInMilliSeconds, string overlaySourceFileExtension,
	string overlaySourcePhysicalDeliveryURL, string overlaySourceTranscoderStagingAssetPathName, bool soundOfMain, int64_t encodingProfileKey,
	json encodingProfileDetailsRoot, string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL, EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::PictureInPicture;

		string parameters;
		{
			json parametersRoot;

			string field;

			field = "mainSourceMediaItemKey";
			parametersRoot[field] = mainSourceMediaItemKey;

			field = "mainSourcePhysicalPathKey";
			parametersRoot[field] = mainSourcePhysicalPathKey;

			field = "mainSourceAssetPathName";
			parametersRoot[field] = mainSourceAssetPathName;

			field = "mainSourceDurationInMilliSeconds";
			parametersRoot[field] = mainSourceDurationInMilliSeconds;

			field = "mainSourceFileExtension";
			parametersRoot[field] = mainSourceFileExtension;

			field = "mainSourcePhysicalDeliveryURL";
			parametersRoot[field] = mainSourcePhysicalDeliveryURL;

			field = "mainSourceTranscoderStagingAssetPathName";
			parametersRoot[field] = mainSourceTranscoderStagingAssetPathName;

			field = "overlaySourceMediaItemKey";
			parametersRoot[field] = overlaySourceMediaItemKey;

			field = "overlaySourcePhysicalPathKey";
			parametersRoot[field] = overlaySourcePhysicalPathKey;

			field = "overlaySourceAssetPathName";
			parametersRoot[field] = overlaySourceAssetPathName;

			field = "overlaySourceDurationInMilliSeconds";
			parametersRoot[field] = overlaySourceDurationInMilliSeconds;

			field = "overlaySourceFileExtension";
			parametersRoot[field] = overlaySourceFileExtension;

			field = "overlaySourcePhysicalDeliveryURL";
			parametersRoot[field] = overlaySourcePhysicalDeliveryURL;

			field = "overlaySourceTranscoderStagingAssetPathName";
			parametersRoot[field] = overlaySourceTranscoderStagingAssetPathName;

			field = "soundOfMain";
			parametersRoot[field] = soundOfMain;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        NULL,              NULL,            NULL,                            0,			  NOW() at time zone "
				"'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_IntroOutroOverlayJob(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,

	int64_t encodingProfileKey, json encodingProfileDetailsRoot,

	int64_t introSourcePhysicalPathKey, string introSourceAssetPathName, string introSourceFileExtension, int64_t introSourceDurationInMilliSeconds,
	string introSourcePhysicalDeliveryURL, string introSourceTranscoderStagingAssetPathName,

	int64_t mainSourcePhysicalPathKey, string mainSourceAssetPathName, string mainSourceFileExtension, int64_t mainSourceDurationInMilliSeconds,
	string mainSourcePhysicalDeliveryURL, string mainSourceTranscoderStagingAssetPathName,

	int64_t outroSourcePhysicalPathKey, string outroSourceAssetPathName, string outroSourceFileExtension, int64_t outroSourceDurationInMilliSeconds,
	string outroSourcePhysicalDeliveryURL, string outroSourceTranscoderStagingAssetPathName,

	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL,
	string mmsIngestionURL,

	EncodingPriority encodingPriority
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
		EncodingType encodingType = EncodingType::IntroOutroOverlay;
		string parameters;
		{
			json parametersRoot;

			string field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "introSourcePhysicalPathKey";
			parametersRoot[field] = introSourcePhysicalPathKey;

			field = "introSourceAssetPathName";
			parametersRoot[field] = introSourceAssetPathName;

			field = "introSourceFileExtension";
			parametersRoot[field] = introSourceFileExtension;

			field = "introSourceDurationInMilliSeconds";
			parametersRoot[field] = introSourceDurationInMilliSeconds;

			field = "introSourcePhysicalDeliveryURL";
			parametersRoot[field] = introSourcePhysicalDeliveryURL;

			field = "introSourceTranscoderStagingAssetPathName";
			parametersRoot[field] = introSourceTranscoderStagingAssetPathName;

			field = "mainSourcePhysicalPathKey";
			parametersRoot[field] = mainSourcePhysicalPathKey;

			field = "mainSourceAssetPathName";
			parametersRoot[field] = mainSourceAssetPathName;

			field = "mainSourceFileExtension";
			parametersRoot[field] = mainSourceFileExtension;

			field = "mainSourceDurationInMilliSeconds";
			parametersRoot[field] = mainSourceDurationInMilliSeconds;

			field = "mainSourcePhysicalDeliveryURL";
			parametersRoot[field] = mainSourcePhysicalDeliveryURL;

			field = "mainSourceTranscoderStagingAssetPathName";
			parametersRoot[field] = mainSourceTranscoderStagingAssetPathName;

			field = "outroSourcePhysicalPathKey";
			parametersRoot[field] = outroSourcePhysicalPathKey;

			field = "outroSourceAssetPathName";
			parametersRoot[field] = outroSourceAssetPathName;

			field = "outroSourceFileExtension";
			parametersRoot[field] = outroSourceFileExtension;

			field = "outroSourceDurationInMilliSeconds";
			parametersRoot[field] = outroSourceDurationInMilliSeconds;

			field = "outroSourcePhysicalDeliveryURL";
			parametersRoot[field] = outroSourcePhysicalDeliveryURL;

			field = "outroSourceTranscoderStagingAssetPathName";
			parametersRoot[field] = outroSourceTranscoderStagingAssetPathName;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, "
				"status, processorMMS, encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, "
				"creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL, "
				"{},      NULL,         NULL,       NULL,        NULL,              NULL,           NULL,                            0,			    "
				"NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

void MMSEngineDBFacade::addEncoding_CutFrameAccurate(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,

	int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey, string sourceAssetPathName, int64_t sourceDurationInMilliSeconds,
	string sourceFileExtension, string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName, string endTime,

	int64_t encodingProfileKey, json encodingProfileDetailsRoot,

	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName, string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL,
	string mmsIngestionURL,

	EncodingPriority encodingPriority, int64_t newUtcStartTimeInMilliSecs, int64_t newUtcEndTimeInMilliSecs
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
		EncodingType encodingType = EncodingType::CutFrameAccurate;
		string parameters;
		{
			json parametersRoot;

			string field = "sourceMediaItemKey";
			parametersRoot[field] = sourceMediaItemKey;

			field = "sourcePhysicalPathKey";
			parametersRoot[field] = sourcePhysicalPathKey;

			field = "sourceAssetPathName";
			parametersRoot[field] = sourceAssetPathName;

			field = "sourceDurationInMilliSeconds";
			parametersRoot[field] = sourceDurationInMilliSeconds;

			field = "sourceFileExtension";
			parametersRoot[field] = sourceFileExtension;

			field = "sourcePhysicalDeliveryURL";
			parametersRoot[field] = sourcePhysicalDeliveryURL;

			field = "sourceTranscoderStagingAssetPathName";
			parametersRoot[field] = sourceTranscoderStagingAssetPathName;

			field = "endTime";
			parametersRoot[field] = endTime;

			field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetailsRoot";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "encodedTranscoderStagingAssetPathName";
			parametersRoot[field] = encodedTranscoderStagingAssetPathName;

			field = "encodedNFSStagingAssetPathName";
			parametersRoot[field] = encodedNFSStagingAssetPathName;

			field = "mmsWorkflowIngestionURL";
			parametersRoot[field] = mmsWorkflowIngestionURL;

			field = "mmsBinaryIngestionURL";
			parametersRoot[field] = mmsBinaryIngestionURL;

			field = "mmsIngestionURL";
			parametersRoot[field] = mmsIngestionURL;

			field = "newUtcStartTimeInMilliSecs";
			parametersRoot[field] = newUtcStartTimeInMilliSecs;

			field = "newUtcEndTimeInMilliSecs";
			parametersRoot[field] = newUtcEndTimeInMilliSecs;

			parameters = JSONUtils::toString(parametersRoot);
		}

		{
			int savedEncodingPriority = static_cast<int>(encodingPriority);
			if (savedEncodingPriority > workspace->_maxEncodingPriority)
			{
				_logger->warn(
					__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer" +
					", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority) +
					", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
				);

				savedEncodingPriority = workspace->_maxEncodingPriority;
			}

			string sqlStatement = fmt::format(
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, "
				"status, processorMMS, encoderKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure, failuresNumber, "
				"creationDate) values ("
				"DEFAULT,        {},               {},    {},			  {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL, "
				"{},      NULL,         NULL,       NULL,        NULL,              NULL,           NULL,                            0,				"
				"NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType), trans.quote(parameters),
				savedEncodingPriority, trans.quote(toString(EncodingStatus::ToBeProcessed))
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

		{
			IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

			string errorMessage;
			string processorMMS;
			_logger->info(
				__FILEREF__ + "Update IngestionJob" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", IngestionStatus: " + toString(newIngestionStatus) + ", errorMessage: " + errorMessage + ", processorMMS: " + processorMMS
			);
			updateIngestionJob(conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::getEncodingTypePriority(MMSEngineDBFacade::EncodingType encodingType)
{
	// The priority is used when engine retrieves the encoding jobs to be executed

	if (encodingType == EncodingType::LiveProxy || encodingType == EncodingType::VODProxy || encodingType == EncodingType::Countdown)
		return 1;
	else if (encodingType == EncodingType::LiveRecorder)
		return 5;
	else
		return 10;
}
