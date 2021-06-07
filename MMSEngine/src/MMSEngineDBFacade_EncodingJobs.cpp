
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

		int liveProxyToBeEncoded = 0;
		int liveRecorderToBeEncoded = 0;
		int awaitingTheBeginningToBeEncoded = 0;
		int othersToBeEncoded = 0;

		encodingItems.clear();

		// first Live-Proxy because if we have many Live-Recording, Live-Proxy will never start
        {
			_logger->info(__FILEREF__ + "getEncodingJobs for LiveProxy");

            lastSQLCommand =
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName, "
				"JSON_EXTRACT(ej.parameters, '$.utcProxyPeriodStart') as utcProxyPeriodStart "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ij.status not like 'End_%' "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and ij.ingestionType = 'Live-Proxy' "
				"order by JSON_EXTRACT(ej.parameters, '$.utcProxyPeriodStart') asc"
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
				+ ", encodingResultSet->rowsCount: " + to_string(encodingResultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            while (encodingResultSet->next())
            {
                int64_t encodingJobKey = encodingResultSet->getInt64("encodingJobKey");

				if (!encodingResultSet->isNull("utcProxyPeriodStart"))
				{
					int64_t utcProxyPeriodStart = encodingResultSet->getInt64("utcProxyPeriodStart");
					if (utcProxyPeriodStart - utcNow >= timeBeforeToPrepareResourcesInMinutes * 60)
					{
						_logger->info(__FILEREF__ + "LiveProxy, EncodingJob discarded because too early to be processed"
                            + ", encodingJobKey: " + to_string(encodingJobKey)
						);

						continue;
					}
				}

                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();

                encodingItem->_encodingJobKey = encodingJobKey;
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
						encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("encoderKey"))
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = encodingResultSet->getInt64("encoderKey");
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
						+ ", workspaceResultSet->rowsCount: " + to_string(workspaceResultSet->rowsCount())
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
                
				// encodingItem->_ingestedParametersRoot
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
						+ ", imgestionResultSet->rowsCount: " + to_string(imgestionResultSet->rowsCount())
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
								&(encodingItem->_ingestedParametersRoot), &errors);
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
										+ ", EncodingStatus::End_Failed: "
											+ MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
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
							+ ", encodingItem->_ingestionJobKey: "
								+ to_string(encodingItem->_ingestionJobKey)
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

					string field = "outputsRoot";
					if (!JSONUtils::isMetadataPresent(encodingItem->_encodingParametersRoot, field))
                    {
                        string errorMessage = __FILEREF__ + "No outputsRoot"
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
					Json::Value outputsRoot = encodingItem->_encodingParametersRoot[field];

					for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
					{
						Json::Value outputRoot = outputsRoot[outputIndex];

						/*
						{
							Json::StreamWriterBuilder wbuilder;
							string sOutputRoot = Json::writeString(wbuilder, outputRoot);
							_logger->info(__FILEREF__ + "outputsRoot encodingProfileKey check"
								+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
								+ ", sOutputRoot: " + sOutputRoot
							);
						}
						*/

						field = "encodingProfileKey";	// added by MMSEngineProcessor
						// if not present it will be -1
						int64_t encodingProfileKey = JSONUtils::asInt64(outputRoot, field, -1);
						// outputRoot[field] = encodingProfileKey;

                        _logger->info(__FILEREF__ + "outputsRoot encodingProfileKey check"
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
						);

						Json::Value encodingProfileDetailsRoot = Json::nullValue;
						MMSEngineDBFacade::ContentType encodingProfileContentType =
							MMSEngineDBFacade::ContentType::Video;

						if (encodingProfileKey != -1)
						{
							lastSQLCommand = 
								"select contentType, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
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
								+ ", encodingProfilesResultSet->rowsCount: " + to_string(encodingProfilesResultSet->rowsCount())
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (encodingProfilesResultSet->next())
							{
								encodingProfileContentType =
									toContentType(encodingProfilesResultSet->getString("contentType"));
								string jsonEncodingProfile = encodingProfilesResultSet->getString("jsonProfile");
								{
									Json::CharReaderBuilder builder;
									Json::CharReader* reader = builder.newCharReader();
									string errors;

									bool parsingSuccessful = reader->parse(jsonEncodingProfile.c_str(),
											jsonEncodingProfile.c_str() + jsonEncodingProfile.size(), 
											&encodingProfileDetailsRoot, &errors);
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

						field = "encodingProfileDetails";
						outputRoot[field] = encodingProfileDetailsRoot;

						field = "encodingProfileContentType";
						outputRoot[field] = MMSEngineDBFacade::toString(encodingProfileContentType);

						/*
						{
							Json::StreamWriterBuilder wbuilder;
							string sOutputRoot = Json::writeString(wbuilder, outputRoot);
							_logger->info(__FILEREF__ + "2. outputsRoot encodingProfileKey check"
								+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
								+ ", encodingProfileKey: " + to_string(encodingProfileKey)
								+ ", sOutputRoot: " + sOutputRoot
							);
						}
						*/

						outputsRoot[outputIndex] = outputRoot;

						/*
						{
							Json::StreamWriterBuilder wbuilder;
							string sOutputRoot = Json::writeString(wbuilder, outputsRoot);
							_logger->info(__FILEREF__ + "3. outputsRoot encodingProfileKey check"
								+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
								+ ", encodingProfileKey: " + to_string(encodingProfileKey)
								+ ", sOutputRoot: " + sOutputRoot
							);
						}
						*/
					}
					encodingItem->_liveProxyData->_outputsRoot = outputsRoot;
					/*
					{
						Json::StreamWriterBuilder wbuilder;
						string sOutputRoot = Json::writeString(wbuilder, encodingItem->_liveProxyData->_outputsRoot);
						_logger->info(__FILEREF__ + "3. outputsRoot encodingProfileKey check"
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", sOutputRoot: " + sOutputRoot
						);
					}
					*/

					if (encodingItem->_liveProxyData->_outputsRoot.size() == 0)
                    {
                        string errorMessage = __FILEREF__ + "No outputsRoot"
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

                encodingItems.push_back(encodingItem);
				liveProxyToBeEncoded++;

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						// + ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ? " // , encodingJobStart = NULL "
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

		// next Awaiting-The-Beginning ...because if we have many Live-Recording, Live-Proxy will never start
        {
			_logger->info(__FILEREF__ + "getEncodingJobs for Awaiting-The-Beginning");

            lastSQLCommand =
				"select ej.encodingJobKey, ej.ingestionJobKey, ej.type, ej.parameters, "
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ij.status not like 'End_%' "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and ij.ingestionType = 'Awaiting-The-Beginning' "
				"order by JSON_EXTRACT(ej.parameters, '$.utcCountDownEnd') asc"
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
				+ ", encodingResultSet->rowsCount: " + to_string(encodingResultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            while (encodingResultSet->next())
            {
                int64_t encodingJobKey = encodingResultSet->getInt64("encodingJobKey");

                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
					make_shared<MMSEngineDBFacade::EncodingItem>();

                encodingItem->_encodingJobKey = encodingJobKey;
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
					encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("encoderKey"))
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = encodingResultSet->getInt64("encoderKey");
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
						+ ", workspaceResultSet->rowsCount: " + to_string(workspaceResultSet->rowsCount())
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

				// encodingItem->_ingestedParametersRoot
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
						+ ", imgestionResultSet->rowsCount: " + to_string(imgestionResultSet->rowsCount())
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
								&(encodingItem->_ingestedParametersRoot), &errors);
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
										+ ", EncodingStatus::End_Failed: "
											+ MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
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
							+ ", encodingItem->_ingestionJobKey: "
								+ to_string(encodingItem->_ingestionJobKey)
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

                // if (encodingItem->_encodingType == EncodingType::AwaitingTheBeginning)
                {
                    encodingItem->_awaitingTheBeginningData = make_shared<EncodingItem::AwaitingTheBeginningData>();

					{
						/*
						{
							Json::StreamWriterBuilder wbuilder;
							string sOutputRoot = Json::writeString(wbuilder, outputRoot);
							_logger->info(__FILEREF__ + "outputsRoot encodingProfileKey check"
								+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
								+ ", sOutputRoot: " + sOutputRoot
							);
						}
						*/

						string field = "encodingProfileKey";	// added by MMSEngineProcessor
						// if not present it will be -1
						encodingItem->_awaitingTheBeginningData->_encodingProfileKey
							= JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, -1);
						// outputRoot[field] = encodingProfileKey;

                        _logger->info(__FILEREF__ + "encodingProfileKey check"
							+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
							+ ", encodingProfileKey: " + to_string(encodingItem->_awaitingTheBeginningData->_encodingProfileKey)
						);

						encodingItem->_awaitingTheBeginningData->_encodingProfileContentType =
							MMSEngineDBFacade::ContentType::Video;

						if (encodingItem->_awaitingTheBeginningData->_encodingProfileKey != -1)
						{
							lastSQLCommand = 
								"select contentType, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
							shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementEncodingProfile->setInt64(queryParameterIndex++,
									encodingItem->_awaitingTheBeginningData->_encodingProfileKey);

							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							shared_ptr<sql::ResultSet> encodingProfilesResultSet (
								preparedStatementEncodingProfile->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", encodingProfileKey: " + to_string(encodingItem->_awaitingTheBeginningData->_encodingProfileKey)
								+ ", encodingProfilesResultSet->rowsCount: " + to_string(encodingProfilesResultSet->rowsCount())
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (encodingProfilesResultSet->next())
							{
								encodingItem->_awaitingTheBeginningData->_encodingProfileContentType =
									toContentType(encodingProfilesResultSet->getString("contentType"));
								string jsonEncodingProfile = encodingProfilesResultSet->getString("jsonProfile");
								{
									Json::CharReaderBuilder builder;
									Json::CharReader* reader = builder.newCharReader();
									string errors;

									bool parsingSuccessful = reader->parse(jsonEncodingProfile.c_str(),
											jsonEncodingProfile.c_str() + jsonEncodingProfile.size(), 
											&(encodingItem->_awaitingTheBeginningData->_encodingProfileDetailsRoot),
											&errors);
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
							}
							else
							{
								string errorMessage = __FILEREF__ + "select failed"
                                    + ", encodingProfileKey: " + to_string(encodingItem->_awaitingTheBeginningData->_encodingProfileKey)
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
                }

                encodingItems.push_back(encodingItem);
				awaitingTheBeginningToBeEncoded++;

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						// + ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ? " // , encodingJobStart = NULL "
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
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName, "
				"JSON_EXTRACT(ej.parameters, '$.utcRecordingPeriodStart') as utcRecordingPeriodStart "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ij.status not like 'End_%' "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and ij.ingestionType = 'Live-Recorder' "
				"order by JSON_EXTRACT(ej.parameters, '$.utcRecordingPeriodStart') asc"
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
				+ ", encodingResultSet->rowsCount: " + to_string(encodingResultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            while (encodingResultSet->next())
            {
                int64_t encodingJobKey = encodingResultSet->getInt64("encodingJobKey");

				if (!encodingResultSet->isNull("utcRecordingPeriodStart"))
				{
					time_t utcRecordingPeriodStart = encodingResultSet->getInt64("utcRecordingPeriodStart");
					if (utcRecordingPeriodStart - utcNow >= timeBeforeToPrepareResourcesInMinutes * 60)
					{
						_logger->info(__FILEREF__ + "LiveRecorder, EncodingJob discarded because too early to be processed"
							+ ", encodingJobKey: " + to_string(encodingJobKey)
						);

						continue;
					}
				}

                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();

                encodingItem->_encodingJobKey = encodingJobKey;
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("ingestionJobKey");
                encodingItem->_encodingType = toEncodingType(encodingResultSet->getString("type"));
                encodingItem->_encodingParameters = encodingResultSet->getString("parameters");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(
						encodingResultSet->getInt("encodingPriority"));
				if (encodingResultSet->isNull("encoderKey"))
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = encodingResultSet->getInt64("encoderKey");
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
						+ ", workspaceResultSet->rowsCount: " + to_string(workspaceResultSet->rowsCount())
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

				// encodingItem->_ingestedParametersRoot
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
						+ ", imgestionResultSet->rowsCount: " + to_string(imgestionResultSet->rowsCount())
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
								&(encodingItem->_ingestedParametersRoot), &errors);
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
										+ ", EncodingStatus::End_Failed: "
											+ MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
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
							+ ", encodingItem->_ingestionJobKey: "
								+ to_string(encodingItem->_ingestionJobKey)
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
						string field = "monitorVirtualVODEncodingProfileKey";
						// if not present it will be -1
						int64_t encodingProfileKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot, field, -1);

						if (encodingProfileKey != -1)
						{
							lastSQLCommand = 
								"select contentType, jsonProfile from MMS_EncodingProfile where encodingProfileKey = ?";
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
								+ ", encodingProfilesResultSet->rowsCount: " + to_string(encodingProfilesResultSet->rowsCount())
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (encodingProfilesResultSet->next())
							{
								encodingItem->_liveRecorderData->_monitorVirtualVODEncodingProfileContentType =
									toContentType(encodingProfilesResultSet->getString("contentType"));
								string jsonEncodingProfile = encodingProfilesResultSet->getString("jsonProfile");
								{
									Json::CharReaderBuilder builder;
									Json::CharReader* reader = builder.newCharReader();
									string errors;

									bool parsingSuccessful = reader->parse(jsonEncodingProfile.c_str(),
										jsonEncodingProfile.c_str() + jsonEncodingProfile.size(), 
										&(encodingItem->_liveRecorderData->_monitorVirtualVODEncodingProfileDetailsRoot), &errors);
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
					}
                }

                encodingItems.push_back(encodingItem);
				liveRecorderToBeEncoded++;

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						// + ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ? " // , encodingJobStart = NULL "
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
				"ej.encodingPriority, ej.encoderKey, ej.stagingEncodedAssetPathName "
				"from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ir.ingestionRootKey = ij.ingestionRootKey "
				"and ij.ingestionJobKey = ej.ingestionJobKey and ej.processorMMS is null "
				"and ij.status not like 'End_%' "
				"and ej.status = ? and ej.encodingJobStart <= NOW() "
				"and (ij.ingestionType != 'Live-Recorder' and ij.ingestionType != 'Live-Proxy' and ij.ingestionType != 'Awaiting-The-Beginning') "
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
				+ ", encodingResultSet->rowsCount: " + to_string(encodingResultSet->rowsCount())
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
				if (encodingResultSet->isNull("encoderKey"))
					encodingItem->_encoderKey = -1;
				else
					encodingItem->_encoderKey = encodingResultSet->getInt64("encoderKey");
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
						+ ", workspaceResultSet->rowsCount: " + to_string(workspaceResultSet->rowsCount())
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

				// encodingItem->_ingestedParametersRoot
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
						+ ", imgestionResultSet->rowsCount: " + to_string(imgestionResultSet->rowsCount())
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
								&(encodingItem->_ingestedParametersRoot), &errors);
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
										+ ", EncodingStatus::End_Failed: "
											+ MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
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
							+ ", encodingItem->_ingestionJobKey: "
								+ to_string(encodingItem->_ingestionJobKey)
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
									+ ", EncodingStatus::End_Failed: "
										+ MMSEngineDBFacade::toString(EncodingStatus::End_Failed)
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
                            "select deliveryTechnology, jsonProfile from MMS_EncodingProfile "
							"where encodingProfileKey = ?";
                        shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementEncodingProfile->setInt64(queryParameterIndex++,
							encodingProfileKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
                        shared_ptr<sql::ResultSet> encodingProfilesResultSet (
								preparedStatementEncodingProfile->executeQuery());
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodingProfileKey: " + to_string(encodingProfileKey)
							+ ", encodingProfilesResultSet->rowsCount: "
								+ to_string(encodingProfilesResultSet->rowsCount())
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
                        if (encodingProfilesResultSet->next())
                        {
                            encodingItem->_encodeData->_deliveryTechnology = toDeliveryTechnology(
									encodingProfilesResultSet->getString("deliveryTechnology"));
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
                }
                else if (encodingItem->_encodingType == EncodingType::SlideShow)
                {
					// nothing to do
                }
                else if (encodingItem->_encodingType == EncodingType::FaceRecognition)
                {
					// nothing to do
                }
                else if (encodingItem->_encodingType == EncodingType::FaceIdentification)
                {
					// nothing to do
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
							+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
								+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
								+ ", physicalPathResultSet->rowsCount: " + to_string(physicalPathResultSet->rowsCount())
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
                }
				else if (encodingItem->_encodingType == EncodingType::IntroOutroOverlay)
				{
					// nothing to do
				}
				/*
                else if (encodingItem->_encodingType == EncodingType::LiveProxy)
                {
                }
				*/
				else if (encodingItem->_encodingType == EncodingType::LiveGrid)
				{
					encodingItem->_liveGridData = make_shared<EncodingItem::LiveGridData>();

					string field = "encodingProfileKey";
					int64_t encodingProfileKey = JSONUtils::asInt64(encodingItem->_encodingParametersRoot,
						field, 0);

					{
						lastSQLCommand = 
							"select deliveryTechnology, jsonProfile from MMS_EncodingProfile "
							"where encodingProfileKey = ?";
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
							+ ", encodingProfilesResultSet->rowsCount: " + to_string(encodingProfilesResultSet->rowsCount())
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						if (encodingProfilesResultSet->next())
						{
							encodingItem->_liveGridData->_deliveryTechnology =
								toDeliveryTechnology(encodingProfilesResultSet->getString("deliveryTechnology"));
							string jsonEncodingProfile =
								encodingProfilesResultSet->getString("jsonProfile");

							{
								Json::CharReaderBuilder builder;
								Json::CharReader* reader = builder.newCharReader();
								string errors;

								bool parsingSuccessful = reader->parse(jsonEncodingProfile.c_str(),
									jsonEncodingProfile.c_str() + jsonEncodingProfile.size(), 
									&(encodingItem->_liveGridData->_encodingProfileDetailsRoot), &errors);
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
                }
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
				othersToBeEncoded++;

                {
					_logger->info(__FILEREF__ + "EncodingJob update"
						+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
						+ ", status: " + MMSEngineDBFacade::toString(EncodingStatus::Processing)
						+ ", processorMMS: " + processorMMS
						// + ", encodingJobStart: " + "NULL"
						);
                    lastSQLCommand = 
                        "update MMS_EncodingJob set status = ?, processorMMS = ? " // , encodingJobStart = NULL 
						"where encodingJobKey = ? and processorMMS is null";
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
			+ ", encodingItems.size: " + to_string(encodingItems.size())
			+ ", maxEncodingsNumber: " + to_string(maxEncodingsNumber)
			+ ", liveProxyToBeEncoded: " + to_string(liveProxyToBeEncoded)
			+ ", awaitingTheBeginningToBeEncoded: " + to_string(awaitingTheBeginningToBeEncoded)
			+ ", liveRecorderToBeEncoded: " + to_string(liveRecorderToBeEncoded)
			+ ", othersToBeEncoded: " + to_string(othersToBeEncoded)
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
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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

				transcoderUpdate = ", encoderKey = NULL";
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
                    string("update MMS_EncodingJob set status = ?, processorMMS = NULL")
					+ transcoderUpdate + ", failuresNumber = ?, encodingProgress = NULL where encodingJobKey = ? and status = ?";
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
				+ ", encoderKey = NULL"
				+ ", encodingProgress: " + "NULL"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, "
				"encoderKey = NULL, encodingProgress = NULL "
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
        else if (encodingError == EncodingError::CanceledByMMS)
        {
            newEncodingStatus       = EncodingStatus::End_CanceledByMMS;
            
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", status: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", processorMMS: " + "NULL"
				+ ", encodingJobEnd: " + "NOW()"
				);
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ?, processorMMS = NULL, encodingJobEnd = NOW() "
				"where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++,
					MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
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
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
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
            updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage);
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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

void MMSEngineDBFacade::forceCancelEncodingJob(
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

        {
			/* 2020-05-24: commented because already logged by the calling method
			_logger->info(__FILEREF__ + "EncodingJob update"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", encodingProgress: " + to_string(encodingPercentage)
				);
			*/
			EncodingStatus encodingStatus = EncodingStatus::End_CanceledByUser;
            lastSQLCommand = 
                "update MMS_EncodingJob set status = ? where ingestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, toString(encodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingStatus: " + toString(encodingStatus)
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			/*
            if (rowsUpdated != 1)
            {
				// 2020-05-24: It is not an error, so just comment next log
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingPercentage: " + to_string(encodingPercentage)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
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

void MMSEngineDBFacade::updateEncodingPid (
        int64_t encodingJobKey,
        int encodingPid)
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
                "update MMS_EncodingJob set encodingPid = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (encodingPid == -1)
				preparedStatement->setNull(queryParameterIndex++, sql::DataType::INTEGER);
			else
				preparedStatement->setInt(queryParameterIndex++, encodingPid);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodingPid: " + to_string(encodingPid)
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
	int64_t encoderKey,
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
				+ ", encoderKey: " + to_string(encoderKey)
				);
            lastSQLCommand = 
				"update MMS_EncodingJob set encoderKey = ?, "
				"stagingEncodedAssetPathName = ? where encodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encoderKey);
            preparedStatement->setString(queryParameterIndex++, stagingEncodedAssetPathName);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderKey: " + to_string(encoderKey)
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
                        + ", encoderKey: " + to_string(encoderKey)
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

int64_t MMSEngineDBFacade::getLiveRecorderOtherTranscoder (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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

		int64_t otherEncoderKey = -1;
        {
			lastSQLCommand = 
				"select encoderKey "
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
			{
				if (!resultSet->isNull("encoderKey"))
					otherEncoderKey = resultSet->getInt64("encoderKey");
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

		return otherEncoderKey;
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


tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus, bool, bool,
	int64_t, MMSEngineDBFacade::EncodingStatus, int64_t>
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
		int64_t		encoderKey;
		// default initialization, important in case the calling methid calls the tie function
		EncodingStatus	status = EncodingStatus::ToBeProcessed;
		bool		highAvailability = false;
		bool		main = false;
		int64_t		theOtherEncodingJobKey = 0;
        {
            lastSQLCommand = 
                "select ingestionJobKey, type, encoderKey, status, "
				"CAST(JSON_EXTRACT(parameters, '$.highAvailability') AS SIGNED INTEGER) as highAvailability, "
				"CAST(JSON_EXTRACT(parameters, '$.main') AS SIGNED INTEGER) as main, "
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
                ingestionJobKey = resultSet->getInt64("ingestionJobKey");
                type = resultSet->getString("type");
				if (resultSet->isNull("encoderKey"))
					encoderKey = -1;
				else
					encoderKey = resultSet->getInt64("encoderKey");
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

		int64_t theOtherEncoderKey;
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
                "select encoderKey, status from MMS_EncodingJob "
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->isNull("encoderKey"))
					theOtherEncoderKey = -1;
				else
					theOtherEncoderKey = resultSet->getInt64("EncoderKey");
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

		// return make_tuple(ingestionJobKey, type, encoderKey, status, highAvailability, main,
		//		theOtherEncoderKey, theOtherStatus, theOtherEncodingJobKey);
		return make_tuple(ingestionJobKey, type, encoderKey, status, highAvailability, main,
				theOtherEncoderKey, theOtherStatus, theOtherEncodingJobKey);
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

	bool asc, string status, string types
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
			sqlWhere += ("and ir.workspaceKey = ? ");
        if (encodingJobKey != -1)
            sqlWhere += ("and ej.encodingJobKey = ? ");
        if (startAndEndIngestionDatePresent)
            sqlWhere += ("and ir.ingestionDate >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ir.ingestionDate <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (startAndEndEncodingDatePresent)
            sqlWhere += ("and ej.encodingJobStart >= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) and ej.encodingJobStart <= convert_tz(STR_TO_DATE(?, '%Y-%m-%dT%H:%i:%sZ'), '+00:00', @@session.time_zone) ");
        if (encoderKey != -1)
            sqlWhere += ("and ej.encoderKey = ? ");
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
				sqlWhere += ("and ej.type = ? ");
			else
				sqlWhere += ("and ej.type in (" + typesArgument + ")");
		}
        
        Json::Value responseRoot;
        {
            lastSQLCommand = 
                string("select count(*) from MMS_IngestionRoot ir, MMS_IngestionJob ij, MMS_EncodingJob ej ")
                    + sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
				;
			else
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
            if (encoderKey != -1)
                preparedStatement->setInt64(queryParameterIndex++, encoderKey);
            if (types != "")
			{
				if (vTypes.size() == 1)
					preparedStatement->setString(queryParameterIndex++, types);
			}
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ((alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
					? "" : (", workspaceKey: " + to_string(workspace->_workspaceKey)))
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", startEncodingDate: " + startEncodingDate
				+ ", endEncodingDate: " + endEncodingDate
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", types: " + types
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
        
        Json::Value encodingJobsRoot(Json::arrayValue);
        {            
            lastSQLCommand = 
                "select ir.workspaceKey, ej.encodingJobKey, ij.ingestionJobKey, ej.type, ej.parameters, "
				"ej.status, ej.encodingProgress, ej.processorMMS, ej.encoderKey, ej.encodingPid, "
				"ej.failuresNumber, ej.encodingPriority, "
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
			if (alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
				;
			else
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
            if (encoderKey != -1)
                preparedStatementEncodingJob->setInt64(queryParameterIndex++, encoderKey);
            if (types != "")
			{
				if (vTypes.size() == 1)
					preparedStatementEncodingJob->setString(queryParameterIndex++, types);
			}
            preparedStatementEncodingJob->setInt(queryParameterIndex++, rows);
            preparedStatementEncodingJob->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSetEncodingJob (preparedStatementEncodingJob->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ((alsoEncodingJobsFromOtherWorkspaces && encoderKey != -1)
					? "" : (", workspaceKey: " + to_string(workspace->_workspaceKey)))
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", startIngestionDate: " + startIngestionDate
				+ ", endIngestionDate: " + endIngestionDate
				+ ", startEncodingDate: " + startEncodingDate
				+ ", endEncodingDate: " + endEncodingDate
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", types: " + types
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSetEncodingJob->rowsCount: " + to_string(resultSetEncodingJob->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSetEncodingJob->next())
            {
                Json::Value encodingJobRoot;

				int64_t workspaceKey = resultSetEncodingJob->getInt64("workspaceKey");

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


                int64_t encodingJobKey = resultSetEncodingJob->getInt64("encodingJobKey");
                
                field = "encodingJobKey";
                encodingJobRoot[field] = encodingJobKey;

				// if (ownedByCurrentWorkspace)
				{
					field = "ingestionJobKey";
					encodingJobRoot[field] = resultSetEncodingJob->getInt64("ingestionJobKey");
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
                encodingJobRoot[field] = static_cast<string>(resultSetEncodingJob->getString("type"));

				// if (ownedByCurrentWorkspace)
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
				/*
				else
				{
                    field = "parameters";
					encodingJobRoot[field] = Json::nullValue;
				}
				*/

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

void MMSEngineDBFacade::fixEncodingJobsHavingWrongStatus()
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	_logger->info(__FILEREF__ + "fixEncodingJobsHavingWrongStatus"
			);

    try
    {
        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		// Scenarios: IngestionJob in final status but EncodingJob not in final status
		//	This is independently by the specific instance of mms-engine (because in this scenario
		//	often the processor field is empty) but someone has to do it
		//	This scenario may happen in case the mms-engine is shutdown not in friendly way
		{
			lastSQLCommand = 
				"select ij.ingestionJobKey, ej.encodingJobKey, "
				"ij.status as ingestionJobStatus, ej.status as encodingJobStatus "
				"from MMS_IngestionJob ij, MMS_EncodingJob ej "
				"where ij.ingestionJobKey = ej.ingestionJobKey "
				"and ij.status like 'End_%' and ej.status not like 'End_%'"
			;

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
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
						-1,	// mediaItemKey,	not used
						-1, // encodedPhysicalPathKey,	not used
						ingestionJobKey,
						errorMessage
					);
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

void MMSEngineDBFacade::addEncodingJob (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?, "
				"?,                NOW(),             NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_OverlayImageOnVideoJob (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?, "
				"?,                NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_OverlayTextOnVideoJob (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_GenerateFramesJob (
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_SlideShowJob (
    shared_ptr<Workspace> workspace,
    int64_t ingestionJobKey,
    vector<string>& imagesSourcePhysicalPaths,
    double durationOfEachSlideInSeconds,
    vector<string>& audiosSourcePhysicalPaths,
    double shortestAudioDurationInSeconds,
	string videoSyncMethod,
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

		string parameters;
		{
			Json::Value parametersRoot;

			string field = "videoSyncMethod";
			parametersRoot[field] = videoSyncMethod;

			field = "outputFrameRate";
			parametersRoot[field] = outputFrameRate;

			{
				Json::Value imagesSourcePhysicalPathsRoot(Json::arrayValue);
				for (string imageSourcePhysicalPath: imagesSourcePhysicalPaths)
					imagesSourcePhysicalPathsRoot.append(imageSourcePhysicalPath);
				field = "imagesSourcePhysicalPaths";
				parametersRoot[field] = imagesSourcePhysicalPathsRoot;
			}

			field = "durationOfEachSlideInSeconds";
			parametersRoot[field] = durationOfEachSlideInSeconds;

			{
				Json::Value audiosSourcePhysicalPathsRoot(Json::arrayValue);
				for (string audioSourcePhysicalPath: audiosSourcePhysicalPaths)
					audiosSourcePhysicalPathsRoot.append(audioSourcePhysicalPath);
				field = "audiosSourcePhysicalPaths";
				parametersRoot[field] = audiosSourcePhysicalPathsRoot;
			}

			field = "shortestAudioDurationInSeconds";
			parametersRoot[field] = shortestAudioDurationInSeconds;

			Json::StreamWriterBuilder wbuilder;
			parameters = Json::writeString(wbuilder, parametersRoot);
		}

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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, "
				"parameters, encodingPriority, encodingJobStart, encodingJobEnd, "
				"encodingProgress, status, processorMMS, encoderKey, "
				"encodingPid, failuresNumber) values ("
				"                            NULL,           ?,               ?, "
				"?,          ?,                NOW(),            NULL, "
				"NULL,             ?,      NULL,         NULL, "
				"NULL,        0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
				+ ", EncodingStatus::ToBeProcessed: "
					+ MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_FaceIdentificationJob (
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_LiveRecorderJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey, string ingestionJobLabel,
	string channelType,
	// bool highAvailability,
	string configurationLabel, int64_t confKey, string url,
	string userAgent,
	time_t utcRecordingPeriodStart,
	time_t utcRecordingPeriodEnd,
	bool autoRenew,
	int segmentDurationInSeconds,
	string outputFileFormat,
	EncodingPriority encodingPriority,

	bool monitorHLS,
	string monitorManifestDirectoryPath,
	string monitorManifestFileName,

	bool liveRecorderVirtualVOD,

	// common between monitor and virtual vod
	int64_t monitorVirtualVODEncodingProfileKey,
	int monitorVirtualVODSegmentDurationInSeconds,
	int monitorVirtualVODPlaylistEntriesNumber
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveRecorderJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", ingestionJobLabel: " + ingestionJobLabel
            + ", channelType: " + channelType
            // + ", highAvailability: " + to_string(highAvailability)
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
            + ", monitorHLS: " + to_string(monitorHLS)
            + ", monitorManifestDirectoryPath: " + monitorManifestDirectoryPath
            + ", monitorManifestFileName: " + monitorManifestFileName
            + ", liveRecorderVirtualVOD: " + to_string(liveRecorderVirtualVOD)
            + ", monitorVirtualVODEncodingProfileKey: " + to_string(monitorVirtualVODEncodingProfileKey)
            + ", monitorVirtualVODSegmentDurationInSeconds: " + to_string(monitorVirtualVODSegmentDurationInSeconds)
            + ", monitorVirtualVODPlaylistEntriesNumber: " + to_string(monitorVirtualVODPlaylistEntriesNumber)

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
        
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "ingestionJobLabel";
				parametersRoot[field] = ingestionJobLabel;

				field = "channelType";
				parametersRoot[field] = channelType;

				// field = "highAvailability";
				// parametersRoot[field] = highAvailability;

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

				field = "monitorHLS";
				parametersRoot[field] = monitorHLS;

				field = "monitorManifestDirectoryPath";
				parametersRoot[field] = monitorManifestDirectoryPath;

				field = "monitorManifestFileName";
				parametersRoot[field] = monitorManifestFileName;

				field = "liveRecorderVirtualVOD";
				parametersRoot[field] = liveRecorderVirtualVOD;

				field = "monitorVirtualVODEncodingProfileKey";
				parametersRoot[field] = monitorVirtualVODEncodingProfileKey;

				field = "monitorVirtualVODSegmentDurationInSeconds";
				parametersRoot[field] = monitorVirtualVODSegmentDurationInSeconds;

				field = "monitorVirtualVODPlaylistEntriesNumber";
				parametersRoot[field] = monitorVirtualVODPlaylistEntriesNumber;

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
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?, "
					"NOW(),            NULL,           NULL,             ?,      NULL, "
					"NULL,       NULL,        0)";

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

		/*
		if (highAvailability)
		{
			bool main = false;

			EncodingType encodingType = EncodingType::LiveRecorder;
        
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "ingestionJobLabel";
				parametersRoot[field] = ingestionJobLabel;

				field = "channelType";
				parametersRoot[field] = channelType;

				field = "highAvailability";
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

				// false 
				// 1. because HLS cannot generate ts files in the same directory
				field = "monitorHLS";
				parametersRoot[field] = false;

				field = "monitorManifestDirectoryPath";
				parametersRoot[field] = monitorManifestDirectoryPath;

				field = "monitorManifestFileName";
				parametersRoot[field] = monitorManifestFileName;

				// false 
				// 1. because HLS cannot generate ts files in the same directory
				field = "liveRecorderVirtualVOD";
				parametersRoot[field] = false;

				field = "monitorVirtualVODEncodingProfileKey";
				parametersRoot[field] = monitorVirtualVODEncodingProfileKey;

				field = "monitorVirtualVODSegmentDurationInSeconds";
				parametersRoot[field] = monitorVirtualVODSegmentDurationInSeconds;

				field = "monitorVirtualVODPlaylistEntriesNumber";
				parametersRoot[field] = monitorVirtualVODPlaylistEntriesNumber;

				Json::StreamWriterBuilder wbuilder;
				parameters = Json::writeString(wbuilder, parametersRoot);
			}

			_logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
				+ ", parameters.length: " + to_string(parameters.length()));
        
			{
				// int savedEncodingPriority = static_cast<int>(encodingPriority);
				// if (savedEncodingPriority > workspace->_maxEncodingPriority)
				// {
				// 	_logger->warn(__FILEREF__ + "EncodingPriority was decreased because overcome the max allowed by this customer"
				// 		+ ", workspace->_maxEncodingPriority: " + to_string(workspace->_maxEncodingPriority)
				// 		+ ", requested encoding priority: " + to_string(static_cast<int>(encodingPriority))
				// 	);

				// 	savedEncodingPriority = workspace->_maxEncodingPriority;
				// }
				// 2019-04-23: we will force the encoding priority to high to be sure this EncodingJob
				//	will be managed as soon as possible
				int savedEncodingPriority = static_cast<int>(EncodingPriority::High);

				lastSQLCommand = 
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?, "
					"NOW(),            NULL,           NULL,             ?,      NULL, "
					"NULL,       NULL,        0)";

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
		*/

		/*
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
		*/

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

void MMSEngineDBFacade::addEncoding_LiveProxyJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string channelType,
	int64_t liveURLConfKey, string configurationLabel, string url,
	bool timePeriod, int64_t utcProxyPeriodStart, int64_t utcProxyPeriodEnd,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors,
	Json::Value outputsRoot
)
{

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveProxyJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", channelType: " + channelType
			+ ", liveURLConfKey: " + to_string(liveURLConfKey)
			+ ", configurationLabel: " + configurationLabel
            + ", url: " + url
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors)
            + ", waitingSecondsBetweenAttemptsInCaseOfErrors: " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors)
            + ", outputsRoot.size: " + to_string(outputsRoot.size())
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
        
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "channelType";
				parametersRoot[field] = channelType;

				field = "liveURLConfKey";
				parametersRoot[field] = liveURLConfKey;

				field = "configurationLabel";
				parametersRoot[field] = configurationLabel;

				field = "url";
				parametersRoot[field] = url;

				field = "timePeriod";
				parametersRoot[field] = timePeriod;

				field = "utcProxyPeriodStart";
				parametersRoot[field] = utcProxyPeriodStart;

				field = "utcProxyPeriodEnd";
				parametersRoot[field] = utcProxyPeriodEnd;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

				field = "outputsRoot";
				parametersRoot[field] = outputsRoot;

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
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?, "
					"NOW(),            NULL,           NULL,             ?,      NULL, "
					"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_AwaitingTheBeginningJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,
	string mmsSourceVideoAssetPathName,
	int64_t videoDurationInMilliSeconds,
	int64_t utcIngestionJobStartProcessing,
	int64_t utcCountDownEnd,
	int64_t deliveryCode,
	string outputType,
	int segmentDurationInSeconds, int playlistEntriesNumber,
	int64_t encodingProfileKey,
	string manifestDirectoryPath, string manifestFileName, string rtmpUrl,
	long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors)
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
			EncodingType encodingType = EncodingType::AwaitingTheBeginning;
        
			string parameters;
			{
				Json::Value parametersRoot;

				string field = "mmsSourceVideoAssetPathName";
				parametersRoot[field] = mmsSourceVideoAssetPathName;

				field = "videoDurationInMilliSeconds";
				parametersRoot[field] = videoDurationInMilliSeconds;

				field = "utcIngestionJobStartProcessing";
				parametersRoot[field] = utcIngestionJobStartProcessing;

				field = "utcCountDownEnd";
				parametersRoot[field] = utcCountDownEnd;

				field = "deliveryCode";
				parametersRoot[field] = deliveryCode;

				field = "outputType";
				parametersRoot[field] = outputType;

				field = "segmentDurationInSeconds";
				parametersRoot[field] = segmentDurationInSeconds;

				field = "playlistEntriesNumber";
				parametersRoot[field] = playlistEntriesNumber;

				field = "encodingProfileKey";
				parametersRoot[field] = encodingProfileKey;

				field = "manifestDirectoryPath";
				parametersRoot[field] = manifestDirectoryPath;

				field = "manifestFileName";
				parametersRoot[field] = manifestFileName;

				field = "rtmpUrl";
				parametersRoot[field] = rtmpUrl;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

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
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?, "
					"NOW(),            NULL,           NULL,             ?,      NULL, "
					"NULL,       NULL,        0)";

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

void MMSEngineDBFacade::addEncoding_LiveGridJob (
		shared_ptr<Workspace> workspace,
		int64_t ingestionJobKey,
		vector<tuple<int64_t, string, string>>& inputChannels,
		int64_t encodingProfileKey,
		string outputType,
		string manifestDirectoryPath, string manifestFileName,
		int segmentDurationInSeconds, int playlistEntriesNumber,
		string srtURL,
		long maxAttemptsNumberInCaseOfErrors, long waitingSecondsBetweenAttemptsInCaseOfErrors
	)
{

    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
        _logger->info(__FILEREF__ + "addEncoding_LiveGridJob"
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            + ", inputChannels.size: " + to_string(inputChannels.size())
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
            + ", outputType: " + outputType
            + ", manifestDirectoryPath: " + manifestDirectoryPath
            + ", manifestFileName: " + manifestFileName,
            + ", segmentDurationInSeconds: " + to_string(segmentDurationInSeconds)
            + ", playlistEntriesNumber: " + to_string(playlistEntriesNumber)
            + ", srtURL: " + srtURL
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors)
            + ", waitingSecondsBetweenAttemptsInCaseOfErrors: " + to_string(waitingSecondsBetweenAttemptsInCaseOfErrors)
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
			EncodingType encodingType = EncodingType::LiveGrid;

			string parameters;
			{
				Json::Value parametersRoot;

				string field;

				{
					Json::Value inputChannelsRoot(Json::arrayValue);

					for (int inputChannelIndex = 0; inputChannelIndex < inputChannels.size(); inputChannelIndex++)
					{
						tuple<int64_t, string, string> inputChannel = inputChannels[inputChannelIndex];

						int64_t inputChannelConfKey;
						string inputConfigurationLabel;
						string inputChannelURL;
						tie(inputChannelConfKey, inputConfigurationLabel, inputChannelURL) = inputChannel;

						Json::Value inputChannelRoot;

						field = "inputChannelConfKey";
						inputChannelRoot[field] = inputChannelConfKey;

						field = "inputConfigurationLabel";
						inputChannelRoot[field] = inputConfigurationLabel;

						field = "inputChannelURL";
						inputChannelRoot[field] = inputChannelURL;

						inputChannelsRoot.append(inputChannelRoot);
					}

					field = "inputChannels";
					parametersRoot[field] = inputChannelsRoot;
				}

				field = "encodingProfileKey";
				parametersRoot[field] = encodingProfileKey;

				field = "outputType";
				parametersRoot[field] = outputType;

				field = "manifestDirectoryPath";
				parametersRoot[field] = manifestDirectoryPath;

				field = "manifestFileName";
				parametersRoot[field] = manifestFileName;

				field = "segmentDurationInSeconds";
				parametersRoot[field] = segmentDurationInSeconds;

				field = "playlistEntriesNumber";
				parametersRoot[field] = playlistEntriesNumber;

				field = "srtURL";
				parametersRoot[field] = srtURL;

				field = "maxAttemptsNumberInCaseOfErrors";
				parametersRoot[field] = maxAttemptsNumberInCaseOfErrors;

				field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
				parametersRoot[field] = waitingSecondsBetweenAttemptsInCaseOfErrors;

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
					"insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
					"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
					"encoderKey, encodingPid, failuresNumber) values ("
												"NULL,           ?,               ?,    ?,          ?, "
					"NOW(),            NULL,           NULL,             ?,      NULL, "
					"NULL,       NULL,        0)";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

void MMSEngineDBFacade::addEncoding_VideoSpeed (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

void MMSEngineDBFacade::addEncoding_PictureInPictureJob (
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, encodingPriority, "
				"encodingJobStart, encodingJobEnd, encodingProgress, status, processorMMS, "
				"encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?,          ?, "
				"NOW(),            NULL,           NULL,             ?,      NULL, "
				"NULL,       NULL,        0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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

void MMSEngineDBFacade::addEncoding_IntroOutroOverlayJob (
	shared_ptr<Workspace> workspace,
	int64_t ingestionJobKey,

	int64_t encodingProfileKey,
	Json::Value encodingProfileDetailsRoot,

	int64_t introVideoPhysicalPathKey,
	string introVideoAssetPathName,
	int64_t introVideoDurationInMilliSeconds,

	int64_t mainVideoPhysicalPathKey,
	string mainVideoAssetPathName,
	int64_t mainVideoDurationInMilliSeconds,

	int64_t outroVideoPhysicalPathKey,
	string outroVideoAssetPathName,
	int64_t outroVideoDurationInMilliSeconds,

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

		EncodingType encodingType = EncodingType::IntroOutroOverlay;
		string parameters;
		{
			Json::Value parametersRoot;

			string field = "encodingProfileKey";
			parametersRoot[field] = encodingProfileKey;

			field = "encodingProfileDetailsRoot";
			parametersRoot[field] = encodingProfileDetailsRoot;

			field = "introVideoPhysicalPathKey";
			parametersRoot[field] = introVideoPhysicalPathKey;

			field = "introVideoAssetPathName";
			parametersRoot[field] = introVideoAssetPathName;

			field = "introVideoDurationInMilliSeconds";
			parametersRoot[field] = introVideoDurationInMilliSeconds;

			field = "mainVideoPhysicalPathKey";
			parametersRoot[field] = mainVideoPhysicalPathKey;

			field = "mainVideoAssetPathName";
			parametersRoot[field] = mainVideoAssetPathName;

			field = "mainVideoDurationInMilliSeconds";
			parametersRoot[field] = mainVideoDurationInMilliSeconds;

			field = "outroVideoPhysicalPathKey";
			parametersRoot[field] = outroVideoPhysicalPathKey;

			field = "outroVideoAssetPathName";
			parametersRoot[field] = outroVideoAssetPathName;

			field = "outroVideoDurationInMilliSeconds";
			parametersRoot[field] = outroVideoDurationInMilliSeconds;
			Json::StreamWriterBuilder wbuilder;
			parameters = Json::writeString(wbuilder, parametersRoot);
		}

		_logger->info(__FILEREF__ + "insert into MMS_EncodingJob"
			+ ", parameters.length: " + to_string(parameters.length()));

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
                "insert into MMS_EncodingJob(encodingJobKey, ingestionJobKey, type, parameters, "
				"encodingPriority, encodingJobStart, encodingJobEnd, encodingProgress, "
				"status, processorMMS, encoderKey, encodingPid, failuresNumber) values ("
                                            "NULL,           ?,               ?,    ?, "
				"?,                NOW(),            NULL,           NULL, "
				"?,      NULL,         NULL,       NULL,        0)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
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
				+ ", EncodingStatus::ToBeProcessed: "
					+ MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed)
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

