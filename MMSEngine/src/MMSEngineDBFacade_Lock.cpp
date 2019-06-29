
#include "MMSEngineDBFacade.h"


void MMSEngineDBFacade::setLock(
    LockType lockType, int waitingTimeoutInSecondsIfLocked,
	string owner, string data
	)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
		string sLockType = toString(lockType);

        conn = _connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		bool alreadyLocked = false;
		time_t utcCallStart = time(NULL);

		do
		{
			autoCommit = false;
			// conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
			{
				lastSQLCommand = 
					"START TRANSACTION";

				shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
				statement->execute(lastSQLCommand);
			}
        
			bool active;
			long maxDurationInMinutes;
			string start;
			string previousOwner;
			{
				lastSQLCommand = 
					"select active, maxDurationInMinutes, owner, "
					"DATE_FORMAT(convert_tz(start, @@session.time_zone, '+00:00'), '%Y-%m-%dT%H:%i:%sZ') as start "
					"from MMS_Lock where type = ? for update";
				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndexIngestionJob = 1;
				preparedStatement->setString(queryParameterIndexIngestionJob++, sLockType);

				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

				if (!resultSet->next())
				{
					string errorMessage = __FILEREF__ + "LockType not found"
						+ ", type: " + sLockType
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				active = resultSet->getInt("active") == 1 ? true : false;
				maxDurationInMinutes = resultSet->getInt("maxDurationInMinutes");
				start = resultSet->getString("start");
				previousOwner = resultSet->getString("owner");
			}

			alreadyLocked = false;

			if (active)
			{
				// check maxDurationInMinutes

				time_t utcStart;
				{
					unsigned long		ulUTCYear;
					unsigned long		ulUTCMonth;
					unsigned long		ulUTCDay;
					unsigned long		ulUTCHour;
					unsigned long		ulUTCMinutes;
					unsigned long		ulUTCSeconds;
					tm					tmStart;
					int					sscanfReturn;


					if ((sscanfReturn = sscanf (start.c_str(),
						"%4lu-%2lu-%2luT%2lu:%2lu:%2luZ",
						&ulUTCYear,
						&ulUTCMonth,
						&ulUTCDay,
						&ulUTCHour,
						&ulUTCMinutes,
						&ulUTCSeconds)) != 6)
					{
						string errorMessage = __FILEREF__ + "start has a wrong format (sscanf failed)"
							+ ", sscanfReturn: " + to_string(sscanfReturn)
							;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					time (&utcStart);
					gmtime_r(&utcStart, &tmStart);

					tmStart.tm_year		= ulUTCYear - 1900;
					tmStart.tm_mon		= ulUTCMonth - 1;
					tmStart.tm_mday		= ulUTCDay;
					tmStart.tm_hour		= ulUTCHour;
					tmStart.tm_min		= ulUTCMinutes;
					tmStart.tm_sec		= ulUTCSeconds;

					utcStart = timegm(&tmStart);
				}

				time_t utcNow;
				{
					chrono::system_clock::time_point now = chrono::system_clock::now();
					utcNow = chrono::system_clock::to_time_t(now);
				}

				if (utcNow - utcStart <= maxDurationInMinutes * 60)
				{
					string errorMessage = __FILEREF__ + "setLock " + toString(lockType)
						+ ", already locked by " + previousOwner
						;
					_logger->warn(errorMessage);

					alreadyLocked = true;
				}
				else
				{
					_logger->warn(__FILEREF__ + "Already locked since too much time. Reset it"
						+ ", type: " + sLockType
						+ ", previousOwner: " + previousOwner
					);
				}
			}

			if (!alreadyLocked)
			{
				lastSQLCommand = 
					"update MMS_Lock set start = NOW(), end = NULL, active = 1, lastUpdate = NOW(), owner = ?";
				if(data != "no data")
					lastSQLCommand += ", data = ? ";
				else
					lastSQLCommand += " ";
				lastSQLCommand += "where type = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setString(queryParameterIndex++, owner);
				if(data != "no data")
				{
					if (data == "")
						preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
					else
						preparedStatement->setString(queryParameterIndex++, data);
				}
				preparedStatement->setString(queryParameterIndex++, sLockType);

				int rowsUpdated = preparedStatement->executeUpdate();
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
						+ ", type: " + sLockType
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
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

			if (alreadyLocked && waitingTimeoutInSecondsIfLocked > 0)
			{
				int milliSecondsToSleepWaitingLock = 500;

			    this_thread::sleep_for(chrono::milliseconds(milliSecondsToSleepWaitingLock));
			}
		}
		while (alreadyLocked && time(NULL) - utcCallStart < waitingTimeoutInSecondsIfLocked);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        _connectionPool->unborrow(conn);
		conn = nullptr;

		if (alreadyLocked)
		{
			string errorMessage = __FILEREF__ + "Ingestion already Locked"
				+ ", type: " + sLockType
				+ ", waitingTimeoutInSecondsIfLocked: " + to_string(waitingTimeoutInSecondsIfLocked)
			;
			_logger->warn(errorMessage);

			throw AlreadyLocked();
		}
		else
		{
			int timeToLock = time(NULL) - utcCallStart;

			if (timeToLock > 1)
			{
				_logger->warn(__FILEREF__ + "Ingestion Lock"
					+ ", type: " + sLockType
					+ ", time to lock (secs): " + to_string(timeToLock)
				);
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
        _logger->warn(__FILEREF__ + "SQL exception"
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

void MMSEngineDBFacade::releaseLock(
    LockType lockType, string data
	)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

    try
    {
		string sLockType = toString(lockType);

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
        
		bool active;
		string owner;
		int lockDuration;
		{
			lastSQLCommand = 
				"select owner, active, IF(start is null, 0, TIME_TO_SEC(TIMEDIFF(NOW(), start))) as lockDuration "
				"from MMS_Lock where type = ? for update";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndexIngestionJob = 1;
			preparedStatement->setString(queryParameterIndexIngestionJob++, sLockType);

			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

			if (!resultSet->next())
			{
				string errorMessage = __FILEREF__ + "LockType not found"
					+ ", type: " + sLockType
					;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			active = resultSet->getInt("active") == 1 ? true : false;
			owner = resultSet->getString("owner");
			lockDuration = resultSet->getInt("lockDuration");
		}

		if (!active)
		{
			string errorMessage = __FILEREF__ + "Lock already not locked"
				+ ", type: " + sLockType
				+ ", owner: " + owner
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			string lockStatisticMessage = __FILEREF__ + "MMS_Lock duration"
				+ ", type: @" + sLockType + "@"
				+ ", owner: @" + owner + "@"
				+ ", lockDuration: @" + to_string(lockDuration) + "@"
			;
			_logger->info(lockStatisticMessage);
		}

		{
			lastSQLCommand =
				"update MMS_Lock set end = NOW(), active = 0, lastUpdate = NOW(), "
				"lastDurationInMilliSecs = TIMESTAMPDIFF(microsecond, start, NOW()) / 1000, owner = NULL";
			if(data != "no data")
				lastSQLCommand += ", data = ? ";
			else
				lastSQLCommand += " ";
			lastSQLCommand += "where type = ?";
			shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if(data != "no data")
			{
				if (data == "")
					preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
				else
					preparedStatement->setString(queryParameterIndex++, data);
			}
			preparedStatement->setString(queryParameterIndex++, sLockType);

			int rowsUpdated = preparedStatement->executeUpdate();
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "MMS_Lock, no update was done"
					+ ", type: " + sLockType
					+ ", owner: " + owner
				;
				_logger->warn(errorMessage);

				throw runtime_error(errorMessage);
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

