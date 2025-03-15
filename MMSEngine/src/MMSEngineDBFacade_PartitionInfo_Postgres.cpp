
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include <random>

void MMSEngineDBFacade::addUpdatePartitionInfo(
	int partitionKey, string partitionPathName, uint64_t currentFreeSizeInBytes, int64_t freeSpaceToLeaveInMB
)
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
		SPDLOG_INFO("mon currentFreeSizeInBytes. addUpdatePartitionInfo, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);
		{
			string sqlStatement = std::format(
				"select partitionPathName, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = {} for update",
				partitionKey
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
				string partitionPathName = res[0]["partitionPathName"].as<string>();
				uint64_t savedCurrentFreeSizeInBytes = res[0]["currentFreeSizeInBytes"].as<uint64_t>();
				SPDLOG_INFO("mon currentFreeSizeInBytes. addUpdatePartitionInfo, savedCurrentFreeSizeInBytes: {}", savedCurrentFreeSizeInBytes);

				SPDLOG_INFO(
					"Difference between estimate and calculate CurrentFreeSizeInBytes"
					", partitionKey: {}"
					", partitionPathName: {}"
					", savedCurrentFreeSizeInBytes: {}"
					", calculated currentFreeSizeInBytes: {}"
					", difference (saved - calculated): {}",
					partitionKey, partitionPathName, savedCurrentFreeSizeInBytes, currentFreeSizeInBytes,
					// la differenza potrebbe dare un valore negativo,
					// per cui -94124 come uint64_t darebbe 18446744073709457492
					// Per questo motivo ho fatto il cast a int64_t
					(int64_t)(savedCurrentFreeSizeInBytes - currentFreeSizeInBytes)
				);

				string sqlStatement = std::format(
					"update MMS_PartitionInfo set currentFreeSizeInBytes = {}, "
					"lastUpdateFreeSize = NOW() at time zone 'utc' "
					"where partitionKey = {} ",
					currentFreeSizeInBytes, partitionKey
				);
				SPDLOG_INFO("mon currentFreeSizeInBytes. addUpdatePartitionInfo, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);
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
			else
			{
				string sqlStatement = std::format(
					"insert into MMS_PartitionInfo ("
					"partitionKey, partitionPathName, currentFreeSizeInBytes, "
					"freeSpaceToLeaveInMB, lastUpdateFreeSize, enabled) values ("
					"{}, {}, {}, {}, NOW() at time zone 'utc', true)",
					partitionKey, trans.transaction->quote(partitionPathName), currentFreeSizeInBytes, freeSpaceToLeaveInMB
				);
				SPDLOG_INFO("mon currentFreeSizeInBytes. addUpdatePartitionInfo, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);
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

pair<int, uint64_t> MMSEngineDBFacade::getPartitionToBeUsedAndUpdateFreeSpace(int64_t ingestionJobKey, uint64_t fsEntrySizeInBytes)
{
	int partitionToBeUsed;
	uint64_t currentFreeSizeInBytes;
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
		{
			string sqlStatement = std::format(
				"select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo "
				"where (currentFreeSizeInBytes / 1000) - (freeSpaceToLeaveInMB * 1000) > {} / 1000 "
				"and enabled = true "
				"for update",
				fsEntrySizeInBytes
			);
			// "order by partitionKey asc limit 1 for update";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", ingestionJobKey: {}"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				ingestionJobKey, sqlStatement, trans.connection->getConnectionId(), elapsed
			);

			if (res.size() == 0)
			{
				string errorMessage = std::format(
					"No more space in MMS Partitions"
					", ingestionJobKey: {}"
					", fsEntrySizeInBytes: {}",
					ingestionJobKey, fsEntrySizeInBytes
				);
				_logger->error(__FILEREF__ + errorMessage);

				throw NoMoreSpaceInMMSPartition();
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
				SPDLOG_INFO("mon currentFreeSizeInBytes. getPartitionToBeUsedAndUpdateFreeSpace, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);

				SPDLOG_INFO(
					"Partition to be used"
					", ingestionJobKey: {}"
					", partitionToBeUsed: {}"
					", res.size: {}"
					", partitionResultSetIndexToBeUsed: {}",
					ingestionJobKey, partitionToBeUsed, res.size(), partitionResultSetIndexToBeUsed
				);
			}
		}

		uint64_t newCurrentFreeSizeInBytes = currentFreeSizeInBytes - fsEntrySizeInBytes;
		// SPDLOG_INFO("TEST currentFreeSizeInBytes: {}, fsEntrySizeInBytes: {}, newCurrentFreeSizeInBytes: {}",
		// 	currentFreeSizeInBytes, fsEntrySizeInBytes, newCurrentFreeSizeInBytes);

		{
			string sqlStatement = std::format(
				"update MMS_PartitionInfo set currentFreeSizeInBytes = {}, "
				"lastUpdateFreeSize = NOW() at time zone 'utc' "
				"where partitionKey = {} ",
				newCurrentFreeSizeInBytes, partitionToBeUsed
			);
			SPDLOG_INFO(
				"mon currentFreeSizeInBytes. getPartitionToBeUsedAndUpdateFreeSpace, newCurrentFreeSizeInBytes: {}", newCurrentFreeSizeInBytes
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.transaction->exec0(sqlStatement);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", ingestionJobKey: {}"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				ingestionJobKey, sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		return make_pair(partitionToBeUsed, newCurrentFreeSizeInBytes);
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

uint64_t MMSEngineDBFacade::updatePartitionBecauseOfDeletion(int partitionKey, uint64_t fsEntrySizeInBytes)
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
		uint64_t currentFreeSizeInBytes;

		{
			string sqlStatement = std::format(
				"select currentFreeSizeInBytes from MMS_PartitionInfo "
				"where partitionKey = {} for update",
				partitionKey
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
				currentFreeSizeInBytes = res[0]["currentFreeSizeInBytes"].as<uint64_t>();
			else
			{
				string errorMessage = string("Partition not found") + ", partitionKey: " + to_string(partitionKey);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			SPDLOG_INFO("mon currentFreeSizeInBytes. updatePartitionBecauseOfDeletion, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);
		}

		uint64_t newCurrentFreeSizeInBytes = currentFreeSizeInBytes + fsEntrySizeInBytes;
		// 2024-01-06: sembra che newCurrentFreeSizeInBytes ad un certo punto contiene un valore basso
		//	Questo valore viene salvato nel DB e, essendo basso, la prossima ingestion in MMS che viene fatta,
		//	indica che non abbiamo spazio nella partizione.
		//	Per cercare di capire di piu, ho aggiunto di seguito un log ed un controllo!!!
		SPDLOG_INFO(
			"updatePartitionBecauseOfDeletion"
			", currentFreeSizeInBytes: {}"
			", fsEntrySizeInBytes: {}"
			", newCurrentFreeSizeInBytes: {}",
			currentFreeSizeInBytes, fsEntrySizeInBytes, newCurrentFreeSizeInBytes
		);

		{
			string sqlStatement = std::format(
				"update MMS_PartitionInfo set currentFreeSizeInBytes = {} "
				"where partitionKey = {} ",
				newCurrentFreeSizeInBytes, partitionKey
			);
			SPDLOG_INFO("mon currentFreeSizeInBytes. updatePartitionBecauseOfDeletion, newCurrentFreeSizeInBytes: {}", newCurrentFreeSizeInBytes);
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

		return newCurrentFreeSizeInBytes;
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

fs::path MMSEngineDBFacade::getPartitionPathName(int partitionKey)
{
	fs::path partitionPathName;
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select partitionPathName from MMS_PartitionInfo "
				"where partitionKey = {} ",
				partitionKey
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
				partitionPathName = res[0]["partitionPathName"].as<string>();
			else
			{
				string errorMessage = string("No partitionInfo found") + ", partitionKey: " + to_string(partitionKey);
				_logger->error(__FILEREF__ + errorMessage);

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

	return partitionPathName;
}

void MMSEngineDBFacade::getPartitionsInfo(vector<pair<int, uint64_t>> &partitionsInfo)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_slavePostgresConnectionPool, false);
	try
	{
		partitionsInfo.clear();

		{
			string sqlStatement = std::format("select partitionKey, currentFreeSizeInBytes from MMS_PartitionInfo ");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				int partitionKey = row["partitionKey"].as<int>();
				uint64_t currentFreeSizeInBytes = row["currentFreeSizeInBytes"].as<uint64_t>();
				SPDLOG_INFO("mon currentFreeSizeInBytes. getPartitionsInfo, currentFreeSizeInBytes: {}", currentFreeSizeInBytes);

				partitionsInfo.push_back(make_pair(partitionKey, currentFreeSizeInBytes));
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
