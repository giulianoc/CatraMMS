
#include <fstream>
#include <sstream>
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"


void MMSEngineDBFacade::createTablesIfNeeded_Postgres()
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
			// 2023-10-16: la partizione dovrebbe essere fatta da (workspaceKey, requestTimestamp) ma
			//	- poichè la sua gestione si complica un po
			//	- attualmente abbiamo un solo cliente che utilizza le statistiche
			// lasciamo solamente la partizione su requestTimestamp
			string sqlStatement =
				"create table if not exists MMS_RequestStatistic ("
					"requestStatisticKey		bigserial, "
					"workspaceKey				smallint not null, "
					"ipAddress					text null, "
					"userId						text not null, "
					"physicalPathKey			bigint null, "
					"confStreamKey				bigint null, "
					"title						text not null, "
					"requestTimestamp			timestamp without time zone not null, "
					"upToNextRequestInSeconds	integer null, "
					"constraint MMS_RequestStatistic_PK PRIMARY KEY (requestStatisticKey, requestTimestamp)) "
					"partition by range (requestTimestamp) "
			;
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
			// usato dal partitioning
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx1 on MMS_RequestStatistic ("
				"requestTimestamp)";
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
			// 2023-03-18: aggiunto per le performance del metodo MMSEngineDBFacade::addRequestStatistic,
			//	in particolare per velocizzare
			//		select max(requestStatisticKey) from MMS_RequestStatistic
			//		where workspaceKey = ? and requestStatisticKey < ? and userId = ?
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx2 on MMS_RequestStatistic ("
				"workspaceKey, userId, requestStatisticKey)";
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

		/*
		{
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx1 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, title, userId, upToNextRequestInSeconds)";
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
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx2 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, userId, title, upToNextRequestInSeconds)";
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
			string sqlStatement =
				"create index if not exists MMS_RequestStatistic_idx3 on MMS_RequestStatistic ("
				"workspaceKey, requestTimestamp, upToNextRequestInSeconds)";
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
		*/

		// per creare la partition
		retentionOfStatisticData();

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

