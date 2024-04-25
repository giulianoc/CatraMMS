
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

int64_t MMSEngineDBFacade::addInvoice(int64_t userKey, string description, int amount, string expirationDate)
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
		string sqlStatement = fmt::format(
			"insert into MMS_Invoice_(userKey, creationDate, description, amount, "
			"expirationDate, paid, paymentDate) values ("
			"{}, now() at time zone 'utc', {}, {}, {}, false, null) returning invoiceKey",
			userKey, trans.quote(description), amount, trans.quote(expirationDate)
		);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		int64_t invoiceKey = trans.exec1(sqlStatement)[0].as<int64_t>();
		SPDLOG_INFO(
			"SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
		);

		/*
		json invoiceRoot;
		{
			string field = "invoiceKey";
			invoiceRoot[field] = invoiceKey;

			field = "userKey";
			invoiceRoot[field] = userKey;

			field = "description";
			invoiceRoot[field] = description;

			field = "amount";
			invoiceRoot[field] = amount;

			field = "expirationDate";
			invoiceRoot[field] = expirationDate;

			field = "paid";
			invoiceRoot[field] = false;

			field = "paymentDate";
			invoiceRoot[field] = nullptr;
		}
		*/

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		// return invoiceRoot;
		return invoiceKey;
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

json MMSEngineDBFacade::getInvoicesList(int64_t userKey, bool admin, int start, int rows)
{
	SPDLOG_INFO(
		"getInvoicesList"
		", userKey: {}"
		", admin: {}"
		", start: {}"
		", rows: {}",
		userKey, admin, start, rows
	);

	json invoiceListRoot;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

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

			{
				field = "userKey";
				requestParametersRoot[field] = userKey;
			}

			{
				field = "admin";
				requestParametersRoot[field] = admin;
			}

			{
				field = "start";
				requestParametersRoot[field] = start;
			}

			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}

			field = "requestParameters";
			invoiceListRoot[field] = requestParametersRoot;
		}

		string sqlWhere;
		if (!admin)
			sqlWhere = fmt::format("where userKey = {} ", userKey);

		json responseRoot;
		{
			string sqlStatement = fmt::format("select count(*) from MMS_Invoice {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			field = "numFound";
			responseRoot[field] = count;
		}

		json invoicesRoot = json::array();
		{
			string orderBy;
			if (admin)
				orderBy = "order by creationDate desc";
			else
				orderBy = "order by paid asc, creationDate desc";
			string sqlStatement = fmt::format(
				"select invoiceKey, userKey, description, amount, paid, paymentDate, "
				"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as formattedCreationDate, "
				"to_char(expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as expirationDate "
				"from MMS_Invoice {} "
				"{} "
				"limit {} offset {}",
				sqlWhere, orderBy, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row : res)
			{
				json invoiceRoot;

				field = "invoiceKey";
				invoiceRoot[field] = row["invoiceKey"].as<int64_t>();

				field = "userKey";
				invoiceRoot[field] = row["userKey"].as<int64_t>();

				field = "creationDate";
				invoiceRoot[field] = row["formattedCreationDate"].as<string>();

				field = "description";
				invoiceRoot[field] = row["description"].as<string>();

				field = "amount";
				invoiceRoot[field] = row["amount"].as<int64_t>();

				field = "expirationDate";
				invoiceRoot[field] = row["expirationDate"].as<string>();

				field = "paid";
				invoiceRoot[field] = row["paid"].as<bool>();

				field = "paymentDate";
				if (row["paymentDate"].is_null())
					invoiceRoot[field] = nullptr;
				else
					invoiceRoot[field] = row["paymentDate"].as<string>();

				invoicesRoot.push_back(invoiceRoot);
			}
			SPDLOG_INFO(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		field = "invoices";
		responseRoot[field] = invoicesRoot;

		field = "response";
		invoiceListRoot[field] = responseRoot;

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

	return invoiceListRoot;
}
