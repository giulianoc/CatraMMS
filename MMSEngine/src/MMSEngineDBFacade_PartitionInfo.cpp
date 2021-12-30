
#include <random>
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void MMSEngineDBFacade::addUpdatePartitionInfo(
	int partitionKey,
	string partitionPathName,
	uint64_t currentFreeSizeInBytes,
	int64_t freeSpaceToLeaveInMB
)
{
	string		lastSQLCommand;

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
			lastSQLCommand =
				"select partitionPathName, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt(queryParameterIndex++, partitionKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				string partitionPathName = resultSet->getString("partitionPathName");
				uint64_t savedCurrentFreeSizeInBytes = resultSet->getUInt64("currentFreeSizeInBytes");

				_logger->info(__FILEREF__
					+ "Difference between estimate and calculate CurrentFreeSizeInBytes"
					+ ", partitionKey: " + to_string(partitionKey)
					+ ", partitionPathName: " + partitionPathName
					+ ", savedCurrentFreeSizeInBytes: " + to_string(savedCurrentFreeSizeInBytes)
					+ ", calculated currentFreeSizeInBytes: " + to_string(currentFreeSizeInBytes)
					+ ", difference (saved - calculated): "
						+ to_string(savedCurrentFreeSizeInBytes - currentFreeSizeInBytes)
				);

				lastSQLCommand =
					"update MMS_PartitionInfo set currentFreeSizeInBytes = ?, "
					"lastUpdateFreeSize = NOW() "
					"where partitionKey = ?";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setUInt64(queryParameterIndex++, currentFreeSizeInBytes);
				preparedStatement->setInt64(queryParameterIndex++, partitionKey);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", currentFreeSizeInBytes: " + to_string(currentFreeSizeInBytes)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
			}
			else
			{
				lastSQLCommand = 
					"insert into MMS_PartitionInfo ("
						"partitionKey, partitionPathName, currentFreeSizeInBytes, "
						"freeSpaceToLeaveInMB, lastUpdateFreeSize) values ("
						"?, ?, ?, ?, NOW())";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt(queryParameterIndex++, partitionKey);
				preparedStatement->setString(queryParameterIndex++, partitionPathName);
				preparedStatement->setUInt64(queryParameterIndex++, currentFreeSizeInBytes);
				preparedStatement->setInt64(queryParameterIndex++, freeSpaceToLeaveInMB);

				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				preparedStatement->executeUpdate();
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", partitionKey: " + to_string(partitionKey)
					+ ", partitionPathName: " + partitionPathName
					+ ", currentFreeSizeInBytes: " + to_string(currentFreeSizeInBytes)
					+ ", freeSpaceToLeaveInMB: " + to_string(freeSpaceToLeaveInMB)
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
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

pair<int, uint64_t> MMSEngineDBFacade::getPartitionToBeUsedAndUpdateFreeSpace(
	int64_t ullFSEntrySizeInBytes
)
{
	string		lastSQLCommand;
	int			partitionToBeUsed;
	uint64_t	currentFreeSizeInBytes;


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
			lastSQLCommand = 
				"select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where (currentFreeSizeInBytes / 1000) - (freeSpaceToLeaveInMB * 1000) > ? / 1000 "
				"for update";
				// "order by partitionKey asc limit 1 for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, ullFSEntrySizeInBytes);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

			if (resultSet->rowsCount() == 0)
			{
				string errorMessage = string("No more space in MMS Partitions")
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			int partitionResultSetIndexToBeUsed;
			{
				unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
				default_random_engine e(seed);
				partitionResultSetIndexToBeUsed = e() % resultSet->rowsCount();
				// _logger->info(__FILEREF__ + "Partition to be used"
				// 	+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				// 	+ ", partitionResultSetIndexToBeUsed: " + to_string(partitionResultSetIndexToBeUsed)
				// );
				for (int resultSetIndex = 0; resultSetIndex < partitionResultSetIndexToBeUsed;
					resultSetIndex++)
					resultSet->next();
			}

            if (resultSet->next())
            {
				partitionToBeUsed = resultSet->getInt("partitionKey");
				currentFreeSizeInBytes = resultSet->getUInt64("currentFreeSizeInBytes");

				_logger->info(__FILEREF__ + "Partition to be used"
					+ ", partitionToBeUsed: " + to_string(partitionToBeUsed)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", partitionResultSetIndexToBeUsed: " + to_string(partitionResultSetIndexToBeUsed)
				);
			}
			else
			{
				string errorMessage = string("No more space in MMS Partitions")
					+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		currentFreeSizeInBytes -= ullFSEntrySizeInBytes;

		{
			lastSQLCommand =
				"update MMS_PartitionInfo set currentFreeSizeInBytes = ?, "
				"lastUpdateFreeSize = NOW() "
				"where partitionKey = ?";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setUInt64(queryParameterIndex++, currentFreeSizeInBytes);
			preparedStatement->setInt(queryParameterIndex++, partitionToBeUsed);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", ullFSEntrySizeInBytes: " + to_string(ullFSEntrySizeInBytes)
				+ ", partitionKey: " + to_string(partitionToBeUsed)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

	return make_pair(partitionToBeUsed, currentFreeSizeInBytes);
}

uint64_t MMSEngineDBFacade::updatePartitionBecauseOfDeletion(
	int partitionKey,
	int64_t ullFSEntrySizeInBytes)
{
	string		lastSQLCommand;
	uint64_t		newCurrentFreeSizeInBytes;


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
			lastSQLCommand = 
				"select currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt(queryParameterIndex++, partitionKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				newCurrentFreeSizeInBytes = resultSet->getUInt64("currentFreeSizeInBytes");
			}
			else
			{
				string errorMessage = string("Partition not found")
					+ ", partitionKey: " + to_string(partitionKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		newCurrentFreeSizeInBytes += ullFSEntrySizeInBytes;

		{
			lastSQLCommand =
				"update MMS_PartitionInfo set currentFreeSizeInBytes = ? "
				"where partitionKey = ?";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setUInt64(queryParameterIndex++, newCurrentFreeSizeInBytes);
			preparedStatement->setInt(queryParameterIndex++, partitionKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newCurrentFreeSizeInBytes: " + to_string(newCurrentFreeSizeInBytes)
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
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

	return newCurrentFreeSizeInBytes;
}

string MMSEngineDBFacade::getPartitionPathName(int partitionKey)
{
	string		lastSQLCommand;
	string		partitionPathName;


	shared_ptr<MySQLConnection> conn = nullptr;

	try
	{
		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

        {
			lastSQLCommand = 
				"select partitionPathName from MMS_PartitionInfo "
				"where partitionKey = ? ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt(queryParameterIndex++, partitionKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", partitionKey: " + to_string(partitionKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				partitionPathName = static_cast<string>(resultSet->getString("partitionPathName"));
			}
			else
			{
				string errorMessage = string("No partitionInfo found")
					+ ", partitionKey: " + to_string(partitionKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

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

	return partitionPathName;
}

void MMSEngineDBFacade::getPartitionsInfo(vector<pair<int, uint64_t>>& partitionsInfo)
{
	string		lastSQLCommand;


	shared_ptr<MySQLConnection> conn = nullptr;

	try
	{
		partitionsInfo.clear();

		conn = _connectionPool->borrow();	
		_logger->debug(__FILEREF__ + "DB connection borrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
		);

        {
			lastSQLCommand = 
				"select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo ";
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
				int partitionKey = resultSet->getInt("partitionKey");
				uint64_t currentFreeSizeInBytes = resultSet->getUInt64("currentFreeSizeInBytes");

				partitionsInfo.push_back(make_pair(partitionKey, currentFreeSizeInBytes));
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

