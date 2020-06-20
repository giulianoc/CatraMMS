
#include "JSONUtils.h"
#include "PersistenceLock.h"
#include "MMSEngineDBFacade.h"

void MMSEngineDBFacade::getEncodingJobs(
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems,
		int maxEncodingsNumber
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
		int milliSecondsToSleepWaitingLock = 500;

        PersistenceLock persistenceLock(this,
            MMSEngineDBFacade::LockType::Encoding,
            _maxSecondsToWaitCheckEncodingJobLock,
            processorMMS, "CheckEncoding",
            milliSecondsToSleepWaitingLock, _logger);

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

		// first Live-Proxy because if we have many Live-Recording, Live-Proxy will never start
        {
			_logger->info(__FILEREF__ + "getEncodingJobs for LiveProxy");

            lastSQLCommand =
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.transcoder, ej.stagingEncodedAssetPathName "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and ij.ingestionType = 'Live-Proxy' "
				;
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();

                encodingItem->_encodingJobKey = encodingResultSet->getInt64("encodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
						encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("transcoder"))
					encodingItem->_transcoder = "";
				else
					encodingItem->_transcoder = encodingResultSet->getString("transcoder");
				if (encodingResultSet->isNull("stagingEncodedAssetPathName"))
					encodingItem->_stagingEncodedAssetPathName = "";
				else
					encodingItem->_stagingEncodedAssetPathName =
						encodingResultSet->getString("stagingEncodedAssetPathName");

                if (encodingItem->_encodingParameters == "")
                {
                    string errorMessage = __FILEREF__ + "encodingItem->_encodingParameters is empty"
                            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                            ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
						_logger->info(__FILEREF__ + "EncodingJob update"
                            + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
						);
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++,
							MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                {
                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    bool parsingSuccessful = reader->parse((encodingItem->_encodingParameters).c_str(),
                            (encodingItem->_encodingParameters).c_str()
							+ (encodingItem->_encodingParameters).size(), 
                            &(encodingItem->_encodingParametersRoot), &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", errors: " + errors
                                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                                ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                int64_t workspaceKey;
                {
                    lastSQLCommand = 
                        "select ir.workspaceKey "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey "
                        "and ij.ingestionJobKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementWorkspace (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementWorkspace->setInt64(queryParameterIndex++,
							encodingItem->_ingestionJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> workspaceResultSet (
							preparedStatementWorkspace->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (workspaceResultSet->next())
                    {
                        encodingItem->_workspace = getWorkspace(workspaceResultSet->getInt64("workspaceKey"));
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                // if (encodingItem->_encodingType == EncodingType::LiveProxy)
                {
                    encodingItem->_liveProxyData = make_shared<EncodingItem::LiveProxyData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string liveProxyParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(liveProxyParameters.c_str(),
                                        liveProxyParameters.c_str() + liveProxyParameters.size(), 
                                        &(encodingItem->_liveProxyData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", liveProxyParameters: " + liveProxyParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }

                encodingItems.push_back(encodingItem);

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ?, encodingJobStart = NULL "
						"where encodingJobKey = ? and processorMMS is null";
                    shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++,
							MMSEngineDBFacade::toString(EncodingStatus::Processing));
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                    preparedStatementUpdateEncoding->setInt64(queryParameterIndex++,
							encodingItem->_encodingJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
            }
        }

		// 2019-12-14: we have a long list of encodings to be done (113 encodings) and
		//	among these we have some live recordings. These has to be managed before the others encodings
        {
			_logger->info(__FILEREF__ + "getEncodingJobs for LiveRecorder");

            lastSQLCommand =
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.transcoder, ej.stagingEncodedAssetPathName "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and ij.ingestionType = 'Live-Recorder' "
				"order by JSON_EXTRACT(ij.metaDataContent, '$.RecordingPeriod.Start') asc"
				;
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();

                encodingItem->_encodingJobKey = encodingResultSet->getInt64("encodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
						encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("transcoder"))
					encodingItem->_transcoder = "";
				else
					encodingItem->_transcoder = encodingResultSet->getString("transcoder");
				if (encodingResultSet->isNull("stagingEncodedAssetPathName"))
					encodingItem->_stagingEncodedAssetPathName = "";
				else
					encodingItem->_stagingEncodedAssetPathName =
						encodingResultSet->getString("stagingEncodedAssetPathName");

                if (encodingItem->_encodingParameters == "")
                {
                    string errorMessage = __FILEREF__ + "encodingItem->_encodingParameters is empty"
                            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                            ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
						_logger->info(__FILEREF__ + "EncodingJob update"
                            + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
						);
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++,
							MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                {
                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    bool parsingSuccessful = reader->parse((encodingItem->_encodingParameters).c_str(),
                            (encodingItem->_encodingParameters).c_str()
							+ (encodingItem->_encodingParameters).size(), 
                            &(encodingItem->_encodingParametersRoot), &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", errors: " + errors
                                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                                ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                int64_t workspaceKey;
                {
                    lastSQLCommand = 
                        "select ir.workspaceKey "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey "
                        "and ij.ingestionJobKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementWorkspace (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementWorkspace->setInt64(queryParameterIndex++,
							encodingItem->_ingestionJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> workspaceResultSet (
							preparedStatementWorkspace->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (workspaceResultSet->next())
                    {
                        encodingItem->_workspace = getWorkspace(workspaceResultSet->getInt64("workspaceKey"));
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                // if (encodingItem->_encodingType == EncodingType::LiveRecorder)
                {
                    encodingItem->_liveRecorderData = make_shared<EncodingItem::LiveRecorderData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string liveRecorderParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(liveRecorderParameters.c_str(),
                                        liveRecorderParameters.c_str() + liveRecorderParameters.size(), 
                                        &(encodingItem->_liveRecorderData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", liveRecorderParameters: " + liveRecorderParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }

                encodingItems.push_back(encodingItem);

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ?, encodingJobStart = NULL "
						"where encodingJobKey = ? and processorMMS is null";
                    shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++,
							MMSEngineDBFacade::toString(EncodingStatus::Processing));
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                    preparedStatementUpdateEncoding->setInt64(queryParameterIndex++,
							encodingItem->_encodingJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
            }
        }

		bool stillRows = true;
		int startRow = 0;
		while(encodingItems.size() < maxEncodingsNumber && stillRows)
        {
			_logger->info(__FILEREF__ + "getEncodingJobs"
				+ ", startRow: " + to_string(startRow)
			);
            lastSQLCommand = 
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.transcoder, ej.stagingEncodedAssetPathName "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and (ij.ingestionType != 'Live-Recorder' and ij.ingestionType != 'Live-Proxy') "
				"order by ej.encodingPriority desc, ir.ingestionDate asc, ej.failuresNumber asc "
				"limit ? offset ?"

				/*
                "select encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"transcoder, stagingEncodedAssetPathName from MMS_EncodingJob " 
                "where processorMMS is null and status = ? and encodingJobStart <= NOW() "
                "order by encodingPriority desc, encodingJobStart asc, failuresNumber asc "
				"limit ? offset ?"
				*/
				// "limit ? for update"
				;
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++,
				MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatementEncoding->setInt(queryParameterIndex++, maxEncodingsNumber);
            preparedStatementEncoding->setInt(queryParameterIndex++, startRow);

			startRow += maxEncodingsNumber;

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", maxEncodingsNumber: " + to_string(maxEncodingsNumber)
				+ ", startRow: " + to_string(startRow)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (encodingResultSet->rowsCount() != maxEncodingsNumber)
				stillRows = false;

            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();
                
                encodingItem->_encodingJobKey = encodingResultSet->getInt64("encodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
						encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("transcoder"))
					encodingItem->_transcoder = "";
				else
					encodingItem->_transcoder = encodingResultSet->getString("transcoder");
				if (encodingResultSet->isNull("stagingEncodedAssetPathName"))
					encodingItem->_stagingEncodedAssetPathName = "";
				else
					encodingItem->_stagingEncodedAssetPathName =
						encodingResultSet->getString("stagingEncodedAssetPathName");

                if (encodingItem->_encodingParameters == "")
                {
                    string errorMessage = __FILEREF__ + "encodingItem->_encodingParameters is empty"
                            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                            ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
						_logger->info(__FILEREF__ + "EncodingJob update"
                            + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                            + ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
						);
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++,
							MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                {
                    Json::CharReaderBuilder builder;
                    Json::CharReader* reader = builder.newCharReader();
                    string errors;

                    bool parsingSuccessful = reader->parse((encodingItem->_encodingParameters).c_str(),
                            (encodingItem->_encodingParameters).c_str()
							+ (encodingItem->_encodingParameters).size(), 
                            &(encodingItem->_encodingParametersRoot), &errors);
                    delete reader;

                    if (!parsingSuccessful)
                    {
                        string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", errors: " + errors
                                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
                                ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                int64_t workspaceKey;
                {
                    lastSQLCommand = 
                        "select ir.workspaceKey "
                        "from MMS_IngestionRoot ir, MMS_IngestionJob ij "
                        "where ir.ingestionRootKey = ij.ingestionRootKey "
                        "and ij.ingestionJobKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementWorkspace (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementWorkspace->setInt64(queryParameterIndex++,
							encodingItem->_ingestionJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    shared_ptr<sql::ResultSet> workspaceResultSet (
							preparedStatementWorkspace->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (workspaceResultSet->next())
                    {
                        encodingItem->_workspace = getWorkspace(workspaceResultSet->getInt64("workspaceKey"));
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        // in case an encoding job row generate an error, we have to make it to Failed
                        // otherwise we will indefinitely get this error
                        {
							_logger->info(__FILEREF__ + "EncodingJob update"
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                            lastSQLCommand = 
                                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                            shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
                            int queryParameterIndex = 1;
                            preparedStatementUpdate->setString(queryParameterIndex++,
									MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                            preparedStatementUpdate->setInt64(queryParameterIndex++,
									encodingItem->_encodingJobKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
                            int rowsUpdated = preparedStatementUpdate->executeUpdate();
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
								+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
                        }

                        continue;
                        // throw runtime_error(errorMessage);
                    }
                }
                
                if (encodingItem->_encodingType == EncodingType::EncodeVideoAudio
                        || encodingItem->_encodingType == EncodingType::EncodeImage)
                {
                    encodingItem->_encodeData = make_shared<EncodingItem::EncodeData>();
                            
                    string field = "sourcePhysicalPathKey";
                    int64_t sourcePhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                                        
                    field = "encodingProfileKey";
                    int64_t encodingProfileKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);

                    {
                        lastSQLCommand = 
                            "select m.contentType, p.partitionNumber, p.mediaItemKey, p.fileName, "
							"p.relativePath, p.durationInMilliSeconds "
                            "from MMS_MediaItem m, MMS_PhysicalPath p where m.mediaItemKey = p.mediaItemKey "
							"and p.physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourcePhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (
								preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            encodingItem->_encodeData->_contentType = MMSEngineDBFacade::toContentType(
								physicalPathResultSet->getString("contentType"));
                            // encodingItem->_encodeData->_mmsPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_encodeData->_mediaItemKey =
								physicalPathResultSet->getInt64("mediaItemKey");
                            encodingItem->_encodeData->_fileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_encodeData->_relativePath =
								physicalPathResultSet->getString("relativePath");
							if (physicalPathResultSet->isNull("durationInMilliSeconds"))
								encodingItem->_encodeData->_durationInMilliSeconds = -1;
							else
								encodingItem->_encodeData->_durationInMilliSeconds =
									physicalPathResultSet->getInt64("durationInMilliSeconds");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select deliveryTechnology, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementEncodingProfile->setInt64(queryParameterIndex++, encodingProfileKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> encodingProfilesResultSet (
								preparedStatementEncodingProfile->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (encodingProfilesResultSet->next())
                        {
                            encodingItem->_encodeData->_deliveryTechnology =
								toDeliveryTechnology(encodingProfilesResultSet->getString("deliveryTechnology"));
                            encodingItem->_encodeData->_jsonProfile =
								encodingProfilesResultSet->getString("jsonProfile");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed"
                                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++,
								encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (
								preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string ingestionParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(ingestionParameters.c_str(),
                                        ingestionParameters.c_str() + ingestionParameters.size(), 
                                        &(encodingItem->_encodeData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", ingestionParameters: " + ingestionParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++,
												MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++,
												encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::OverlayImageOnVideo)
                {
                    encodingItem->_overlayImageOnVideoData =
						make_shared<EncodingItem::OverlayImageOnVideoData>();
                    
                    int64_t sourceVideoPhysicalPathKey;
                    int64_t sourceImagePhysicalPathKey;    

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);

                        field = "sourceImagePhysicalPathKey";
                        sourceImagePhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (
								preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            // encodingItem->_overlayImageOnVideoData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayImageOnVideoData->_videoFileName =
								physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayImageOnVideoData->_videoRelativePath =
								physicalPathResultSet->getString("relativePath");
							if (physicalPathResultSet->isNull("durationInMilliSeconds"))
								encodingItem->_overlayImageOnVideoData->_videoDurationInMilliSeconds = -1;
							else
								encodingItem->_overlayImageOnVideoData->_videoDurationInMilliSeconds =
									physicalPathResultSet->getInt64("durationInMilliSeconds");
                            
                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select partitionNumber, fileName, relativePath "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++,
								sourceImagePhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (
								preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourceImagePhysicalPathKey: " + to_string(sourceImagePhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            // encodingItem->_overlayImageOnVideoData->_mmsImagePartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayImageOnVideoData->_imageFileName =
								physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayImageOnVideoData->_imageRelativePath =
								physicalPathResultSet->getString("relativePath");                            
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceImagePhysicalPathKey: " + to_string(sourceImagePhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++,
								encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string overlayParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(overlayParameters.c_str(),
                                        overlayParameters.c_str() + overlayParameters.size(), 
                                        &(encodingItem->_overlayImageOnVideoData->_ingestedParametersRoot),
										&errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", overlayParameters: " + overlayParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++,
												MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++,
												encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::OverlayTextOnVideo)
                {
                    encodingItem->_overlayTextOnVideoData = make_shared<EncodingItem::OverlayTextOnVideoData>();
                    
                    int64_t sourceVideoPhysicalPathKey;

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            // encodingItem->_overlayTextOnVideoData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_overlayTextOnVideoData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_overlayTextOnVideoData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
							if (physicalPathResultSet->isNull("durationInMilliSeconds"))
								encodingItem->_overlayTextOnVideoData->_videoDurationInMilliSeconds = -1;
							else
								encodingItem->_overlayTextOnVideoData->_videoDurationInMilliSeconds =
									physicalPathResultSet->getInt64("durationInMilliSeconds");
                            
                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string overlayTextParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(overlayTextParameters.c_str(),
                                        overlayTextParameters.c_str() + overlayTextParameters.size(), 
                                        &(encodingItem->_overlayTextOnVideoData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", overlayTextParameters: " + overlayTextParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::GenerateFrames)
                {
                    encodingItem->_generateFramesData = make_shared<EncodingItem::GenerateFramesData>();
                    
                    int64_t sourceVideoPhysicalPathKey;

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            // encodingItem->_generateFramesData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_generateFramesData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_generateFramesData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
							if (physicalPathResultSet->isNull("durationInMilliSeconds"))
								encodingItem->_generateFramesData->_videoDurationInMilliSeconds = -1;
							else
								encodingItem->_generateFramesData->_videoDurationInMilliSeconds =
									physicalPathResultSet->getInt64("durationInMilliSeconds");

                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string generateFramesParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(generateFramesParameters.c_str(),
                                        generateFramesParameters.c_str() + generateFramesParameters.size(), 
                                        &(encodingItem->_generateFramesData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", generateFramesParameters: " + generateFramesParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::SlideShow)
                {
                    encodingItem->_slideShowData = make_shared<EncodingItem::SlideShowData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string slideShowParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(slideShowParameters.c_str(),
                                        slideShowParameters.c_str() + slideShowParameters.size(), 
                                        &(encodingItem->_slideShowData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", slideShowParameters: " + slideShowParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::FaceRecognition)
                {
                    encodingItem->_faceRecognitionData = make_shared<EncodingItem::FaceRecognitionData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string faceRecognitionParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(faceRecognitionParameters.c_str(),
                                        faceRecognitionParameters.c_str() + faceRecognitionParameters.size(), 
                                        &(encodingItem->_faceRecognitionData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", faceRecognitionParameters: " + faceRecognitionParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++,
												MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++,
												encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::FaceIdentification)
                {
                    encodingItem->_faceIdentificationData = make_shared<EncodingItem::FaceIdentificationData>();
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string faceIdentificationParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(faceIdentificationParameters.c_str(),
                                        faceIdentificationParameters.c_str() + faceIdentificationParameters.size(), 
                                        &(encodingItem->_faceIdentificationData->_ingestedParametersRoot),
										&errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", faceIdentificationParameters: " + faceIdentificationParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++,
												MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++,
												encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
				/* Live-Recorder are managed in the previous loop
                else if (encodingItem->_encodingType == EncodingType::LiveRecorder)
                {
                }
				*/
                else if (encodingItem->_encodingType == EncodingType::VideoSpeed)
                {
                    encodingItem->_videoSpeedData = make_shared<EncodingItem::VideoSpeedData>();
                    
                    int64_t sourceVideoPhysicalPathKey;

                    {
                        string field = "sourceVideoPhysicalPathKey";
                        sourceVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                    }

                    int64_t videoMediaItemKey;
                    {
                        lastSQLCommand = 
                            "select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
                            "from MMS_PhysicalPath where physicalPathKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementPhysicalPath->setInt64(queryParameterIndex++, sourceVideoPhysicalPathKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (physicalPathResultSet->next())
                        {
                            // encodingItem->_videoSpeedData->_mmsVideoPartitionNumber = physicalPathResultSet->getInt("partitionNumber");
                            encodingItem->_videoSpeedData->_videoFileName = physicalPathResultSet->getString("fileName");
                            encodingItem->_videoSpeedData->_videoRelativePath = physicalPathResultSet->getString("relativePath");
							if (physicalPathResultSet->isNull("durationInMilliSeconds"))
								encodingItem->_videoSpeedData->_videoDurationInMilliSeconds = -1;
							else
								encodingItem->_videoSpeedData->_videoDurationInMilliSeconds =
									physicalPathResultSet->getInt64("durationInMilliSeconds");

                            videoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                    
                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string videoSpeedParameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(videoSpeedParameters.c_str(),
                                        videoSpeedParameters.c_str() + videoSpeedParameters.size(), 
                                        &(encodingItem->_videoSpeedData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", videoSpeedParameters: " + videoSpeedParameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
                else if (encodingItem->_encodingType == EncodingType::PictureInPicture)
                {
                    encodingItem->_pictureInPictureData = make_shared<EncodingItem::PictureInPictureData>();
                    
                    int64_t mainVideoPhysicalPathKey;
                    int64_t overlayVideoPhysicalPathKey;    

                    {
                        string field = "mainVideoPhysicalPathKey";
                        mainVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);

                        field = "overlayVideoPhysicalPathKey";
                        overlayVideoPhysicalPathKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, 0);
                    }

					{
						int64_t mainVideoMediaItemKey;
						{
							lastSQLCommand = 
								"select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
								"from MMS_PhysicalPath where physicalPathKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementPhysicalPath->setInt64(queryParameterIndex++,
									mainVideoPhysicalPathKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							shared_ptr<sql::ResultSet> physicalPathResultSet (
									preparedStatementPhysicalPath->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", mainVideoPhysicalPathKey: " + to_string(mainVideoPhysicalPathKey)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (physicalPathResultSet->next())
							{
								// encodingItem->_pictureInPictureData->_mmsMainVideoPartitionNumber
								// 	= physicalPathResultSet->getInt("partitionNumber");
								encodingItem->_pictureInPictureData->_mainVideoFileName
									= physicalPathResultSet->getString("fileName");
								encodingItem->_pictureInPictureData->_mainVideoRelativePath
									= physicalPathResultSet->getString("relativePath");
								if (physicalPathResultSet->isNull("durationInMilliSeconds"))
									encodingItem->_pictureInPictureData->_mainVideoDurationInMilliSeconds = -1;
								else
									encodingItem->_pictureInPictureData->_mainVideoDurationInMilliSeconds =
										physicalPathResultSet->getInt64("durationInMilliSeconds");
                            
								mainVideoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
							}
							else
							{
								string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", mainVideoPhysicalPathKey: " + to_string(mainVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
								;
								_logger->error(errorMessage);

								// in case an encoding job row generate an error, we have to make it to Failed
								// otherwise we will indefinitely get this error
								{
									_logger->info(__FILEREF__ + "EncodingJob update"
										+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
										+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
										);
									lastSQLCommand = 
										"update MMS_EncodingJob set status = ? where encodingJobKey = ?";
									shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
									int queryParameterIndex = 1;
									preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
									preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

									chrono::system_clock::time_point startSql = chrono::system_clock::now();
									int rowsUpdated = preparedStatementUpdate->executeUpdate();
									_logger->info(__FILEREF__ + "@SQL statistics@"
										+ ", lastSQLCommand: " + lastSQLCommand
										+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
										+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
										+ ", rowsUpdated: " + to_string(rowsUpdated)
										+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
											chrono::system_clock::now() - startSql).count()) + "@"
									);
								}

								continue;
								// throw runtime_error(errorMessage);
							}
						}
					}

					{
						int64_t overlayVideoMediaItemKey;
						{
							lastSQLCommand = 
								"select partitionNumber, mediaItemKey, fileName, relativePath, durationInMilliSeconds "
								"from MMS_PhysicalPath where physicalPathKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (
									conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementPhysicalPath->setInt64(queryParameterIndex++,
									overlayVideoPhysicalPathKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							shared_ptr<sql::ResultSet> physicalPathResultSet (
									preparedStatementPhysicalPath->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", overlayVideoPhysicalPathKey: " + to_string(overlayVideoPhysicalPathKey)
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (physicalPathResultSet->next())
							{
								// encodingItem->_pictureInPictureData->_mmsOverlayVideoPartitionNumber
								// 	= physicalPathResultSet->getInt("partitionNumber");
								encodingItem->_pictureInPictureData->_overlayVideoFileName
									= physicalPathResultSet->getString("fileName");
								encodingItem->_pictureInPictureData->_overlayVideoRelativePath
									= physicalPathResultSet->getString("relativePath");
								if (physicalPathResultSet->isNull("durationInMilliSeconds"))
									encodingItem->_pictureInPictureData->_overlayVideoDurationInMilliSeconds = -1;
								else
									encodingItem->_pictureInPictureData->_overlayVideoDurationInMilliSeconds =
										physicalPathResultSet->getInt64("durationInMilliSeconds");
                            
								overlayVideoMediaItemKey = physicalPathResultSet->getInt64("mediaItemKey");
							}
							else
							{
								string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", overlayVideoPhysicalPathKey: " + to_string(overlayVideoPhysicalPathKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
								;
								_logger->error(errorMessage);

								// in case an encoding job row generate an error, we have to make it to Failed
								// otherwise we will indefinitely get this error
								{
									_logger->info(__FILEREF__ + "EncodingJob update"
										+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
										+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
										);
									lastSQLCommand = 
										"update MMS_EncodingJob set status = ? where encodingJobKey = ?";
									shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
										conn->_sqlConnection->prepareStatement(lastSQLCommand));
									int queryParameterIndex = 1;
									preparedStatementUpdate->setString(queryParameterIndex++,
										MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
									preparedStatementUpdate->setInt64(queryParameterIndex++,
										encodingItem->_encodingJobKey);

									chrono::system_clock::time_point startSql = chrono::system_clock::now();
									int rowsUpdated = preparedStatementUpdate->executeUpdate();
									_logger->info(__FILEREF__ + "@SQL statistics@"
										+ ", lastSQLCommand: " + lastSQLCommand
										+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
										+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
										+ ", rowsUpdated: " + to_string(rowsUpdated)
										+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
											chrono::system_clock::now() - startSql).count()) + "@"
									);
								}

								continue;
								// throw runtime_error(errorMessage);
							}
						}
					}

                    {
                        lastSQLCommand = 
                            "select metaDataContent from MMS_IngestionJob where ingestionJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementIngestion->setInt64(queryParameterIndex++, encodingItem->_ingestionJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> imgestionResultSet (preparedStatementIngestion->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (imgestionResultSet->next())
                        {
                            string parameters = imgestionResultSet->getString("metaDataContent");
                            
                            {
                                Json::CharReaderBuilder builder;
                                Json::CharReader* reader = builder.newCharReader();
                                string errors;

                                bool parsingSuccessful = reader->parse(parameters.c_str(),
                                        parameters.c_str() + parameters.size(), 
                                        &(encodingItem->_pictureInPictureData->_ingestedParametersRoot), &errors);
                                delete reader;

                                if (!parsingSuccessful)
                                {
                                    string errorMessage = __FILEREF__ + "failed to parse 'parameters'"
                                            + ", errors: " + errors
                                            + ", parameters: " + parameters
                                            ;
                                    _logger->error(errorMessage);

                                    // in case an encoding job row generate an error, we have to make it to Failed
                                    // otherwise we will indefinitely get this error
                                    {
										_logger->info(__FILEREF__ + "EncodingJob update"
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											);
                                        lastSQLCommand = 
                                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (
												conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                        int queryParameterIndex = 1;
                                        preparedStatementUpdate->setString(queryParameterIndex++,
												MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                        preparedStatementUpdate->setInt64(queryParameterIndex++,
												encodingItem->_encodingJobKey);

										chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
										_logger->info(__FILEREF__ + "@SQL statistics@"
											+ ", lastSQLCommand: " + lastSQLCommand
											+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
											+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
											+ ", rowsUpdated: " + to_string(rowsUpdated)
											+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
												chrono::system_clock::now() - startSql).count()) + "@"
										);
                                    }

                                    continue;
                                    // throw runtime_error(errorMessage);
                                }
                            }
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "select failed, no row returned"
                                    + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            // in case an encoding job row generate an error, we have to make it to Failed
                            // otherwise we will indefinitely get this error
                            {
								_logger->info(__FILEREF__ + "EncodingJob update"
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									);
                                lastSQLCommand = 
                                    "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                                shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                                int queryParameterIndex = 1;
                                preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                                preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

								chrono::system_clock::time_point startSql = chrono::system_clock::now();
                                int rowsUpdated = preparedStatementUpdate->executeUpdate();
								_logger->info(__FILEREF__ + "@SQL statistics@"
									+ ", lastSQLCommand: " + lastSQLCommand
									+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
									+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
										chrono::system_clock::now() - startSql).count()) + "@"
								);
                            }

                            continue;
                            // throw runtime_error(errorMessage);
                        }
                    }
                }
				/*
                else if (encodingItem->_encodingType == EncodingType::LiveProxy)
                {
                }
				*/
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingType is wrong"
                            + ", EncodingType: " + toString(encodingItem->_encodingType)
                    ;
                    _logger->error(errorMessage);

                    // in case an encoding job row generate an error, we have to make it to Failed
                    // otherwise we will indefinitely get this error
                    {
						_logger->info(__FILEREF__ + "EncodingJob update"
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							);
                        lastSQLCommand = 
                            "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementUpdate (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdate->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));
                        preparedStatementUpdate->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        int rowsUpdated = preparedStatementUpdate->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", EncodingStatus::End_Failed: " + MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
							+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                    }
                    
                    continue;
                    // throw runtime_error(errorMessage);
                }
                
                encodingItems.push_back(encodingItem);

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ?, encodingJobStart = NULL where encodingJobKey = ? and processorMMS is null";
                    shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                    preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
                    int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);
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
		_logger->info(__FILEREF__ + "getEncodingJobs statistics"
			+ ", elapsed (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count())
        );
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", processorMMS: " + processorMMS
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

int MMSEngineDBFacade::updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t mediaItemKey,
        int64_t encodedPhysicalPathKey,
        int64_t ingestionJobKey,
		string ingestionErrorMessage,
		bool forceEncodingToBeFailed)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    int encodingFailureNumber = -1;

    try
    {
		/*
		int milliSecondsToSleepWaitingLock = 500;

        PersistenceLock persistenceLock(this,
            MMSEngineDBFacade::LockType::Encoding,
            maxSecondsToWaitUpdateEncodingJobLock,
            processorMMS, "UpdateEncodingJob",
            milliSecondsToSleepWaitingLock, _logger);
		*/

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
        
        EncodingStatus newEncodingStatus;
        if (encodingError == EncodingError::PunctualError)
        {
			string type;
            {
                lastSQLCommand = 
                    "select type, failuresNumber from MMS_EncodingJob "
					"where encodingJobKey = ?";
					// "where encodingJobKey = ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (resultSet->next())
                {
                    type = resultSet->getString("type");
                    encodingFailureNumber = resultSet->getInt("failuresNumber");
                }
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingJob not found"
                            + ", EncodingJobKey: " + to_string(encodingJobKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
			string transcoderUpdate;
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
			}
            else
            {
                newEncodingStatus          = EncodingStatus::ToBeProcessed;
                encodingFailureNumber++;

				transcoderUpdate = ", transcoder = NULL";
            }

            {
				_logger->info(__FILEREF__ + "EncodingJob update"
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", processorMMS: " + "NULL"
					+ ", " + transcoderUpdate
					+ ", failuresNumber: " + to_string(encodingFailureNumber)
					+ ", encodingProgress: " + "NULL"
					);
                lastSQLCommand = 
                    string("update MMS_EncodingJob set status = ?, processorMMS = NULL") + transcoderUpdate + ", failuresNumber = ?, encodingProgress = NULL where encodingJobKey = ? and status = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++,
						MMSEngineDBFacade::toString(newEncodingStatus));
                preparedStatement->setInt(queryParameterIndex++, encodingFailureNumber);
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setString(queryParameterIndex++,
						MMSEngineDBFacade::toString(EncodingStatus::Processing));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
                int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
					+ ", encodingFailureNumber: " + to_string(encodingFailureNumber)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", MMSEngineDBFacade::toString(newEncodingStatus): "
								+ MMSEngineDBFacade::toString(newEncodingStatus)
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
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
				+ ", transcoder: " + "NULL"
				+ ", encodingProgress: " + "NULL"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, transcoder = NULL, encodingProgress = NULL "
				"where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(EncodingStatus::Processing));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
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
				+ ", encodingJobEnd: " + "NOW()"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW() "
				"where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(EncodingStatus::Processing));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
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
				+ ", encodingJobEnd: " + "NOW()"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW() "
				"where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
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
				+ ", encodingJobEnd: " + "NOW()"
				+ ", encodingProgress: " + "100"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW(), encodingProgress = 100 "
                "where encodingJobKey = ? and status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(EncodingStatus::Processing));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", EncodingStatus::Processing: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): "
							+ MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
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
            // In the Generate-Frames scenario we will have mediaItemKey and encodedPhysicalPathKey set to -1.
            // In this case we do not have to update the IngestionJob because this is done when all the images (the files generated)
            // will be ingested
            if (mediaItemKey != -1 && encodedPhysicalPathKey != -1 && ingestionJobKey != -1)
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
                updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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
            updateIngestionJob (ingestionJobKey, ingestionStatus, ingestionErrorMessage);
        }
        else if (newEncodingStatus == EncodingStatus::End_KilledByUser && ingestionJobKey != -1)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_CancelledByUser;
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
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
        }
        else if (newEncodingStatus == EncodingStatus::End_CanceledByUser && ingestionJobKey != -1)
        {
            IngestionStatus ingestionStatus = IngestionStatus::End_CancelledByUser;
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
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
    
    return encodingFailureNumber;
}

void MMSEngineDBFacade::updateIngestionAndEncodingLiveRecordingPeriod (
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		time_t utcRecordingPeriodStart,
		time_t utcRecordingPeriodEnd)
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
			_logger->info(__FILEREF__ + "IngestionJob update"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", JSON_SET...utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
				+ ", JSON_SET....utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
				);
			// "RecordingPeriod" : { "AutoRenew" : true, "End" : "2020-05-10T02:00:00Z", "Start" : "2020-05-03T02:00:00Z" }
            lastSQLCommand = 
                "update MMS_IngestionJob set "
				"metaDataContent = JSON_SET(metaDataContent, '$.RecordingPeriod.Start', DATE_FORMAT(CONVERT_TZ(FROM_UNIXTIME(?), @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ'))"
				", metaDataContent = JSON_SET(metaDataContent, '$.RecordingPeriod.End', DATE_FORMAT(CONVERT_TZ(FROM_UNIXTIME(?), @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ')) "
				"where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodStart);
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodEnd);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
				+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
					+ ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->warn(errorMessage);

				// throw runtime_error(errorMessage);
            }
        }

        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", JSON_SET...utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
				+ ", JSON_SET....utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set encodingJobStart = NOW(), "
				"parameters = JSON_SET(parameters, '$.utcRecordingPeriodStart', ?)"
				", parameters = JSON_SET(parameters, '$.utcRecordingPeriodEnd', ?) "
				"where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodStart);
            preparedStatement->setInt64(queryParameterIndex++, utcRecordingPeriodEnd);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
				+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
                        + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingJobPriority (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey,
    EncodingPriority newEncodingPriority)
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
        
        EncodingStatus currentEncodingStatus;
        EncodingPriority currentEncodingPriority;
        {
            lastSQLCommand = 
                "select status, encodingPriority from MMS_EncodingJob "
				// "where encodingJobKey = ?";
				"where encodingJobKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                currentEncodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSet->getString("status"));
                currentEncodingPriority = static_cast<EncodingPriority>(resultSet->getInt("encodingPriority"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        if (currentEncodingStatus != EncodingStatus::ToBeProcessed)
        {
            string errorMessage = __FILEREF__ + "EncodingJob cannot change EncodingPriority because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        if (currentEncodingPriority == newEncodingPriority)
        {
            string errorMessage = __FILEREF__ + "EncodingJob has already the same status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
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
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingPriority: " + to_string(static_cast<int>(newEncodingPriority))
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set encodingPriority = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newEncodingPriority));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingPriority: " + to_string(static_cast<int>(newEncodingPriority))
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingPriority: " + toString(newEncodingPriority)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        _logger->info(__FILEREF__ + "EncodingJob updated successful"
            + ", newEncodingPriority: " + toString(newEncodingPriority)
            + ", encodingJobKey: " + to_string(encodingJobKey)
        );
        
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingJobTryAgain (
    shared_ptr<Workspace> workspace,
    int64_t encodingJobKey)
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
        
        EncodingStatus currentEncodingStatus;
		int64_t ingestionJobKey;
        {
            lastSQLCommand = 
                "select status, ingestionJobKey from MMS_EncodingJob "
				// "where encodingJobKey = ?";
				"where encodingJobKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                currentEncodingStatus = MMSEngineDBFacade::toEncodingStatus(resultSet->getString("status"));
                ingestionJobKey = resultSet->getInt64("ingestionJobKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
            
        if (currentEncodingStatus != EncodingStatus::End_Failed)
        {
            string errorMessage = __FILEREF__ + "EncodingJob cannot be encoded again because of his status"
                    + ", currentEncodingStatus: " + toString(currentEncodingStatus)
                    + ", EncodingJobKey: " + to_string(encodingJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }

        EncodingStatus newEncodingStatus = EncodingStatus::ToBeProcessed;
        {
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingStatus: " + toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
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
            lastSQLCommand = 
                "update MMS_IngestionJob set status = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, toString(newIngestionStatus));
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newIngestionStatus: " + toString(newIngestionStatus)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newEncodingStatus: " + toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
        );

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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage)
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
			/* 2020-05-24: commented because already logged by the calling method
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingProgress: " + to_string(encodingPercentage)
				);
			*/
            lastSQLCommand = 
                "update MMS_EncodingJob set encodingProgress = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, encodingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingPercentage: " + to_string(encodingPercentage)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);
				*/

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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

// this method is used in case of LiveProxy:
// - it is always running except when it is killed by User
// - failuresNumber > 0 means it is failing
// - failuresNumber == 0 means it is running successful
long MMSEngineDBFacade::updateEncodingJobFailuresNumber (
        int64_t encodingJobKey,
        long failuresNumber)
{
    
    string      lastSQLCommand;
	long		previousFailuresNumber;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "select failuresNumber from MMS_EncodingJob where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				previousFailuresNumber = resultSet->getInt("failuresNumber");
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
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
            lastSQLCommand = 
                "update MMS_EncodingJob set failuresNumber = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, failuresNumber);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", failuresNumber: " + to_string(failuresNumber)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                // in case it is alyways failing, it will be already 1
				/*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingPercentage: " + to_string(encodingPercentage)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);
				*/

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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

	return previousFailuresNumber;
}

void MMSEngineDBFacade::updateEncodingJobTranscoder (
	int64_t encodingJobKey,
	string transcoder,
	string stagingEncodedAssetPathName)
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
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", transcoder: " + transcoder
				);
            lastSQLCommand = 
				"update MMS_EncodingJob set transcoder = ?, stagingEncodedAssetPathName = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, transcoder);
            preparedStatement->setString(queryParameterIndex++, stagingEncodedAssetPathName);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", transcoder: " + transcoder
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", transcoder: " + transcoder
                        + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

string MMSEngineDBFacade::getLiveRecorderOtherTranscoder (
	bool isEncodingJobKeyMain, int64_t encodingJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t otherEncodingJobKey;
        {
			if (isEncodingJobKeyMain)
				lastSQLCommand = 
					"select JSON_EXTRACT(parameters, '$.backupEncodingJobKey') as otherEncodingJobKey "
					"from MMS_EncodingJob where encodingJobKey = ?";
			else
				lastSQLCommand = 
					"select JSON_EXTRACT(parameters, '$.mainEncodingJobKey') as otherEncodingJobKey "
					"from MMS_EncodingJob where encodingJobKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
			{
				if (resultSet->isNull("otherEncodingJobKey"))
				{
					string errorMessage = __FILEREF__ + "otherEncodingJobKey is null"
                       + ", EncodingJobKey: " + to_string(encodingJobKey)
                       + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}

				otherEncodingJobKey = resultSet->getInt64("otherEncodingJobKey");
			}
			else
			{
				string errorMessage = __FILEREF__ + "EncodingJob not found"
                       + ", EncodingJobKey: " + to_string(encodingJobKey)
                       + ", lastSQLCommand: " + lastSQLCommand
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

		string otherTranscoder;
        {
			lastSQLCommand = 
				"select transcoder "
				"from MMS_EncodingJob where encodingJobKey = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, otherEncodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", otherEncodingJobKey: " + to_string(otherEncodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
			{
				if (!resultSet->isNull("transcoder"))
				{
					otherTranscoder = resultSet->getString("transcoder");
				}
			}
			else
			{
				string errorMessage = __FILEREF__ + "EncodingJob not found"
                       + ", EncodingJobKey: " + to_string(encodingJobKey)
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

		return otherTranscoder;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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


tuple<int64_t, string, string, MMSEngineDBFacade::EncodingStatus, bool, bool,
	string, MMSEngineDBFacade::EncodingStatus, int64_t>
	MMSEngineDBFacade::getEncodingJobDetails (int64_t encodingJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int64_t		ingestionJobKey;
		string      type;
		string      transcoder;
		// default initialization, important in case the calling methid calls the tie function
		EncodingStatus	status = EncodingStatus::ToBeProcessed;
		bool		highAvailability = false;
		bool		main = false;
		int64_t		theOtherEncodingJobKey = 0;
        {
            lastSQLCommand = 
                "select ingestionJobKey, type, transcoder, status, "
				"JSON_EXTRACT(parameters, '$.highAvailability') as highAvailability, "
				"JSON_EXTRACT(parameters, '$.main') as main, "
				"JSON_EXTRACT(parameters, '$.mainEncodingJobKey') as mainEncodingJobKey, "
				"JSON_EXTRACT(parameters, '$.backupEncodingJobKey') as backupEncodingJobKey "
				"from MMS_EncodingJob "
				"where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                ingestionJobKey = resultSet->getInt64("ingestionJobKey");
                type = resultSet->getString("type");
                transcoder = resultSet->getString("transcoder");
                status = toEncodingStatus(resultSet->getString("status"));

				if (!resultSet->isNull("highAvailability"))
					highAvailability = resultSet->getInt("highAvailability") == 1 ? true : false;
				if (highAvailability)
				{
					if (!resultSet->isNull("main"))
						main = resultSet->getInt("main") == 1 ? true : false;

					if (type == "LiveRecorder")
					{
						if (main)
						{
							if (!resultSet->isNull("backupEncodingJobKey"))
								theOtherEncodingJobKey = resultSet->getInt64("backupEncodingJobKey");
						}
						else
						{
							if (!resultSet->isNull("mainEncodingJobKey"))
								theOtherEncodingJobKey = resultSet->getInt64("mainEncodingJobKey");
						}
					}
				}
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

		string theOtherTranscoder;
		// default initialization, important in case the calling methid calls the tie function
		EncodingStatus theOtherStatus = EncodingStatus::ToBeProcessed;
		if (type == "LiveRecorder" && highAvailability)
        {
			// select to get the other encodingJobKey

			if (theOtherEncodingJobKey == 0)
            {
                string errorMessage = __FILEREF__ + "theOtherEncodingJobKey had to be initialized!!!"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            lastSQLCommand = 
                "select transcoder, status from MMS_EncodingJob "
				"where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, theOtherEncodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", theOtherEncodingJobKey: " + to_string(theOtherEncodingJobKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                theOtherTranscoder = resultSet->getString("transcoder");
                theOtherStatus = toEncodingStatus(resultSet->getString("status"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
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

		return make_tuple(ingestionJobKey, type, transcoder, status, highAvailability, main,
				theOtherTranscoder, theOtherStatus, theOtherEncodingJobKey);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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
            + ", encodingJobKey: " + to_string(encodingJobKey)
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

Json::Value MMSEngineDBFacade::getEncodingJobsStatus (
        shared_ptr<Workspace> workspace, int64_t encodingJobKey,
        int start, int rows,
        bool startAndEndIngestionDatePresent, string startIngestionDate, string endIngestionDate,
        bool startAndEndEncodingDatePresent, string startEncodingDate, string endEncodingDate,
        bool asc, string status, string type
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
            
            if (encodingJobKey != -1)
            {
                field = "encodingJobKey";
                requestParametersRoot[field] = encodingJobKey;
            }
            
            if (startAndEndIngestionDatePresent)
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;

                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (startAndEndEncodingDatePresent)
            {
                field = "startEncodingDate";
                requestParametersRoot[field] = startEncodingDate;

                field = "endEncodingDate";
                requestParametersRoot[field] = endEncodingDate;
            }
            
            field = "status";
            requestParametersRoot[field] = status;

			if (type != "")
			{
				field = "type";
				requestParametersRoot[field] = type;
			}

            field = "requestParameters";
            statusListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere = string ("where ir.ingestionRootKey = ij.ingestionRootKey and ij.ingestionJobKey = ej.ingestionJobKey ");
        sqlWhere += ("and ir.workspaceKey = ? ");
        if (encodingJobKey != -1)
            sqlWhere += ("and ej.encodingJobKey = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (startAndEndEncodingDatePresent)
            sqlWhere += ("and ej.encodingJobStart >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ej.encodingJobStart <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (status == "All")
            ;
        else if (status == "Completed")
            sqlWhere += ("and ej.status like 'End_%' ");
        else if (status == "Processing")
            sqlWhere += ("and ej.status = 'Processing' ");
        else if (status == "ToBeProcessed")
            sqlWhere += ("and ej.status = 'ToBeProcessed' ");
        if (type != "")
            sqlWhere += ("and ej.type = ? ");
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (encodingJobKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startIngestionDate);
                preparedStatement->setString(queryParameterIndex++, endIngestionDate);
            }
            if (startAndEndEncodingDatePresent)
            {
                preparedStatement->setString(queryParameterIndex++, startEncodingDate);
                preparedStatement->setString(queryParameterIndex++, endEncodingDate);
            }
            if (type != "")
                preparedStatement->setString(queryParameterIndex++, type);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", startEncodingDate: " + startEncodingDate
				+ ", endEncodingDate: " + endEncodingDate
				+ ", type: " + type
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
        
        Json::Value encodingJobsRoot(Json::arrayValue);
        {            
            lastSQLCommand = 
                "select ej.encodingJobKey, ej.type, ej.parameters, ej.status, ej.encodingProgress, "
				"ej.processorMMS, ej.transcoder, ej.failuresNumber, ej.encodingPriority, "
                "DATE_FORMAT(convert_tz(ej.encodingJobStart, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobStart, "
                "DATE_FORMAT(convert_tz(ej.encodingJobEnd, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as encodingJobEnd, "
                "IF(ij.startProcessing is null, NOW(), ij.startProcessing) as newStartProcessing, "
                "IF(ij.endProcessing is null, NOW(), ij.endProcessing) as newEndProcessing "
                "from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
                + sqlWhere
                + "order by newStartProcessing " + (asc ? "asc" : "desc") + ", newEndProcessing " + (asc ? "asc " : "desc ")
                + "limit ? offset ?";
            shared_ptr<sql::PreparedStatement> preparedStatementEncodingJob (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncodingJob->setInt64(queryParameterIndex++, workspace->_workspaceKey);
            if (encodingJobKey != -1)
                preparedStatementEncodingJob->setInt64(queryParameterIndex++, encodingJobKey);
            if (startAndEndIngestionDatePresent)
            {
                preparedStatementEncodingJob->setString(queryParameterIndex++, startIngestionDate);
                preparedStatementEncodingJob->setString(queryParameterIndex++, endIngestionDate);
            }
            if (startAndEndEncodingDatePresent)
            {
                preparedStatementEncodingJob->setString(queryParameterIndex++, startEncodingDate);
                preparedStatementEncodingJob->setString(queryParameterIndex++, endEncodingDate);
            }
            if (type != "")
                preparedStatementEncodingJob->setString(queryParameterIndex++, type);
            preparedStatementEncodingJob->setInt(queryParameterIndex++, rows);
            preparedStatementEncodingJob->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSetEncodingJob (preparedStatementEncodingJob->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspace->_workspaceKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", startEncodingDate: " + startEncodingDate
				+ ", endEncodingDate: " + endEncodingDate
				+ ", type: " + type
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSetEncodingJob->next())
            {
                Json::Value encodingJobRoot;

                int64_t encodingJobKey = resultSetEncodingJob->getInt64("encodingJobKey");
                
                field = "encodingJobKey";
                encodingJobRoot[field] = encodingJobKey;

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

                field = "transcoder";
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("transcoder"));

                field = "failuresNumber";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("failuresNumber");  

                field = "encodingPriority";
                encodingJobRoot[field] = toString(static_cast<EncodingPriority>(resultSetEncodingJob->getInt("encodingPriority")));

                field = "encodingPriorityCode";
                encodingJobRoot[field] = resultSetEncodingJob->getInt("encodingPriority");

                field = "maxEncodingPriorityCode";
                encodingJobRoot[field] = workspace->_maxEncodingPriority;

                encodingJobsRoot.append(encodingJobRoot);
            }
        }
        
        field = "encodingJobs";
        responseRoot[field] = encodingJobsRoot;
        
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

int MMSEngineDBFacade::addEncodingJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    int64_t destEncodingProfileKey,
    int64_t sourceMediaItemKey,
    int64_t sourcePhysicalPathKey,
    EncodingPriority encodingPriority
)
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
        
        ContentType contentType;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t localSourcePhysicalPathKey = sourcePhysicalPathKey;
        if (sourcePhysicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, sourceMediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localSourcePhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", sourceMediaItemKey: " + to_string(sourceMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        EncodingType encodingType;
        if (contentType == ContentType::Image)
            encodingType = EncodingType::EncodeImage;
        else
            encodingType = EncodingType::EncodeVideoAudio;
        
        string parameters = string()
                + "{ "
                + "\"encodingProfileKey\": " + to_string(destEncodingProfileKey)
                + ", \"sourcePhysicalPathKey\": " + to_string(localSourcePhysicalPathKey)
                + "} ";        
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_OverlayImageOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    int64_t mediaItemKey_1, int64_t physicalPathKey_1,
    int64_t mediaItemKey_2, int64_t physicalPathKey_2,
    string imagePosition_X_InPixel, string imagePosition_Y_InPixel,
    EncodingPriority encodingPriority
)
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
        
        ContentType contentType_1;
        {
            lastSQLCommand =
                "select workspaceKey, contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_1);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey_1: " + to_string(mediaItemKey_1)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                contentType_1 = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey_1: " + to_string(mediaItemKey_1)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        ContentType contentType_2;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_2);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey_2: " + to_string(mediaItemKey_2)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                contentType_2 = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey_2: " + to_string(mediaItemKey_2)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        int64_t localSourcePhysicalPathKey_1 = physicalPathKey_1;
        if (physicalPathKey_1 == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_1);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey_1: " + to_string(mediaItemKey_1)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localSourcePhysicalPathKey_1 = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey_1: " + to_string(mediaItemKey_1)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        int64_t localSourcePhysicalPathKey_2 = physicalPathKey_2;
        if (physicalPathKey_2 == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey_2);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey_2: " + to_string(mediaItemKey_2)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localSourcePhysicalPathKey_2 = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey_2: " + to_string(mediaItemKey_2)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t sourceVideoPhysicalPathKey;
        int64_t sourceImagePhysicalPathKey;
        if (contentType_1 == ContentType::Video && contentType_2 == ContentType::Image)
        {
            sourceVideoPhysicalPathKey = localSourcePhysicalPathKey_1;
            sourceImagePhysicalPathKey = localSourcePhysicalPathKey_2;
        }
        else if (contentType_1 == ContentType::Image && contentType_2 == ContentType::Video)
        {
            sourceVideoPhysicalPathKey = localSourcePhysicalPathKey_2;
            sourceImagePhysicalPathKey = localSourcePhysicalPathKey_1;
        }
        else
        {
            string errorMessage = __FILEREF__ + "addOverlayImageOnVideoJob is not receiving one Video and one Image"
                    + ", contentType_1: " + toString(contentType_1)
					+ ", mediaItemKey_1: " + to_string(mediaItemKey_1)
					+ ", physicalPathKey_1: " + to_string(physicalPathKey_1)
                    + ", contentType_2: " + toString(contentType_2)
					+ ", mediaItemKey_2: " + to_string(mediaItemKey_2)
					+ ", physicalPathKey_2: " + to_string(physicalPathKey_2)
            ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);                    
        }
        
        EncodingType encodingType = EncodingType::OverlayImageOnVideo;
        
        string parameters = string()
                + "{ "
                + "\"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)
                + ", \"sourceImagePhysicalPathKey\": " + to_string(sourceImagePhysicalPathKey)
                + ", \"imagePosition_X_InPixel\": \"" + imagePosition_X_InPixel + "\""
                + ", \"imagePosition_Y_InPixel\": \"" + imagePosition_Y_InPixel + "\""
                + "} ";
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_OverlayTextOnVideoJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,
        
    int64_t mediaItemKey, int64_t physicalPathKey,
    string text,
    string textPosition_X_InPixel,
    string textPosition_Y_InPixel,
    string fontType,
    int fontSize,
    string fontColor,
    int textPercentageOpacity,
    bool boxEnable,
    string boxColor,
    int boxPercentageOpacity
)
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
        
        ContentType contentType;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t sourceVideoPhysicalPathKey = physicalPathKey;
        if (physicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                sourceVideoPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                
        EncodingType encodingType = EncodingType::OverlayTextOnVideo;
        
        string parameters = string()
                + "{ "
                + "\"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)

                + ", \"text\": \"" + text + "\""
                + ", \"textPosition_X_InPixel\": \"" + textPosition_X_InPixel + "\""
                + ", \"textPosition_Y_InPixel\": \"" + textPosition_Y_InPixel + "\""
                + ", \"fontType\": \"" + fontType + "\""
                + ", \"fontSize\": " + to_string(fontSize)
                + ", \"fontColor\": \"" + fontColor + "\""
                + ", \"textPercentageOpacity\": " + to_string(textPercentageOpacity)
                + ", \"boxEnable\": " + (boxEnable ? "true" : "false")
                + ", \"boxColor\": \"" + boxColor + "\""
                + ", \"boxPercentageOpacity\": " + to_string(boxPercentageOpacity)

                + "} ";
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_GenerateFramesJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    EncodingPriority encodingPriority,
        
    string imageDirectory,
    double startTimeInSeconds, int maxFramesNumber, 
    string videoFilter, int periodInSeconds, 
    bool mjpeg, int imageWidth, int imageHeight,
    int64_t sourceVideoPhysicalPathKey,
    int64_t videoDurationInMilliSeconds
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_GenerateFramesJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", encodingPriority: " + toString(encodingPriority)
            + ", imageDirectory: " + imageDirectory
            + ", startTimeInSeconds: " + to_string(startTimeInSeconds)
            + ", maxFramesNumber: " + to_string(maxFramesNumber)
            + ", videoFilter: " + videoFilter
            + ", periodInSeconds: " + to_string(periodInSeconds)
            + ", mjpeg: " + to_string(mjpeg)
            + ", imageWidth: " + to_string(imageWidth)
            + ", imageHeight: " + to_string(imageHeight)
            + ", sourceVideoPhysicalPathKey: " + to_string(sourceVideoPhysicalPathKey)
            + ", videoDurationInMilliSeconds: " + to_string(videoDurationInMilliSeconds)
        );
        
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
                        
        EncodingType encodingType = EncodingType::GenerateFrames;
        
        string parameters = string()
                + "{ "
                + "\"imageDirectory\": \"" + imageDirectory + "\""

                + ", \"startTimeInSeconds\": " + to_string(startTimeInSeconds)
                + ", \"maxFramesNumber\": " + to_string(maxFramesNumber)
                + ", \"videoFilter\": \"" + videoFilter + "\""
                + ", \"periodInSeconds\": " + to_string(periodInSeconds)
                + ", \"mjpeg\": " + (mjpeg ? "true" : "false")
                + ", \"imageWidth\": " + to_string(imageWidth)
                + ", \"imageHeight\": " + to_string(imageHeight)
                + ", \"ingestionJobKey\": " + to_string(ingestionJobKey)
                + ", \"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)
                + ", \"videoDurationInMilliSeconds\": " + to_string(videoDurationInMilliSeconds)

                + "} ";
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_SlideShowJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    vector<string>& sourcePhysicalPaths,
    double durationOfEachSlideInSeconds,
    int outputFrameRate,
    EncodingPriority encodingPriority
)
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

        EncodingType encodingType = EncodingType::SlideShow;
        
        string parameters = string()
                + "{ "
                + "\"durationOfEachSlideInSeconds\": " + to_string(durationOfEachSlideInSeconds)
                + ", \"outputFrameRate\": " + to_string(outputFrameRate)
                + ", \"sourcePhysicalPaths\": [ ";
        bool firstSourcePhysicalPath = true;
        for (string sourcePhysicalPath: sourcePhysicalPaths)
        {
            if (!firstSourcePhysicalPath)
                parameters += ", ";
            parameters += ("\"" + sourcePhysicalPath + "\"");
            
            if (firstSourcePhysicalPath)
                firstSourcePhysicalPath = false;
        }
        parameters += (
                string(" ] ")
                + "} "
                );

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_FaceRecognitionJob (
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

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_FaceIdentificationJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string sourcePhysicalPath,
	string faceIdentificationCascadeName,
	string deepLearnedModelTagsCommaSeparated,
	EncodingPriority encodingPriority
)
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

        EncodingType encodingType = EncodingType::FaceIdentification;
        
        string parameters = string()
                + "{ "
                + "\"sourcePhysicalPath\": \"" + sourcePhysicalPath + "\""
                + ", \"faceIdentificationCascadeName\": \"" + faceIdentificationCascadeName + "\""
                + ", \"deepLearnedModelTagsCommaSeparated\": " + deepLearnedModelTagsCommaSeparated + ""
                + "} "
                ;

        _logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
            + ", parameters.length: " + to_string(parameters.length()));
        
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_LiveRecorderJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	bool highAvailability,
	string configurationLabel, int64_t confKey, string url,
	string userAgent,
	time_t utcRecordingPeriodStart,
	time_t utcRecordingPeriodEnd,
	bool autoRenew,
	int segmentDurationInSeconds,
	string outputFileFormat,
	EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveRecorderJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", highAvailability: " + to_string(highAvailability)
            + ", configurationLabel: " + configurationLabel
            + ", confKey: " + to_string(confKey)
            + ", url: " + url
            + ", userAgent: " + userAgent
            + ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
            + ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
            + ", autoRenew: " + to_string(autoRenew)
            + ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
            + ", outputFileFormat: " + outputFileFormat
            + ", encodingPriority: " + toString(encodingPriority)
        );

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

		int64_t mainEncodingJobKey = -1;
		{
			bool main = true;

			EncodingType encodingType = EncodingType::LiveRecorder;
        
			/*
			string parameters = string()
                + "{ "
                + "\"highAvailability\": " + to_string(highAvailability) + ""
                + ", \"main\": " + to_string(main) + ""
				// configurationLabel is used by the GUI (encodingJobs.java to get info to be displayed)
                + ", \"configurationLabel\": \"" + configurationLabel + "\""
                + ", \"confKey\": " + to_string(confKey)
                + ", \"liveURL\": \"" + liveURL + "\""
                + ", \"userAgent\": \"" + userAgent + "\""
				// utcRecordingPeriodStart/utcRecordingPeriodEnd is used by the GUI (encodingJobs.java to calculate and display the duration)
                + ", \"utcRecordingPeriodStart\": " + to_string(utcRecordingPeriodStart) + ""
                + ", \"utcRecordingPeriodEnd\": " + to_string(utcRecordingPeriodEnd) + ""
                + ", \"autoRenew\": " + to_string(autoRenew) + ""
                + ", \"segmentDurationInSeconds\": " + to_string(segmentDurationInSeconds) + ""
                + ", \"outputFileFormat\": \"" + outputFileFormat + "\""
                + "} "
                ;
			*/
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "highAvailability";
				parametersRoot[field] = highAvailability;

				field = "main";
				parametersRoot[field] = main;

				field = "configurationLabel";
				parametersRoot[field] = configurationLabel;

				field = "confKey";
				parametersRoot[field] = confKey;

				field = "url";
				parametersRoot[field] = url;

				field = "userAgent";
				parametersRoot[field] = userAgent;

				field = "utcRecordingPeriodStart";
				parametersRoot[field] = utcRecordingPeriodStart;

				field = "utcRecordingPeriodEnd";
				parametersRoot[field] = utcRecordingPeriodEnd;

				field = "autoRenew";
				parametersRoot[field] = autoRenew;

				field = "segmentDurationInSeconds";
				parametersRoot[field] = segmentDurationInSeconds;

				field = "outputFileFormat";
				parametersRoot[field] = outputFileFormat;

				Json::StreamWriterBuilder wbuilder;
				parameters = Json::writeString(wbuilder, parametersRoot);
			}

			_logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
				+ ", parameters.length: " + to_string(parameters.length()));
        
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

				lastSQLCommand = 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
				preparedStatement->setString(queryParameterIndex++, toString(encodingType));
				preparedStatement->setString(queryParameterIndex++, parameters);
				preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
				preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingType: " + toString(encodingType)
					+ ", parameters: " + parameters
					+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
					+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}

			mainEncodingJobKey = getLastInsertId(conn);
        
			if (main)
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
				updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		int64_t backupEncodingJobKey = -1;

		if (highAvailability)
		{
			bool main = false;

			EncodingType encodingType = EncodingType::LiveRecorder;
        
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "highAvailability";
				parametersRoot[field] = highAvailability;

				field = "main";
				parametersRoot[field] = main;

				field = "configurationLabel";
				parametersRoot[field] = configurationLabel;

				field = "confKey";
				parametersRoot[field] = confKey;

				field = "url";
				parametersRoot[field] = url;

				field = "userAgent";
				parametersRoot[field] = userAgent;

				field = "utcRecordingPeriodStart";
				parametersRoot[field] = utcRecordingPeriodStart;

				field = "utcRecordingPeriodEnd";
				parametersRoot[field] = utcRecordingPeriodEnd;

				field = "autoRenew";
				parametersRoot[field] = autoRenew;

				field = "segmentDurationInSeconds";
				parametersRoot[field] = segmentDurationInSeconds;

				field = "outputFileFormat";
				parametersRoot[field] = outputFileFormat;

				Json::StreamWriterBuilder wbuilder;
				parameters = Json::writeString(wbuilder, parametersRoot);
			}

			_logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
				+ ", parameters.length: " + to_string(parameters.length()));
        
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

				lastSQLCommand = 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
				preparedStatement->setString(queryParameterIndex++, toString(encodingType));
				preparedStatement->setString(queryParameterIndex++, parameters);
				preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
				preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingType: " + toString(encodingType)
					+ ", parameters: " + parameters
					+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
					+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}

			backupEncodingJobKey = getLastInsertId(conn);
        
			if (main)
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
				updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		if (mainEncodingJobKey != -1 && backupEncodingJobKey != -1)
		{
			{
				lastSQLCommand = 
					"update MMS_EncodingJob set parameters = JSON_SET(parameters, '$.backupEncodingJobKey', ?) "
					"where encodingJobKey = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, backupEncodingJobKey);
				preparedStatement->setInt64(queryParameterIndex++, mainEncodingJobKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", backupEncodingJobKey: " + to_string(backupEncodingJobKey)
					+ ", mainEncodingJobKey: " + to_string(mainEncodingJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "it should never happen"
                        + ", mainEncodingJobKey: " + to_string(mainEncodingJobKey)
                        + ", backupEncodingJobKey: " + to_string(backupEncodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}

			{
				lastSQLCommand = 
					"update MMS_EncodingJob set parameters = JSON_SET(parameters, '$.mainEncodingJobKey', ?) "
					"where encodingJobKey = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, mainEncodingJobKey);
				preparedStatement->setInt64(queryParameterIndex++, backupEncodingJobKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", mainEncodingJobKey: " + to_string(mainEncodingJobKey)
					+ ", backupEncodingJobKey: " + to_string(backupEncodingJobKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "it should never happen"
                        + ", mainEncodingJobKey: " + to_string(mainEncodingJobKey)
                        + ", backupEncodingJobKey: " + to_string(backupEncodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
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

int MMSEngineDBFacade::addEncoding_LiveProxyJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t liveURLConfKey, string configurationLabel, string url,
	string outputType, int segmentDurationInSeconds, int playlistEntriesNumber, string cdnURL,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors,
	EncodingPriority encodingPriority
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveProxyJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", liveURLConfKey: " + to_string(liveURLConfKey)
			+ ", configurationLabel: " + configurationLabel
            + ", url: " + url
            + ", outputType: " + outputType
            + ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
            + ", playlistEntriesNumber: " + to_string(playlistEntriesNumber)
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors)
            + ", waitingSecondsBetweenAttemptsInCaseOfErrors: " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors)
            + ", cdnURL: " + cdnURL
            + ", encodingPriority: " + toString(encodingPriority)
        );

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
			EncodingType encodingType = EncodingType::LiveProxy;
        
			/*
			string parameters = string()
                + "{ "
				// configurationLabel is used by the GUI (encodingJobs.java to get info to be displayed)
                + "\"liveURLConfKey\": " + to_string(liveURLConfKey)
                + ", \"configurationLabel\": \"" + configurationLabel + "\""
                + ", \"liveURL\": \"" + liveURL + "\""
                + ", \"outputType\": \"" + outputType + "\""
                + ", \"segmentDurationInSeconds\": " + to_string(segmentDurationInSeconds)
                + ", \"playlistEntriesNumber\": " + to_string(playlistEntriesNumber)
                + ", \"maxAttemptsNumberInCaseOfErrors\": " + to_string(maxAttemptsNumberInCaseOfErrors)
                + ", \"waitingSecondsBetweenAttemptsInCaseOfErrors\": " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors)
                + ", \"cdnURL\": \"" + cdnURL + "\""
                + "} "
                ;
			*/
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "liveURLConfKey";
				parametersRoot[field] = liveURLConfKey;

				field = "configurationLabel";
				parametersRoot[field] = configurationLabel;

				field = "url";
				parametersRoot[field] = url;

				field = "outputType";
				parametersRoot[field] = outputType;

				field = "segmentDurationInSeconds";
				parametersRoot[field] = segmentDurationInSeconds;

				field = "playlistEntriesNumber";
				parametersRoot[field] = playlistEntriesNumber;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

				field = "cdnURL";
				parametersRoot[field] = cdnURL;

				Json::StreamWriterBuilder wbuilder;
				parameters = Json::writeString(wbuilder, parametersRoot);
			}

			_logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
				+ ", parameters.length: " + to_string(parameters.length()));
        
			{
				// 2019-11-06: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				lastSQLCommand = 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
				preparedStatement->setString(queryParameterIndex++, toString(encodingType));
				preparedStatement->setString(queryParameterIndex++, parameters);
				preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
				preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", ingestionJobKey: " + to_string(ingestionJobKey)
					+ ", encodingType: " + toString(encodingType)
					+ ", parameters: " + parameters
					+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
					+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
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
				updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_VideoSpeed (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t mediaItemKey, int64_t physicalPathKey,
	VideoSpeedType videoSpeedType, int videoSpeedSize,
	EncodingPriority encodingPriority)
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
        
        ContentType contentType;
        {
            lastSQLCommand =
                "select contentType from MMS_MediaItem where mediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                contentType = MMSEngineDBFacade::toContentType(resultSet->getString("contentType"));
            }
            else
            {
                string errorMessage = __FILEREF__ + "mediaItemKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        int64_t sourceVideoPhysicalPathKey = physicalPathKey;
        if (physicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mediaItemKey: " + to_string(mediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                sourceVideoPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                
        EncodingType encodingType = EncodingType::VideoSpeed;
        
        string parameters = string()
                + "{ "
                + "\"sourceVideoPhysicalPathKey\": " + to_string(sourceVideoPhysicalPathKey)

                + ", \"videoSpeedType\": \"" + toString(videoSpeedType) + "\""
                + ", \"videoSpeedSize\": " + to_string(videoSpeedSize)

                + "} ";
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

int MMSEngineDBFacade::addEncoding_PictureInPictureJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	int64_t mainMediaItemKey, int64_t mainPhysicalPathKey,
	int64_t overlayMediaItemKey, int64_t overlayPhysicalPathKey,
	string overlayPosition_X_InPixel, string overlayPosition_Y_InPixel,
	string overlay_Width_InPixel, string overlay_Height_InPixel,
	bool soundOfMain, EncodingPriority encodingPriority)
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
        
        int64_t localMainPhysicalPathKey = mainPhysicalPathKey;
        if (mainPhysicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mainMediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", mainMediaItemKey: " + to_string(mainMediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localMainPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", mainMediaItemKey: " + to_string(mainMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        int64_t localOverlayPhysicalPathKey = overlayPhysicalPathKey;
        if (overlayPhysicalPathKey == -1)
        {
            lastSQLCommand =
                "select physicalPathKey from MMS_PhysicalPath "
                "where mediaItemKey = ? and encodingProfileKey is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, overlayMediaItemKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", overlayMediaItemKey: " + to_string(overlayMediaItemKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                localOverlayPhysicalPathKey = resultSet->getInt64("physicalPathKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey not found"
                        + ", overlayMediaItemKey: " + to_string(overlayMediaItemKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        EncodingType encodingType = EncodingType::PictureInPicture;

        string parameters = string()
                + "{ "
                + "\"mainVideoPhysicalPathKey\": " + to_string(localMainPhysicalPathKey)
                + ", \"overlayVideoPhysicalPathKey\": " + to_string(localOverlayPhysicalPathKey)
                + ", \"overlayPosition_X_InPixel\": \"" + overlayPosition_X_InPixel + "\""
                + ", \"overlayPosition_Y_InPixel\": \"" + overlayPosition_Y_InPixel + "\""
                + ", \"overlay_Width_InPixel\": \"" + overlay_Width_InPixel + "\""
                + ", \"overlay_Height_InPixel\": \"" + overlay_Height_InPixel + "\""
                + ", \"soundOfMain\": " + to_string(soundOfMain)
                + "} ";
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

            lastSQLCommand = 
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, transcoder, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?,                NULL,             NULL,           NULL,             ?,      NULL,         NULL,       0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setString(queryParameterIndex++, toString(encodingType));
            preparedStatement->setString(queryParameterIndex++, parameters);
            preparedStatement->setInt(queryParameterIndex++, savedEncodingPriority);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", encodingType: " + toString(encodingType)
				+ ", parameters: " + parameters
				+ ", savedEncodingPriority: " + to_string(savedEncodingPriority)
				+ ", EncodingStatus::ToBeProcessed: " + MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
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
            updateIngestionJob (conn, ingestionJobKey, newIngestionStatus, errorMessage);
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

