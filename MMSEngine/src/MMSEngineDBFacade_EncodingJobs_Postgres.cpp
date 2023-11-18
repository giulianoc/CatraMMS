
#include "JSONUtils.h"
#include "PersistenceLock.h"
#include "MMSEngineDBFacade.h"


void MMSEngineDBFacade::getEncodingJobs(
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems,
		int timeBeforeToPrepareResourcesInMinutes,
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

		_logger->info(__FILEREF__ + "getEncodingJobs"
			+ ", initialGetEncodingJobsCurrentIndex: " + to_string(initialGetEncodingJobsCurrentIndex)
		);

		bool stillRows = true;
		while(encodingItems.size() < maxEncodingsNumber && stillRows)
        {
			_logger->info(__FILEREF__ + "getEncodingJobs (before select)"
				+ ", _getEncodingJobsCurrentIndex: " + to_string(_getEncodingJobsCurrentIndex)
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
					"ej.utcScheduleStart_virtual - unix_timestamp() < {} * 60) "
				"order by ej.typePriority asc, ej.utcScheduleStart_virtual asc, "
					"ej.encodingPriority desc, ej.creationDate asc, ej.failuresNumber asc "
				"limit {} offset {} for update skip locked",
				trans.quote(MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)),
				timeBeforeToPrepareResourcesInMinutes,
				maxEncodingsNumber, _getEncodingJobsCurrentIndex
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);

			_getEncodingJobsCurrentIndex += maxEncodingsNumber;
			if (res.size() != maxEncodingsNumber)
				stillRows = false;

			_logger->info(__FILEREF__ + "getEncodingJobs (after select)"
				+ ", _getEncodingJobsCurrentIndex: " + to_string(_getEncodingJobsCurrentIndex)
				+ ", encodingResultSet->rowsCount: " + to_string(res.size())
			);
			int resultSetIndex = 0;
			for (auto row: res)
            {
				int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();

                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
					make_shared<MMSEngineDBFacade::EncodingItem>();

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
					encodingItem->_stagingEncodedAssetPathName =
						row["stagingEncodedAssetPathName"].as<string>();

				string encodingParameters = row["parameters"].as<string>();

				_logger->info(__FILEREF__ + "getEncodingJobs (resultSet loop)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
					+ ", initialGetEncodingJobsCurrentIndex: " + to_string(initialGetEncodingJobsCurrentIndex)
					+ ", resultSetIndex: " + to_string(resultSetIndex) + "/" + to_string(res.size())
				);
				resultSetIndex++;

                {
                    string sqlStatement = fmt::format( 
                        "select ir.workspaceKey "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey "
                        "and ij.ingestionJobKey = {}",
						encodingItem->_ingestionJobKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					SPDLOG_INFO("SQL statement"
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
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                + ", sqlStatement: " + sqlStatement
                        ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            string sqlStatement = fmt::format( 
                                "WITH rows AS (update MMS_EncodingJob set status = {}, "
								"encodingJobEnd = NOW() at time zone 'utc' "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
							SPDLOG_INFO("SQL statement"
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
				_logger->info(__FILEREF__ + "getEncodingJobs (after workspaceKey)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
				);

				{
					tuple<string, MMSEngineDBFacade::IngestionType, MMSEngineDBFacade::IngestionStatus,
						string, string> ingestionJobDetails = getIngestionJobDetails(
						encodingItem->_workspace->_workspaceKey, encodingItem->_ingestionJobKey,
						// 2022-12-18: probable the ingestionJob is added recently, let's set true
						true);

					IngestionStatus ingestionJobStatus;
					tie(ignore, ignore, ingestionJobStatus, ignore, ignore) = ingestionJobDetails;

					string sIngestionJobStatus = toString(ingestionJobStatus);
					string prefix = "End_";
					if (sIngestionJobStatus.size() >= prefix.size()
						&& 0 == sIngestionJobStatus.compare(0, prefix.size(), prefix))
					{
						string errorMessage = string("Found EncodingJob with wrong status")
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobStatus: " + sIngestionJobStatus
							// + ", encodingJobStatus: " + encodingJobStatus
						;
						_logger->error(__FILEREF__ + errorMessage);

						// 2122-01-05: updateEncodingJob is not used because the row is already locked
						// updateEncodingJob (                                                                       
						// 	encodingJobKey,                                                                       
						// 	EncodingError::CanceledByMMS,                                                         
						// 	false,  // isIngestionJobFinished: this field is not used by updateEncodingJob        
						// 	ingestionJobKey,                                                                      
						// 	errorMessage                                                                          
						// );
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_CanceledByMMS)
							);
                            string sqlStatement = fmt::format( 
                                "WITH rows AS (update MMS_EncodingJob set status = {}, "
								"encodingJobEnd = NOW() at time zone 'utc' "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_CanceledByMMS)),
								encodingItem->_encodingJobKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
							SPDLOG_INFO("SQL statement"
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
				_logger->info(__FILEREF__ + "getEncodingJobs (after check ingestionStatus)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
				);

				try
                {
					encodingItem->_encodingParametersRoot = JSONUtils::toJson(-1, encodingJobKey,
						encodingParameters);
                }
				catch (runtime_error e)
                {
					_logger->error(__FILEREF__ + e.what());

					// in case an encoding job row generate an error, we have to make it to Failed
					// otherwise we will indefinitely get this error
					{
						_logger->info(__FILEREF__ + "EncodingJob update"
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
						);
						string sqlStatement = fmt::format( 
							"WITH rows AS (update MMS_EncodingJob set status = {} "
							"where encodingJobKey = {} returning 1) select count(*) from rows",
							trans.quote(toString(EncodingStatus::End_Failed)), encodingItem->_encodingJobKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
						SPDLOG_INFO("SQL statement"
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

				_logger->info(__FILEREF__ + "getEncodingJobs (after encodingParameters)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
				);
                
				// encodingItem->_ingestedParametersRoot
				{
					string sqlStatement = fmt::format( 
						"select metaDataContent from MMS_IngestionJob where ingestionJobKey = {}",
						encodingItem->_ingestionJobKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					SPDLOG_INFO("SQL statement"
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
							encodingItem->_ingestedParametersRoot =
								JSONUtils::toJson(encodingItem->_ingestionJobKey, -1, ingestionParameters);
						}
						catch(runtime_error& e)
						{
							_logger->error(e.what());

							// in case an encoding job row generate an error, we have to make it to Failed
							// otherwise we will indefinitely get this error
							{
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								);
								string sqlStatement = fmt::format( 
									"WITH rows AS (update MMS_EncodingJob set status = {} "
									"where encodingJobKey = {} returning 1) select count(*) from rows",
									trans.quote(toString(EncodingStatus::End_Failed)),
									encodingItem->_encodingJobKey);
								chrono::system_clock::time_point startSql = chrono::system_clock::now();
								int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
								SPDLOG_INFO("SQL statement"
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
						string errorMessage = __FILEREF__ + "select failed, no row returned"
							+ ", encodingItem->_ingestionJobKey: "
								+ to_string(encodingItem->_ingestionJobKey)
							+ ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						// in case an encoding job row generate an error, we have to make it to Failed
						// otherwise we will indefinitely get this error
						{
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
							string sqlStatement = fmt::format( 
								"WITH rows AS (update MMS_EncodingJob set status = {} "
								"where encodingJobKey = {} returning 1) select count(*) from rows",
								trans.quote(toString(EncodingStatus::End_Failed)),
								encodingItem->_encodingJobKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
							SPDLOG_INFO("SQL statement"
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
				_logger->info(__FILEREF__ + "getEncodingJobs"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
				);

                encodingItems.push_back(encodingItem);
				othersToBeEncoded++;

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
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
							trans.quote(toString(EncodingStatus::Processing)),
							trans.quote(processorMMS), encodingItem->_encodingJobKey);
					else
						sqlStatement = fmt::format( 
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = {}"
							", encodingJobStart = NOW() at time zone 'utc' "
							"where encodingJobKey = {} and processorMMS is null "
							"returning 1) select count(*) from rows",
							trans.quote(toString(EncodingStatus::Processing)),
							trans.quote(processorMMS), encodingItem->_encodingJobKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", sqlStatement: " + sqlStatement
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
				_logger->info(__FILEREF__ + "getEncodingJobs (after encodingJob update)"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", encodingType: " + encodingType
				);
            }
        }

		if (encodingItems.size() < maxEncodingsNumber)
			_getEncodingJobsCurrentIndex = 0;

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        chrono::system_clock::time_point endPoint = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "getEncodingJobs statistics"
			+ ", encodingItems.size: " + to_string(encodingItems.size())
			+ ", maxEncodingsNumber: " + to_string(maxEncodingsNumber)
			+ ", liveProxyToBeEncoded: " + to_string(liveProxyToBeEncoded)
			+ ", liveRecorderToBeEncoded: " + to_string(liveRecorderToBeEncoded)
			+ ", othersToBeEncoded: " + to_string(othersToBeEncoded)
			+ ", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
        );
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

int MMSEngineDBFacade::updateEncodingJob (
	int64_t encodingJobKey,
	EncodingError encodingError,
	bool isIngestionJobFinished,
	int64_t ingestionJobKey,
	string ingestionErrorMessage,
	bool forceEncodingToBeFailed)
{
	int encodingFailureNumber;
    
	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	bool updateToBeTriedAgain = true;
	int retriesNumber = 0;
	int maxRetriesNumber = 3;
	int secondsBetweenRetries = 5;
	while(updateToBeTriedAgain && retriesNumber < maxRetriesNumber)
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
						"where encodingJobKey = {}", encodingJobKey);
						// "where encodingJobKey = ? for update";
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					SPDLOG_INFO("SQL statement"
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
						string errorMessage = __FILEREF__ + "EncodingJob not found"
                            + ", EncodingJobKey: " + to_string(encodingJobKey)
                            + ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                    
					}
				}
            
				{
					string sqlStatement;
					// in case of LiveRecorder there is no more retries since it already run up
					//		to the end of the recording
					if (forceEncodingToBeFailed
						|| encodingFailureNumber + 1 >= _maxEncodingFailures)
					{
						newEncodingStatus          = EncodingStatus::End_Failed;

						_logger->info(__FILEREF__ + "update EncodingJob"
							+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
							+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						);
						sqlStatement = fmt::format( 
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, failuresNumber = {}, "
							"encodingProgress = NULL where encodingJobKey = {} and status = {} "
							"returning 1) select count(*) from rows",
							trans.quote(toString(newEncodingStatus)), encodingFailureNumber,
							encodingJobKey, trans.quote(toString(EncodingStatus::Processing)));
					}
					else
					{
						newEncodingStatus          = EncodingStatus::ToBeProcessed;
						encodingFailureNumber++;

						sqlStatement = fmt::format( 
							"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, encoderKey = NULL, "
							"failuresNumber = {}, encodingProgress = NULL where encodingJobKey = {} "
							"and status = {} returning 1) select count(*) from rows",
							trans.quote(toString(newEncodingStatus)), encodingFailureNumber,
							encodingJobKey, trans.quote(toString(EncodingStatus::Processing)));
					}
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
					SPDLOG_INFO("SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, conn->getConnectionId(),
						chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done"
                            + ", MMSEngineDBFacade::toString(newEncodingStatus): "
								+ MMSEngineDBFacade::toString(newEncodingStatus)
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                    
					}
				}
            
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::MaxCapacityReached
				|| encodingError == EncodingError::ErrorBeforeEncoding)
			{
				newEncodingStatus       = EncodingStatus::ToBeProcessed;
            
				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", encoderKey = NULL"
					+ ", encodingProgress: " + "NULL"
					);
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encoderKey = NULL, encodingProgress = NULL "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey,
					trans.quote(toString(EncodingStatus::Processing)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::KilledByUser)
			{
				newEncodingStatus       = EncodingStatus::End_KilledByUser;
            
				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", encodingJobEnd: " + "NOW() at time zone 'utc'"
					);
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey,
					trans.quote(toString(EncodingStatus::Processing)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::CanceledByUser)
			{
				newEncodingStatus       = EncodingStatus::End_CanceledByUser;
            
				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", encodingJobEnd: " + "NOW() at time zone 'utc'"
					);
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else if (encodingError == EncodingError::CanceledByMMS)
			{
				newEncodingStatus       = EncodingStatus::End_CanceledByMMS;
            
				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", encodingJobEnd: " + "NOW() at time zone 'utc'"
					);
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc' "
					"where encodingJobKey = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
				);
			}
			else    // success
			{
				newEncodingStatus       = EncodingStatus::End_Success;

				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", encodingJobEnd: " + "NOW() at time zone 'utc'"
					+ ", encodingProgress: " + "100"
					);
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_EncodingJob set status = {}, processorMMS = NULL, "
					"encodingJobEnd = NOW() at time zone 'utc', encodingProgress = 100 "
					"where encodingJobKey = {} and status = {} returning 1) select count(*) from rows",
					trans.quote(toString(newEncodingStatus)), encodingJobKey,
					trans.quote(toString(EncodingStatus::Processing)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
				_logger->info(__FILEREF__ + "EncodingJob updated successful"
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
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
					_logger->info(__FILEREF__ + "Update IngestionJob"
						+ ", ingestionJobKey: " + to_string(ingestionJobKey)
						+ ", IngestionStatus: " + toString(newIngestionStatus)
						+ ", errorMessage: " + errorMessage
						+ ", processorMMS: " + processorMMS
					);                            
					updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
				}
			}
			else if (newEncodingStatus == EncodingStatus::End_Failed && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_IngestionFailure;
				// string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(ingestionStatus)
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", errorMessage: " + ingestionErrorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, ingestionStatus, ingestionErrorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_KilledByUser && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByUser;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(ingestionStatus)
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_CanceledByUser && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByUser;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(ingestionStatus)
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}
			else if (newEncodingStatus == EncodingStatus::End_CanceledByMMS && ingestionJobKey != -1)
			{
				IngestionStatus ingestionStatus = IngestionStatus::End_CanceledByMMS;
				string errorMessage;
				string processorMMS;
				int64_t physicalPathKey = -1;

				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(ingestionStatus)
					+ ", physicalPathKey: " + to_string(physicalPathKey)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, ingestionStatus, errorMessage);
			}

			updateToBeTriedAgain = false;

			trans.commit();
			connectionPool->unborrow(conn);
			conn = nullptr;
		}
		catch(sql_error const &e)
		{
			// in caso di mysql avevamo una gestione di retry in caso di "Lock wait timeout exceeded"
			// Bisogna riportarla anche in Postgres?

			SPDLOG_ERROR("SQL exception"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);

			try
			{
				trans.abort();
			}
			catch (exception& e)
			{
				SPDLOG_ERROR("abort failed"
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
		catch(exception& e)
		{
			SPDLOG_ERROR("exception"
				", conn: {}",
				(conn != nullptr ? conn->getConnectionId() : -1)
			);

			try
			{
				trans.abort();
			}
			catch (exception& e)
			{
				SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateIngestionAndEncodingLiveRecordingPeriod (
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd)
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
			_logger->info(__FILEREF__ + "IngestionJob update"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", JSON_SET...utcScheduleStart: " + to_string(utcRecordingPeriodStart)
				+ ", JSON_SET....utcScheduleEnd: " + to_string(utcRecordingPeriodEnd)
				);
			// "RecordingPeriod" : { "AutoRenew" : true, "End" : "2020-05-10T02:00:00Z", "Start" : "2020-05-03T02:00:00Z" }
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_IngestionJob set "
				"metaDataContent = jsonb_set(metaDataContent, '{{schedule,start}}', ('\"' || to_char(to_timestamp({}), 'YYYY-MM-DD') || 'T' || to_char(to_timestamp({}), 'HH24:MI:SS') || 'Z\"')::jsonb), "
				"metaDataContent = jsonb_set(metaDataContent, '{{schedule,end}}', ('\"' || to_char(to_timestamp({}), 'YYYY-MM-DD') || 'T' || to_char(to_timestamp({}), 'HH24:MI:SS') || 'Z\"')::jsonb) "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				utcRecordingPeriodStart, utcRecordingPeriodStart,
				utcRecordingPeriodEnd, utcRecordingPeriodEnd, ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
				// 2020-05-10: in case of 'high availability', this update will be done two times
				//	For this reason it is a warn below and no exception is raised
                string errorMessage = __FILEREF__ + "no ingestion update was done"
					+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
					+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
				;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
            }
        }

        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", JSON_SET...utcScheduleStart: " + to_string(utcRecordingPeriodStart)
				+ ", JSON_SET....utcScheduleEnd: " + to_string(utcRecordingPeriodEnd)
				);
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_EncodingJob set encodingJobStart = NOW() at time zone 'utc', "
				"parameters = jsonb_set(parameters, '{{utcScheduleStart}}', jsonb '{}'), "
				"parameters = jsonb_set(parameters, '{{utcScheduleEnd}}', jsonb '{}') "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				utcRecordingPeriodStart, utcRecordingPeriodEnd, encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
                        + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobPriority (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey,
    EncodingPriority newEncodingPriority)
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
				"where encodingJobKey = {}", encodingJobKey);		// for update";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
            {
				currentEncodingStatus = toEncodingStatus(res[0]["status"].as<string>());
				currentEncodingPriority = static_cast<EncodingPriority>(res[0]["encodingPriority"].as<int>());
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            
			if (currentEncodingStatus != EncodingStatus::ToBeProcessed)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot change EncodingPriority because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}

			if (currentEncodingPriority == newEncodingPriority)
			{
				string errorMessage = __FILEREF__ + "EncodingJob has already the same status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}

			if (static_cast<int>(currentEncodingPriority) > workspace->_maxEncodingPriority)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot be changed to an higher priority"
                    + ", currentEncodingPriority: " + toString(currentEncodingPriority)
                    + ", maxEncodingPriority: " + toString(static_cast<EncodingPriority>(workspace->_maxEncodingPriority))
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingPriority: " + to_string(static_cast<int>(newEncodingPriority))
				);
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_EncodingJob set encodingPriority = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				static_cast<int>(newEncodingPriority), encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingPriority: " + toString(newEncodingPriority)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", newEncodingPriority: " + toString(newEncodingPriority)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobTryAgain (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey)
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
				"where encodingJobKey = {}", encodingJobKey);	//for update";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
            {
				currentEncodingStatus = toEncodingStatus(res[0]["status"].as<string>());
				ingestionJobKey = res[0]["ingestionJobKey"].as<int64_t>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

			if (currentEncodingStatus != EncodingStatus::End_Failed)
			{
				string errorMessage = __FILEREF__ + "EncodingJob cannot be encoded again because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
        }
            
        EncodingStatus newEncodingStatus = EncodingStatus::ToBeProcessed;
        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
				);
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_EncodingJob set status = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(toString(newEncodingStatus)), encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingStatus: " + toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", newEncodingStatus: " + toString(newEncodingStatus)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );
        
            
        IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;
        {            
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_IngestionJob set status = {} "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(toString(newIngestionStatus)), ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingStatus: " + toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::forceCancelEncodingJob(
	int64_t ingestionJobKey)
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
				trans.quote(toString(encodingStatus)), ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage)
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
				encodingPercentage, encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingPid (
        int64_t encodingJobKey,
        int encodingPid)
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
                "WITH rows AS (update MMS_EncodingJob set encodingPid = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				encodingPid == -1 ? "null" : to_string(encodingPid), encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

bool MMSEngineDBFacade::updateEncodingJobFailuresNumber (
	int64_t encodingJobKey,
	long failuresNumber)
{
	bool		isKilled;

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
				"select case when isKilled is null then false else isKilled end as isKilled "
				"from MMS_EncodingJob "
				"where encodingJobKey = {}", encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
            {
				isKilled = res[0]["isKilled"].as<bool>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", failuresNumber: " + to_string(failuresNumber)
				);
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_EncodingJob set failuresNumber = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				failuresNumber, encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobIsKilled (
        int64_t encodingJobKey,
        bool isKilled)
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
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", isKilled: " + to_string(isKilled)
				);
			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_EncodingJob set isKilled = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				isKilled, encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
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
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobTranscoder (
	int64_t encodingJobKey,
	int64_t encoderKey,
	string stagingEncodedAssetPathName)
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
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encoderKey: " + to_string(encoderKey)
				);
            string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_EncodingJob set encoderKey = {}, "
				"stagingEncodedAssetPathName = {} where encodingJobKey = {} returning 1) select count(*) from rows",
				encoderKey, trans.quote(stagingEncodedAssetPathName), encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateEncodingJobParameters (
	int64_t encodingJobKey,
	string parameters
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
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_EncodingJob set parameters = {} "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(parameters), encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", parameters: " + parameters
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", parameters: " + parameters
            + ", encodingJobKey: " + to_string(encodingJobKey)
            );

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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


void MMSEngineDBFacade::updateOutputRtmpAndPlaURL (
	int64_t ingestionJobKey, int64_t encodingJobKey,
	int outputIndex, string rtmpURL, string playURL
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
			string path_playUrl = fmt::format("{{outputs,{},playUrl}}", outputIndex);
            string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_IngestionJob set "
				"metaDataContent = jsonb_set(metaDataContent, {}, jsonb {}) "
				"where ingestionJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_playUrl), trans.quote("\"" + playURL  + "\""), ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", playURL: " + playURL
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        {
			string path_playUrl = fmt::format("{{outputsRoot,{},playUrl}}", outputIndex);
			string path_rtmpUrl = fmt::format("{{outputsRoot,{},rtmpUrl}}", outputIndex);
            string sqlStatement = fmt::format(
                "WITH rows AS (update MMS_EncodingJob set "
				"parameters = jsonb_set(parameters, {}, jsonb {}), "
				"parameters = jsonb_set(parameters, {}, jsonb {}) "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_playUrl), trans.quote("\"" + playURL + "\""),
				trans.quote(path_rtmpUrl), trans.quote("\"" + rtmpURL + "\""),
				encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", playURL: " + playURL
					+ ", rtmpURL: " + rtmpURL
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", playURL: " + playURL
			+ ", rtmpURL: " + rtmpURL
            );

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::updateOutputHLSDetails (
	int64_t ingestionJobKey, int64_t encodingJobKey,
	int outputIndex, int64_t deliveryCode, int segmentDurationInSeconds, int playlistEntriesNumber,
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
				trans.quote(path_deliveryCode), trans.quote(to_string(deliveryCode)),
				ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
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
				"parameters = jsonb_set(parameters, {}, jsonb {}), ",
				trans.quote(path_deliveryCode), trans.quote(to_string(deliveryCode))
			);
			if (segmentDurationInSeconds != -1)
				sqlStatement += fmt::format(
					"parameters = jsonb_set(parameters, {}, jsonb {}), ",
					trans.quote(path_segmentDuration), trans.quote(to_string(segmentDurationInSeconds))
				);
			if (playlistEntriesNumber != -1)
				sqlStatement += fmt::format(
					"parameters = jsonb_set(parameters, {}, jsonb {}), ",
					trans.quote(path_playlistEntries), trans.quote(to_string(playlistEntriesNumber))
				);
			sqlStatement += fmt::format(
				"parameters = jsonb_set(parameters, {}, jsonb {}), "
				"parameters = jsonb_set(parameters, {}, jsonb {}), "
				"parameters = jsonb_set(parameters, {}, jsonb {}) "
				"where encodingJobKey = {} returning 1) select count(*) from rows",
				trans.quote(path_manifestDirectoryPath), trans.quote("\"" + manifestDirectoryPath + "\""),
				trans.quote(path_manifestFileName), trans.quote("\"" + manifestFileName + "\""),
				trans.quote(path_otherOutputOptions), trans.quote("\"" + otherOutputOptions + "\""),
				encodingJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", deliveryCode: " + to_string(deliveryCode)
					+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
					+ ", playlistEntriesNumber: " + to_string(playlistEntriesNumber)
					+ ", manifestDirectoryPath: " + manifestDirectoryPath
					+ ", manifestFileName: " + manifestFileName
					+ ", otherOutputOptions: " + otherOutputOptions
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }
        
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", deliveryCode: " + to_string(deliveryCode)
			+ ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
			+ ", playlistEntriesNumber: " + to_string(playlistEntriesNumber)
			+ ", manifestDirectoryPath: " + manifestDirectoryPath
			+ ", manifestFileName: " + manifestFileName
			+ ", otherOutputOptions: " + otherOutputOptions
		);

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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


tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus, string>
	MMSEngineDBFacade::getEncodingJobDetails (int64_t encodingJobKey, bool fromMaster)
{
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
		int64_t		ingestionJobKey;
		string      type;
		int64_t		encoderKey;
		string		parameters;
		// default initialization, important in case the calling methid calls the tie function
		EncodingStatus	status = EncodingStatus::ToBeProcessed;
        {
            string sqlStatement = fmt::format( 
                "select ingestionJobKey, type, encoderKey, status, parameters "
				"from MMS_EncodingJob "
				"where encodingJobKey = {}", encodingJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (!empty(res))
            {
				ingestionJobKey = res[0]["ingestionJobKey"].as<int64_t>();
				type = res[0]["type"].as<string>();
				if (res[0]["encoderKey"].is_null())
					encoderKey = -1;
				else
					encoderKey = res[0]["encoderKey"].as<int64_t>();
                status = toEncodingStatus(res[0]["status"].as<string>());
                parameters = res[0]["parameters"].as<string>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		// return make_tuple(ingestionJobKey, type, encoderKey, status, highAvailability, main,
		//		theOtherEncoderKey, theOtherStatus, theOtherEncodingJobKey);
		return make_tuple(ingestionJobKey, type, encoderKey, status, parameters);
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

tuple<int64_t, int64_t, string> MMSEngineDBFacade::getEncodingJobDetailsByIngestionJobKey(
	int64_t ingestionJobKey, bool fromMaster
)
{
    int64_t		encoderKey = -1;
    int64_t		encodingJobKey = -1;
	string		parameters;

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
			string sqlStatement = fmt::format(
				"select encodingJobKey, encoderKey, parameters "
				"from MMS_EncodingJob where ingestionJobKey = {}", ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			if (empty(res))
            {
				string errorMessage = __FILEREF__ + "No EncodingJob found"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				;

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
            }

			encodingJobKey = res[0]["encodingJobKey"].as<int64_t>();
			if (!res[0]["encoderKey"].is_null())
				encoderKey = res[0]["encoderKey"].as<int64_t>();
			parameters = res[0]["parameters"].as<string>();
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
    
    return make_tuple(encodingJobKey, encoderKey, parameters);
}

Json::Value MMSEngineDBFacade::getEncodingJobsStatus (
	shared_ptr<Workspace> workspace, int64_t encodingJobKey,
	int start, int rows,
	// bool startAndEndIngestionDatePresent,
	string startIngestionDate, string endIngestionDate,
	// bool startAndEndEncodingDatePresent,
	string startEncodingDate, string endEncodingDate,
	int64_t encoderKey,

	// 2021-01-29: next parameter is used ONLY if encoderKey != -1
	// The goal is the, if the user from a GUI asks for the encoding jobs of a specific encoder,
	// wants to know how the encoder is loaded and, to know that, he need to know also the encoding jobs
	// running on that encoder from other workflows.
	// So, if alsoEncodingJobsFromOtherWorkspaces is true and encoderKey != -1, we will send all the encodingJobs
	// running on that encoder.
	// In this case, for the one not belonging to the current workspace, we will not fill
	// the ingestionJobKey, so it is not possible to retrieve information by GUI like 'title media, ...'
	bool alsoEncodingJobsFromOtherWorkspaces,

	bool asc, string status, string types,
	bool fromMaster
)
{
    Json::Value statusListRoot;

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
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (encodingJobKey != -1)
            {
                field = "encodingJobKey";
                requestParametersRoot[field] = encodingJobKey;
            }
            
			/*
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
			*/
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


			/*
            if (startAndEndEncodingDatePresent)
            {
                field = "startEncodingDate";
                requestParametersRoot[field] = startEncodingDate;

                field = "endEncodingDate";
                requestParametersRoot[field] = endEncodingDate;
            }
			*/
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
			char delim = ',';	// types comma separator
			while (getline(ss, type, delim))
			{
				if (!type.empty())
				{
					vTypes.push_back(type);
					if (typesArgument == "")
						typesArgument = ("'" + type + "'");
					else
						typesArgument += (", '" + type + "'");
				}
			}
		}

        string sqlWhere = string ("where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey ");
		if (alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
			;
		else
			sqlWhere += fmt::format("and ir.workspaceKey = {} ", workspace->_workspaceKey);
        if (encodingJobKey != -1)
            sqlWhere += fmt::format("and ej.encodingJobKey = {} ", encodingJobKey);
        // if (startAndEndIngestionDatePresent)
        //     sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (startIngestionDate != "")
            sqlWhere += fmt::format("and ir.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(startIngestionDate));
        if (endIngestionDate != "")
            sqlWhere += fmt::format("and ir.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(endIngestionDate));
        // if (startAndEndEncodingDatePresent)
        //     sqlWhere += ("and ej.encodingJobStart >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ej.encodingJobStart <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (startEncodingDate != "")
            sqlWhere += fmt::format("and ej.encodingJobStart >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(startEncodingDate));
        if (endEncodingDate != "")
            sqlWhere += fmt::format("and ej.encodingJobStart <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(endEncodingDate));
        if (encoderKey != -1)
            sqlWhere += fmt::format("and ej.encoderKey = {} ", encoderKey);
        if (status == "All")
            ;
        else if (status == "Completed")
            sqlWhere += ("and ej.status like 'End_%' ");
        else if (status == "Processing")
            sqlWhere += ("and ej.status = 'Processing' ");
        else if (status == "ToBeProcessed")
            sqlWhere += ("and ej.status = 'ToBeProcessed' ");
        if (types != "")
		{
			if (vTypes.size() == 1)
				sqlWhere += fmt::format("and ej.type = {} ", types);
			else
				sqlWhere += ("and ej.type in (" + typesArgument + ")");
		}
        
        Json::Value responseRoot;
        {
            string sqlStatement = fmt::format( 
                "select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            field = "numFound";
            responseRoot[field] = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        Json::Value encodingJobsRoot(Json::arrayValue);
        {            
            string sqlStatement = fmt::format( 
                "select ir.workspaceKey, ej.encodingJobKey, ij.ingestionJobKey, ej.type, ej.parameters, "
				"ej.status, ej.encodingProgress, ej.processorMMS, ej.encoderKey, ej.encodingPid, "
				"ej.failuresNumber, ej.encodingPriority, "
                "to_char(ej.encodingJobStart, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobStart, "
                "to_char(ej.encodingJobEnd, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as encodingJobEnd, "
				"case when ij.startProcessing IS NULL then NOW() at time zone 'utc' else ij.startProcessing end as newStartProcessing, "
				"case when ij.endProcessing IS NULL then NOW() at time zone 'utc' else ij.endProcessing end as newEndProcessing "
                "from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej {} "
                "order by newStartProcessing {}, newEndProcessing {} "
                "limit {} offset {}",
				sqlWhere, asc ? "asc" : "desc", asc ? "asc " : "desc", rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                Json::Value encodingJobRoot;

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
					encodingJobRoot[field] = Json::nullValue;
				}
				*/

                field = "type";
                encodingJobRoot[field] = row["type"].as<string>();

				// if (ownedByCurrentWorkspace)
                {
                    string parameters = row["parameters"].as<string>();

                    Json::Value parametersRoot;
                    if (parameters != "")
						parametersRoot = JSONUtils::toJson(-1, encodingJobKey, parameters);

                    field = "parameters";
                    encodingJobRoot[field] = parametersRoot;
                }
				/*
				else
				{
                    field = "parameters";
					encodingJobRoot[field] = Json::nullValue;
				}
				*/

                field = "status";
                encodingJobRoot[field] = row["status"].as<string>();
                EncodingStatus encodingStatus = MMSEngineDBFacade::toEncodingStatus(row["status"].as<string>());

                field = "progress";
                if (row["encodingProgress"].is_null())
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = row["encodingProgress"].as<int>();

                field = "start";
                if (encodingStatus == EncodingStatus::ToBeProcessed)
                    encodingJobRoot[field] = Json::nullValue;
                else
                {
                    if (row["encodingJobStart"].is_null())
                        encodingJobRoot[field] = Json::nullValue;
                    else
                        encodingJobRoot[field] = row["encodingJobStart"].as<string>();
                }

                field = "end";
                if (row["encodingJobEnd"].is_null())
                    encodingJobRoot[field] = Json::nullValue;
                else
                    encodingJobRoot[field] = row["encodingJobEnd"].as<string>();

                field = "processorMMS";
                if (row["processorMMS"].is_null())
                    encodingJobRoot[field] = Json::nullValue;
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

                field = "failuresNumber";
                encodingJobRoot[field] = row["failuresNumber"].as<int>();  

                field = "encodingPriority";
                encodingJobRoot[field] = toString(static_cast<EncodingPriority>(row["encodingPriority"].as<int>()));

                field = "encodingPriorityCode";
                encodingJobRoot[field] = row["encodingPriority"].as<int>();

                field = "maxEncodingPriorityCode";
                encodingJobRoot[field] = workspace->_maxEncodingPriority;

                encodingJobsRoot.append(encodingJobRoot);
            }
        }
        
        field = "encodingJobs";
        responseRoot[field] = encodingJobsRoot;
        
        field = "response";
        statusListRoot[field] = responseRoot;

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
					"and ij.status like 'End_%' and ej.status not like 'End_%'"
				;
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.exec(sqlStatement);
				for (auto row: res)
				{
					int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
					int64_t encodingJobKey = row["encodingJobKey"].as<int64_t>();
					string ingestionJobStatus = row["ingestionJobStatus"].as<string>();
					string encodingJobStatus = row["encodingJobStatus"].as<string>();

					{
						string errorMessage = string("Found EncodingJob with wrong status")
							+ ", ingestionJobKey: " + to_string(ingestionJobKey)
							+ ", encodingJobKey: " + to_string(encodingJobKey)
							+ ", ingestionJobStatus: " + ingestionJobStatus
							+ ", encodingJobStatus: " + encodingJobStatus
						;
						_logger->error(__FILEREF__ + errorMessage);

						updateEncodingJob (
							encodingJobKey,
							EncodingError::CanceledByMMS,
							false,  // isIngestionJobFinished: this field is not used by updateEncodingJob
							ingestionJobKey,
							errorMessage
						);

						totalRowsUpdated++;
					}
				}

				toBeExecutedAgain = false;
			}
			catch(sql_error const &e)
			{
				currentRetriesOnError++;
				if (currentRetriesOnError >= maxRetriesOnError)
					throw e;

				// Deadlock!!!
				SPDLOG_ERROR("SQL exception"
					", query: {}"
					", exceptionMessage: {}"
					", conn: {}",
					e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
				);

				{
					int secondsBetweenRetries = 15;
					_logger->info(__FILEREF__ + "fixEncodingJobsHavingWrongStatus failed, "
						+ "waiting before to try again"
						+ ", currentRetriesOnError: " + to_string(currentRetriesOnError)
						+ ", maxRetriesOnError: " + to_string(maxRetriesOnError)
						+ ", secondsBetweenRetries: " + to_string(secondsBetweenRetries)
					);
					this_thread::sleep_for(chrono::seconds(secondsBetweenRetries));
				}
			}
		}

		_logger->info(__FILEREF__ + "fixEncodingJobsHavingWrongStatus "
			+ ", totalRowsUpdated: " + to_string(totalRowsUpdated)
		);

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncodingJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	MMSEngineDBFacade::ContentType contentType,
	EncodingPriority encodingPriority,
	int64_t encodingProfileKey,
	Json::Value encodingProfileDetailsRoot,

	Json::Value sourcesToBeEncodedRoot,

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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: "
						+ to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: "
						+ to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, "
				"processorMMS, encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},              {},   {},           {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL,             {}, "
				"NULL,         NULL,       NULL,        0,				NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {     
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;
            
            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_OverlayImageOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,
    int64_t sourceVideoMediaItemKey, int64_t sourceVideoPhysicalPathKey, int64_t videoDurationInMilliSeconds,
	string mmsSourceVideoAssetPathName, string sourceVideoPhysicalDeliveryURL,
	string sourceVideoFileExtension,
	int64_t sourceImageMediaItemKey, int64_t sourceImagePhysicalPathKey,
	string mmsSourceImageAssetPathName, string sourceImagePhysicalDeliveryURL,
	string sourceVideoTranscoderStagingAssetPathName,                                                 
	string encodedTranscoderStagingAssetPathName,                                                     
	string encodedNFSStagingAssetPathName,
    EncodingPriority encodingPriority,
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},  {},		   {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_OverlayTextOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,

	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,

	string sourceAssetPathName,
	int64_t sourceDurationInMilliSeconds,
	string sourcePhysicalDeliveryURL,
	string sourceFileExtension,

	string sourceTranscoderStagingAssetPathName, string encodedTranscoderStagingAssetPathName,
	string encodedNFSStagingAssetPathName,
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_GenerateFramesJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,

    string nfsImagesDirectory,
	string transcoderStagingImagesDirectory,
	string sourcePhysicalDeliveryURL,
	string sourceTranscoderStagingAssetPathName,
	string sourceAssetPathName,
    int64_t sourceVideoPhysicalPathKey,
	string sourceFileExtension,
	string sourceFileName,
    int64_t videoDurationInMilliSeconds,
    double startTimeInSeconds, int maxFramesNumber, 
    string videoFilter, int periodInSeconds, 
    bool mjpeg, int imageWidth, int imageHeight,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL,
	string mmsIngestionURL
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_SlideShowJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot, string targetFileFormat,
	Json::Value imagesRoot, Json::Value audiosRoot, float shortestAudioDurationInSeconds,
	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, "
				"parameters, encodingPriority, encodingJobStart, encodingJobEnd, "
				"encodingProgress, status, processorMMS, encoderKey, "
				"encodingPid, failuresNumber, creationDate) values ("
				"                            NULL,           {},               {},    {}, "
				"{},          {},              NOW() at time zone 'utc', NULL, "
				"NULL,             {},      NULL,         NULL, "
				"NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_FaceRecognitionJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey,
	int64_t sourceVideoPhysicalPathKey,
	string sourcePhysicalPath,
	string faceRecognitionCascadeName,
	string faceRecognitionOutput,
	EncodingPriority encodingPriority,
	long initialFramesNumberToBeSkipped,
	bool oneFramePerSecond
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
        
        string parameters = string()
                + "{ "
                + "\"sourceMediaItemKey\": " + to_string(sourceMediaItemKey)
                + ", \"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)
                + ", \"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\""
                + ", \"faceRecognitionCascadeName\": \"" + faceRecognitionCascadeName + "\""
                + ", \"faceRecognitionOutput\": \"" + faceRecognitionOutput + "\""
                + ", \"initialFramesNumberToBeSkipped\": " + to_string(initialFramesNumberToBeSkipped)
                + ", \"oneFramePerSecond\": " + to_string(oneFramePerSecond)
                + "} "
                ;

        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_FaceIdentificationJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string sourcePhysicalPath,
	string faceIdentificationCascadeName,
	string deepLearnedModelTagsCommaSeparated,
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
        EncodingType encodingType = EncodingType::FaceIdentification;
        
        string parameters = string()
                + "{ "
                + "\"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\""
                + ", \"faceIdentificationCascadeName\": \"" + faceIdentificationCascadeName + "\""
                + ", \"deepLearnedModelTagsCommaSeparated\": " + deepLearnedModelTagsCommaSeparated + ""
                + "} "
                ;

        {
            int savedEncodingPriority = static_cast<int>(encodingPriority);
            if (savedEncodingPriority > workspace->_maxEncodingPriority)
            {
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_LiveRecorderJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey, string ingestionJobLabel,
	string streamSourceType,

	string configurationLabel, int64_t confKey, string liveURL, string encodersPoolLabel,
	EncodingPriority encodingPriority,

	int pushListenTimeout, int64_t pushEncoderKey, string pushServerName,
	Json::Value captureRoot,
	Json::Value tvRoot,

	bool monitorHLS,
	bool liveRecorderVirtualVOD,
	int monitorVirtualVODOutputRootIndex,

	Json::Value outputsRoot, Json::Value framesToBeDetectedRoot,

	string chunksTranscoderStagingContentsPath, string chunksNFSStagingContentsPath,
	string segmentListFileName, string recordedFileNamePrefix,
	string virtualVODStagingContentsPath, string virtualVODTranscoderStagingContentsPath,
	int64_t liveRecorderVirtualVODImageMediaItemKey,

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
        _logger->info(__FILEREF__ + "addEncoding_LiveRecorderJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ingestionJobLabel: " + ingestionJobLabel
            + ", streamSourceType: " + streamSourceType
            + ", configurationLabel: " + configurationLabel
            + ", confKey: " + to_string(confKey)
            + ", liveURL: " + liveURL
            + ", encodingPriority: " + toString(encodingPriority)
            + ", monitorHLS: " + to_string(monitorHLS)
            + ", liveRecorderVirtualVOD: " + to_string(liveRecorderVirtualVOD)
            + ", outputsRoot.size: " + (outputsRoot != Json::nullValue ? to_string(outputsRoot.size()) : to_string(0))
        );

		{
			EncodingType encodingType = EncodingType::LiveRecorder;
        
			string parameters;
			{
				Json::Value parametersRoot;

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

				field = "pushServerName";
				parametersRoot[field] = pushServerName;

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
					"encoderKey, encodingPid, failuresNumber, creationDate) values ("
												"NULL,           {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
					trans.quote(parameters), savedEncodingPriority,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		int64_t backupEncodingJobKey = -1;

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_LiveProxyJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	Json::Value inputsRoot,
	string streamSourceType,
	int64_t utcProxyPeriodStart,
	// long maxAttemptsNumberInCaseOfErrors,
	long waitingSecondsBetweenAttemptsInCaseOfErrors,
	Json::Value outputsRoot
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
        _logger->info(__FILEREF__ + "addEncoding_LiveProxyJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", streamSourceType: " + streamSourceType
            + ", waitingSecondsBetweenAttemptsInCaseOfErrors: " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors)
            + ", outputsRoot.size: " + to_string(outputsRoot.size())
        );

		{
			EncodingType encodingType = EncodingType::LiveProxy;
        
			string parameters;
			{
				Json::Value parametersRoot;

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

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format( 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber, creationDate) values ("
												"NULL,           {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
					trans.quote(parameters), savedEncodingPriority,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			// int64_t encodingJobKey = getLastInsertId(conn);
        
			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_VODProxyJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	Json::Value inputsRoot,
	int64_t utcProxyPeriodStart,
	Json::Value outputsRoot,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors
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
        _logger->info(__FILEREF__ + "addEncoding_VODProxyJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", outputsRoot.size: " + to_string(outputsRoot.size())
        );

		{
			EncodingType encodingType = EncodingType::VODProxy;
        
			string parameters;
			{
				Json::Value parametersRoot;

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

				parameters = JSONUtils::toString(parametersRoot);
			}

			// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
			//	will be managed as soon as possible
			int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

			{
				string sqlStatement = fmt::format( 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber, creationDate) values ("
												"NULL,           {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
					trans.quote(parameters), savedEncodingPriority,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			// int64_t encodingJobKey = getLastInsertId(conn);
        
			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_CountdownJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	Json::Value inputsRoot,
	int64_t utcProxyPeriodStart,
	Json::Value outputsRoot,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors)
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
				Json::Value parametersRoot;

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

				parameters = JSONUtils::toString(parametersRoot);
			}

			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				string sqlStatement = fmt::format( 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber, creationDate) values ("
												"NULL,           {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
					trans.quote(parameters), savedEncodingPriority,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			// int64_t encodingJobKey = getLastInsertId(conn);
        
			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_LiveGridJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	Json::Value inputChannelsRoot,
	Json::Value outputsRoot
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
        _logger->info(__FILEREF__ + "addEncoding_LiveGridJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );

		{
			EncodingType encodingType = EncodingType::LiveGrid;

			string parameters;
			{
				Json::Value parametersRoot;

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
					"encoderKey, encodingPid, failuresNumber, creationDate) values ("
												"NULL,           {},               {},    {},			  {},          {}, "
					"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
					"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
					ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
					trans.quote(parameters), savedEncodingPriority,
					trans.quote(toString(EncodingStatus::ToBeProcessed)));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans.exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			// int64_t encodingJobKey = getLastInsertId(conn);
        
			{
				IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

				string errorMessage;
				string processorMMS;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", IngestionStatus: " + toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);                            
				updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_VideoSpeed (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,                                                        
	string sourceAssetPathName, int64_t sourceDurationInMilliSeconds, string sourceFileExtension,                                                         
	string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName,                                  
	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,
	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
	EncodingPriority encodingPriority)
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_AddSilentAudio (
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	Json::Value sourcesRoot,
	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
	EncodingPriority encodingPriority)
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
				"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_PictureInPictureJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t mainSourceMediaItemKey, int64_t mainSourcePhysicalPathKey, string mainSourceAssetPathName,                       
	int64_t mainSourceDurationInMilliSeconds, string mainSourceFileExtension,                                                                          
	string mainSourcePhysicalDeliveryURL, string mainSourceTranscoderStagingAssetPathName,                          
	int64_t overlaySourceMediaItemKey, int64_t overlaySourcePhysicalPathKey, string overlaySourceAssetPathName,              
	int64_t overlaySourceDurationInMilliSeconds, string overlaySourceFileExtension,
	string overlaySourcePhysicalDeliveryURL, string overlaySourceTranscoderStagingAssetPathName,                    
	bool soundOfMain,
	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,
	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,
	EncodingPriority encodingPriority)
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {},          {}, "
				"NOW() at time zone 'utc', NULL,   NULL,             {},      NULL, "
				"NULL,       NULL,        0,			  NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_IntroOutroOverlayJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,

	int64_t encodingProfileKey,
	Json::Value encodingProfileDetailsRoot,

	int64_t introSourcePhysicalPathKey, string introSourceAssetPathName,
	string introSourceFileExtension, int64_t introSourceDurationInMilliSeconds,
	string introSourcePhysicalDeliveryURL, string introSourceTranscoderStagingAssetPathName,

	int64_t mainSourcePhysicalPathKey, string mainSourceAssetPathName,
	string mainSourceFileExtension, int64_t mainSourceDurationInMilliSeconds,
	string mainSourcePhysicalDeliveryURL, string mainSourceTranscoderStagingAssetPathName,

	int64_t outroSourcePhysicalPathKey, string outroSourceAssetPathName,
	string outroSourceFileExtension, int64_t outroSourceDurationInMilliSeconds,
	string outroSourcePhysicalDeliveryURL, string outroSourceTranscoderStagingAssetPathName,

	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,

	EncodingPriority encodingPriority)
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, "
				"status, processorMMS, encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL, "
				"{},      NULL,         NULL,       NULL,        0,			    NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

void MMSEngineDBFacade::addEncoding_CutFrameAccurate (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,

	int64_t sourceMediaItemKey, int64_t sourcePhysicalPathKey,
	string sourceAssetPathName, int64_t sourceDurationInMilliSeconds, string sourceFileExtension,
	string sourcePhysicalDeliveryURL, string sourceTranscoderStagingAssetPathName,
	string endTime,

	int64_t encodingProfileKey, Json::Value encodingProfileDetailsRoot,

	string encodedTranscoderStagingAssetPathName, string encodedNFSStagingAssetPathName,
	string mmsWorkflowIngestionURL, string mmsBinaryIngestionURL, string mmsIngestionURL,

	EncodingPriority encodingPriority,
	int64_t newUtcStartTimeInMilliSecs, int64_t newUtcEndTimeInMilliSecs)
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
			Json::Value parametersRoot;

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
                _logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
                    + ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
                    + ", requested encoding profile key: " + to_string(static_cast<int>(encodingPriority))
                );

                savedEncodingPriority = workspace->_maxEncodingPriority;
            }

            string sqlStatement = fmt::format( 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, typePriority, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, "
				"status, processorMMS, encoderKey, encodingPid, failuresNumber, creationDate) values ("
                                            "NULL,           {},               {},    {},			  {}, "
				"{},               NOW() at time zone 'utc', NULL,   NULL, "
				"{},      NULL,         NULL,       NULL,        0,				NOW() at time zone 'utc')",
				ingestionJobKey, trans.quote(toString(encodingType)), getEncodingTypePriority(encodingType),
				trans.quote(parameters), savedEncodingPriority,
				trans.quote(toString(EncodingStatus::ToBeProcessed)));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
        {            
            IngestionStatus newIngestionStatus = IngestionStatus::EncodingQueued;

            string errorMessage;
            string processorMMS;
            _logger->info(__FILEREF__ + "Update IngestionJob"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", IngestionStatus: " + toString(newIngestionStatus)
                + ", errorMessage: " + errorMessage
                + ", processorMMS: " + processorMMS
            );                            
            updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		try
		{
			trans.abort();
		}
		catch (exception& e)
		{
			SPDLOG_ERROR("abort failed"
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

int MMSEngineDBFacade::getEncodingTypePriority(
	MMSEngineDBFacade::EncodingType encodingType)
{
	// The priority is used when engine retrieves the encoding jobs to be executed

	if (encodingType == EncodingType::LiveProxy
		|| encodingType == EncodingType::VODProxy
		|| encodingType == EncodingType::Countdown)
		return 1;
	else if (encodingType == EncodingType::LiveRecorder)
		return 5;
	else
		return 10;
}

