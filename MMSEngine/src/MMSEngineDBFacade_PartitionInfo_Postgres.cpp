
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
			string sqlStatement = fmt::format(
				"select partitionPathName, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = {} for update",
				partitionKey);
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
				string partitionPathName = res[0]["partitionPathName"].as<string>();
				uint64_t savedCurrentFreeSizeInBytes = res[0]["currentFreeSizeInBytes"].as<uint64_t>();

				SPDLOG_INFO(
					"Difference between estimate and calculate CurrentFreeSizeInBytes"
					", partitionKey: {}"
					", partitionPathName: {}"
					", savedCurrentFreeSizeInBytes: {}"
					", calculated currentFreeSizeInBytes: {}"
					", difference (saved - calculated): {}",
					partitionKey, partitionPathName,
					savedCurrentFreeSizeInBytes, currentFreeSizeInBytes,
					savedCurrentFreeSizeInBytes - currentFreeSizeInBytes
				);

				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_PartitionInfo set currentFreeSizeInBytes = {}, "
					"lastUpdateFreeSize = NOW() at time zone 'utc' "
					"where partitionKey = {} returning 1) select count(*) from rows",
					currentFreeSizeInBytes, partitionKey);
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
			else
			{
				string sqlStatement = fmt::format( 
					"insert into MMS_PartitionInfo ("
						"partitionKey, partitionPathName, currentFreeSizeInBytes, "
						"freeSpaceToLeaveInMB, lastUpdateFreeSize, enabled) values ("
						"{}, {}, {}, {}, NOW() at time zone 'utc', true)",
					partitionKey, trans.quote(partitionPathName), currentFreeSizeInBytes,
					freeSpaceToLeaveInMB);
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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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

pair<int, uint64_t> MMSEngineDBFacade::getPartitionToBeUsedAndUpdateFreeSpace(
	uint64_t fsEntrySizeInBytes
)
{
	int			partitionToBeUsed;
	uint64_t	currentFreeSizeInBytes;
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
			string sqlStatement = fmt::format( 
				"select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where (currentFreeSizeInBytes / 1000) - (freeSpaceToLeaveInMB * 1000) > {} / 1000 "
				"and enabled = true "
				"for update", fsEntrySizeInBytes);
				// "order by partitionKey asc limit 1 for update";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (res.size() == 0)
			{
				string errorMessage = string("No more space in MMS Partitions")
					+ ", fsEntrySizeInBytes: " + to_string(fsEntrySizeInBytes)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			int partitionResultSetIndexToBeUsed;
			{
				unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
				default_random_engine e(seed);
				partitionResultSetIndexToBeUsed = e() % res.size();
				// _logger->info(__FILEREF__ + "Partition to be used"
				// 	+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				// 	+ ", partitionResultSetIndexToBeUsed: " + to_string(partitionResultSetIndexToBeUsed)
				// );
			}

            {
				partitionToBeUsed = res[partitionResultSetIndexToBeUsed]["partitionKey"].as<int>();
				currentFreeSizeInBytes = res[partitionResultSetIndexToBeUsed]["currentFreeSizeInBytes"].as<uint64_t>();

				_logger->info(__FILEREF__ + "Partition to be used"
					+ ", partitionToBeUsed: " + to_string(partitionToBeUsed)
					+ ", res.size: " + to_string(res.size())
					+ ", partitionResultSetIndexToBeUsed: " + to_string(partitionResultSetIndexToBeUsed)
				);
			}
		}

		uint64_t newCurrentFreeSizeInBytes = currentFreeSizeInBytes - fsEntrySizeInBytes;
		// SPDLOG_INFO("TEST currentFreeSizeInBytes: {}, fsEntrySizeInBytes: {}, newCurrentFreeSizeInBytes: {}",
		// 	currentFreeSizeInBytes, fsEntrySizeInBytes, newCurrentFreeSizeInBytes);

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_PartitionInfo set currentFreeSizeInBytes = {}, "
				"lastUpdateFreeSize = NOW() at time zone 'utc' "
				"where partitionKey = {} returning 1) select count(*) from rows",
				newCurrentFreeSizeInBytes, partitionToBeUsed);
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

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return make_pair(partitionToBeUsed, newCurrentFreeSizeInBytes);
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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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

uint64_t MMSEngineDBFacade::updatePartitionBecauseOfDeletion(
	int partitionKey,
	uint64_t fsEntrySizeInBytes)
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
		uint64_t		currentFreeSizeInBytes;

        {
			string sqlStatement = fmt::format( 
				"select currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = {} for update",
				partitionKey);
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
				currentFreeSizeInBytes = res[0]["currentFreeSizeInBytes"].as<uint64_t>();
			else
			{
				string errorMessage = string("Partition not found")
					+ ", partitionKey: " + to_string(partitionKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		uint64_t newCurrentFreeSizeInBytes = currentFreeSizeInBytes + fsEntrySizeInBytes;
		// SPDLOG_INFO("TEST currentFreeSizeInBytes: {}, fsEntrySizeInBytes: {}, newCurrentFreeSizeInBytes: {}",
		// 	currentFreeSizeInBytes, fsEntrySizeInBytes, newCurrentFreeSizeInBytes);

		{
			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_PartitionInfo set currentFreeSizeInBytes = {} "
				"where partitionKey = {} returning 1) select count(*) from rows",
				newCurrentFreeSizeInBytes, partitionKey);
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

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		return newCurrentFreeSizeInBytes;
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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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

fs::path MMSEngineDBFacade::getPartitionPathName(int partitionKey)
{
	fs::path	partitionPathName;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
        {
			string sqlStatement = fmt::format( 
				"select partitionPathName from MMS_PartitionInfo "
				"where partitionKey = {} ",
				partitionKey);
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
				partitionPathName = res[0]["partitionPathName"].as<string>();
			else
			{
				string errorMessage = string("No partitionInfo found")
					+ ", partitionKey: " + to_string(partitionKey)
				;
				_logger->error(__FILEREF__ + errorMessage);

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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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

	return partitionPathName;
}

void MMSEngineDBFacade::getPartitionsInfo(vector<pair<int, uint64_t>>& partitionsInfo)
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	try
	{
		partitionsInfo.clear();

        {
			string sqlStatement = fmt::format( 
				"select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo ");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
				int partitionKey = row["partitionKey"].as<int>();
				uint64_t currentFreeSizeInBytes = row["currentFreeSizeInBytes"].as<uint64_t>();

				partitionsInfo.push_back(make_pair(partitionKey, currentFreeSizeInBytes));
			}
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
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
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
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

