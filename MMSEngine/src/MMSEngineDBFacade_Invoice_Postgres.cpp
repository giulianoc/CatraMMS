
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"

int64_t MMSEngineDBFacade::addInvoice(int64_t userKey, string description, int amount, string expirationDate)
{

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		string sqlStatement = std::format(
			"insert into MMS_Invoice_(userKey, creationDate, description, amount, "
			"expirationDate, paid, paymentDate) values ("
			"{}, now() at time zone 'utc', {}, {}, {}, false, null) returning invoiceKey",
			userKey, trans.transaction->quote(description), amount, trans.transaction->quote(expirationDate)
		);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		int64_t invoiceKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
		long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
		SQLQUERYLOG(
			"default", elapsed,
			"SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, trans.connection->getConnectionId(), elapsed
		);

		// return invoiceRoot;
		return invoiceKey;
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
			sqlWhere = std::format("where userKey = {} ", userKey);

		json responseRoot;
		{
			string sqlStatement = std::format("select count(*) from MMS_Invoice {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			int64_t count = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
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
			string sqlStatement = std::format(
				"select invoiceKey, userKey, description, amount, paid, paymentDate, "
				"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as formattedCreationDate, "
				"to_char(expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SSZ') as expirationDate "
				"from MMS_Invoice {} "
				"{} "
				"limit {} offset {}",
				sqlWhere, orderBy, rows, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
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

		field = "invoices";
		responseRoot[field] = invoicesRoot;

		field = "response";
		invoiceListRoot[field] = responseRoot;
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

	return invoiceListRoot;
}
