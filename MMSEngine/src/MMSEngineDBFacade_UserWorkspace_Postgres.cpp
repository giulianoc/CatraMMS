
#include <random>
#include "catralibraries/Encrypt.h"
#include "catralibraries/StringUtils.h"
#include "MMSEngineDBFacade.h"
#include "JSONUtils.h"

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(int64_t workspaceKey)
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
		string sqlStatement = fmt::format(
			"select w.workspaceKey, w.name, w.directoryName, w.maxEncodingPriority, "
			"wc.maxStorageInGB, wc.currentCostForStorage, "
			"wc.dedicatedEncoder_power_1, wc.currentCostForDedicatedEncoder_power_1, "
			"wc.dedicatedEncoder_power_2, wc.currentCostForDedicatedEncoder_power_2, "
			"wc.dedicatedEncoder_power_3, wc.currentCostForDedicatedEncoder_power_3, "
			"wc.CDN_type_1, wc.currentCostForCDN_type_1, "
			"wc.support_type_1, wc.currentCostForSupport_type_1 "
			"from MMS_Workspace w, MMS_WorkspaceCost wc "
			"where w.workspaceKey = wc.workspaceKey and w.workspaceKey = {}",
			workspaceKey);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans.exec(sqlStatement);
		SPDLOG_INFO("SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, conn->getConnectionId(),
			chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
		);

		shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
		if (!empty(res))
		{
			workspace->_workspaceKey = res[0]["workspaceKey"].as<int64_t>();
			workspace->_name = res[0]["name"].as<string>();
			workspace->_directoryName = res[0]["directoryName"].as<string>();
			workspace->_maxEncodingPriority = static_cast<int>(toEncodingPriority(res[0]["maxEncodingPriority"].as<string>()));

			workspace->_maxStorageInGB = res[0]["maxStorageInGB"].as<int>();
			workspace->_currentCostForStorage = res[0]["currentCostForStorage"].as<int>();
			workspace->_dedicatedEncoder_power_1 = res[0]["dedicatedEncoder_power_1"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_1 = res[0]["currentCostForDedicatedEncoder_power_1"].as<int>();
			workspace->_dedicatedEncoder_power_2 = res[0]["dedicatedEncoder_power_2"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_2 = res[0]["currentCostForDedicatedEncoder_power_2"].as<int>();
			workspace->_dedicatedEncoder_power_3 = res[0]["dedicatedEncoder_power_3"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_3 = res[0]["currentCostForDedicatedEncoder_power_3"].as<int>();
			workspace->_CDN_type_1 = res[0]["CDN_type_1"].as<int>();
			workspace->_currentCostForCDN_type_1 = res[0]["currentCostForCDN_type_1"].as<int>();
			workspace->_support_type_1 = res[0]["support_type_1"].as<bool>();
			workspace->_currentCostForSupport_type_1 = res[0]["currentCostForSupport_type_1"].as<int>();

			// getTerritories(workspace);
		}
		else
		{
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
			conn = nullptr;

			string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceKey: " + to_string(workspaceKey)
                + ", sqlStatement: " + sqlStatement
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);                    
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    
		return workspace;
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

shared_ptr<Workspace> MMSEngineDBFacade::getWorkspace(string workspaceName)
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
		string sqlStatement = fmt::format(
			"select w.workspaceKey, w.name, w.directoryName, w.maxEncodingPriority "
			"wc.maxStorageInGB, wc.currentCostForStorage, "
			"wc.dedicatedEncoder_power_1, wc.currentCostForDedicatedEncoder_power_1, "
			"wc.dedicatedEncoder_power_2, wc.currentCostForDedicatedEncoder_power_2, "
			"wc.dedicatedEncoder_power_3, wc.currentCostForDedicatedEncoder_power_3, "
			"wc.CDN_type_1, wc.currentCostForCDN_type_1, "
			"wc.support_type_1, wc.currentCostForSupport_type_1 "
			"from MMS_Workspace w, MMS_WorkspaceCost wc "
			"where w.workspaceKey = wc.workspaceKey and w.name = {}",
			trans.quote(workspaceName));
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans.exec(sqlStatement);
		SPDLOG_INFO("SQL statement"
			", sqlStatement: @{}@"
			", getConnectionId: @{}@"
			", elapsed (millisecs): @{}@",
			sqlStatement, conn->getConnectionId(),
			chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
		);

		shared_ptr<Workspace>    workspace = make_shared<Workspace>();
    
		if (!empty(res))
		{
			workspace->_workspaceKey = res[0]["workspaceKey"].as<int64_t>();
			workspace->_name = res[0]["name"].as<string>();
			workspace->_directoryName = res[0]["directoryName"].as<string>();
			workspace->_maxEncodingPriority = static_cast<int>(toEncodingPriority(
				res[0]["maxEncodingPriority"].as<string>()));

			workspace->_maxStorageInGB = res[0]["maxStorageInGB"].as<int>();
			workspace->_currentCostForStorage = res[0]["currentCostForStorage"].as<int>();
			workspace->_dedicatedEncoder_power_1 = res[0]["dedicatedEncoder_power_1"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_1 = res[0]["currentCostForDedicatedEncoder_power_1"].as<int>();
			workspace->_dedicatedEncoder_power_2 = res[0]["dedicatedEncoder_power_2"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_2 = res[0]["currentCostForDedicatedEncoder_power_2"].as<int>();
			workspace->_dedicatedEncoder_power_3 = res[0]["dedicatedEncoder_power_3"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_3 = res[0]["currentCostForDedicatedEncoder_power_3"].as<int>();
			workspace->_CDN_type_1 = res[0]["CDN_type_1"].as<int>();
			workspace->_currentCostForCDN_type_1 = res[0]["currentCostForCDN_type_1"].as<int>();
			workspace->_support_type_1 = res[0]["support_type_1"].as<bool>();
			workspace->_currentCostForSupport_type_1 = res[0]["currentCostForSupport_type_1"].as<int>();

			// getTerritories(workspace);
		}
		else
		{
			_logger->debug(__FILEREF__ + "DB connection unborrow"
				+ ", getConnectionId: " + to_string(conn->getConnectionId())
			);
			connectionPool->unborrow(conn);
			conn = nullptr;

			string errorMessage = __FILEREF__ + "select failed"
                + ", workspaceName: " + workspaceName
                + ", sqlStatement: " + sqlStatement
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);                    
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;
    
		return workspace;
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

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUserAndAddWorkspace(
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    string userTimezone,
    string workspaceName,
    WorkspaceType workspaceType,
    string deliveryURL,
    EncodingPriority maxEncodingPriority,
    EncodingPeriod encodingPeriod,
    long maxIngestionsNumber,
    long maxStorageInMB,
    string languageCode,
    chrono::system_clock::time_point userExpirationLocalDate
)
{
    int64_t         workspaceKey;
    int64_t         userKey;
    string          userRegistrationCode;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
		string trimUserName = StringUtils::trim(userName);
		if (trimUserName == "")
		{
			string errorMessage = string("userName is not well formed.")                             
				+ ", userName: " + userName                                                     
			;                                                                                             
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (!isTimezoneValid(userTimezone))
			userTimezone = "CET";

        {
			// This method is called only in case of MMS user (no ldapEnabled)
			char        strExpirationUtcDate [64];
            {
                tm          tmDateTime;
                time_t utcTime = chrono::system_clock::to_time_t(userExpirationLocalDate);
                
                gmtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);
            }
            string sqlStatement = fmt::format(
                "insert into MMS_User (name, eMailAddress, password, country, timezone, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, {}, NOW() at time zone 'utc', false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"NULL) returning userKey",
				trans.quote(trimUserName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry), trans.quote(userTimezone),
				trans.quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			userKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        {
			string trimWorkspaceName = StringUtils::trim(workspaceName);
			if (trimWorkspaceName == "")
			{
				string errorMessage = string("WorkspaceName is not well formed.")
					+ ", workspaceName: " + workspaceName
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}

			// defaults when a new user is added/registered
            bool admin = false;
            bool createRemoveWorkspace = true;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            bool editConfiguration = true;
            bool killEncoding = true;
            bool cancelIngestionJob = true;
            bool editEncodersPool = true;
            bool applicationRecorder = true;

            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
					trans,
                    userKey,
                    admin,
					createRemoveWorkspace,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
					editConfiguration,
					killEncoding,
					cancelIngestionJob,
					editEncodersPool,
					applicationRecorder,
                    trimWorkspaceName,
                    workspaceType,
                    deliveryURL,
                    maxEncodingPriority,
                    encodingPeriod,
                    maxIngestionsNumber,
                    maxStorageInMB,
                    languageCode,
                    userExpirationLocalDate);

			workspaceKey = workspaceKeyAndConfirmationCode.first;
			userRegistrationCode = workspaceKeyAndConfirmationCode.second;
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
    
	tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
		make_tuple(workspaceKey, userKey, userRegistrationCode);

	return workspaceKeyUserKeyAndConfirmationCode;
}

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerUserAndShareWorkspace(
    string userName,
    string userEmailAddress,
    string userPassword,
    string userCountry,
    string userTimezone,
    string shareWorkspaceCode,
    chrono::system_clock::time_point userExpirationLocalDate
)
{
    int64_t         workspaceKey;
    int64_t         userKey;
    string          userRegistrationCode;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
		string trimUserName = StringUtils::trim(userName);
		if (trimUserName == "")
		{
			string errorMessage = string("userName is not well formed.")                             
				+ ", userName: " + userName                                                     
			;                                                                                             
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		if (!isTimezoneValid(userTimezone))
			userTimezone = "CET";

        {
			// This method is called only in case of MMS user (no ldapEnabled)
			char        strExpirationUtcDate [64];
            {
                tm          tmDateTime;
                time_t utcTime = chrono::system_clock::to_time_t(userExpirationLocalDate);
                
                gmtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);
            }
            string sqlStatement = fmt::format( 
                "insert into MMS_User (name, eMailAddress, password, country, timezone, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, {}, NOW() at time zone 'utc', "
				"false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), NULL) returning userKey",
				trans.quote(trimUserName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry), trans.quote(userTimezone),
				trans.quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			userKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		{
			// this is a registration of a user because of a share workspace
			{
				string sqlStatement = fmt::format( 
					"select workspaceKey from MMS_Code "
					"where code = {} and userEmail = {} and type = {}",
					trans.quote(shareWorkspaceCode), trans.quote(userEmailAddress),
					trans.quote(toString(CodeType::ShareWorkspace)));
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
					workspaceKey = res[0]["workspaceKey"].as<int64_t>();
				else
				{
					string errorMessage = __FILEREF__ + "Share Workspace Code not found"
						+ ", shareWorkspaceCode: " + shareWorkspaceCode
						+ ", userEmailAddress: " + userEmailAddress
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_Code set userKey = {}, type = {} "
					"where code = {} returning 1) select count(*) from rows",
					userKey, trans.quote(toString(CodeType::UserRegistrationComingFromShareWorkspace)),
					trans.quote(shareWorkspaceCode));
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
					string errorMessage = __FILEREF__ + "Code update failed"
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
						+ ", shareWorkspaceCode: " + shareWorkspaceCode
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			userRegistrationCode = shareWorkspaceCode;
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
    
	tuple<int64_t,int64_t,string> workspaceKeyUserKeyAndConfirmationCode = 
		make_tuple(workspaceKey, userKey, userRegistrationCode);

	return workspaceKeyUserKeyAndConfirmationCode;
}

pair<int64_t,string> MMSEngineDBFacade::createWorkspace(
    int64_t userKey,
    string workspaceName,
    WorkspaceType workspaceType,
    string deliveryURL,
    EncodingPriority maxEncodingPriority,
    EncodingPeriod encodingPeriod,
    long maxIngestionsNumber,
    long maxStorageInMB,
    string languageCode,
	bool admin,
    chrono::system_clock::time_point userExpirationLocalDate
)
{
    int64_t         workspaceKey;
    string          confirmationCode;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
		string trimWorkspaceName = StringUtils::trim(workspaceName);
		if (trimWorkspaceName == "")
		{
			string errorMessage = string("WorkspaceName is not well formed.")
				+ ", workspaceName: " + workspaceName
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

        {
            bool createRemoveWorkspace = true;
            bool ingestWorkflow = true;
            bool createProfiles = true;
            bool deliveryAuthorization = true;
            bool shareWorkspace = true;
            bool editMedia = true;
            bool editConfiguration = true;
            bool killEncoding = true;
            bool cancelIngestionJob = true;
			bool editEncodersPool = true;
			bool applicationRecorder = true;

            pair<int64_t,string> workspaceKeyAndConfirmationCode =
                addWorkspace(
                    conn,
					trans,
                    userKey,
                    admin,
                    createRemoveWorkspace,
                    ingestWorkflow,
                    createProfiles,
                    deliveryAuthorization,
                    shareWorkspace,
                    editMedia,
					editConfiguration,
					killEncoding,
					cancelIngestionJob,
					editEncodersPool,
					applicationRecorder,
                    trimWorkspaceName,
                    workspaceType,
                    deliveryURL,
                    maxEncodingPriority,
                    encodingPeriod,
                    maxIngestionsNumber,
                    maxStorageInMB,
                    languageCode,
                    userExpirationLocalDate);
            
            workspaceKey = workspaceKeyAndConfirmationCode.first;
            confirmationCode = workspaceKeyAndConfirmationCode.second;
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
    
    pair<int64_t,string> workspaceKeyAndConfirmationCode = 
            make_pair(workspaceKey, confirmationCode);
    
    return workspaceKeyAndConfirmationCode;
}

string MMSEngineDBFacade::createCode(
    int64_t workspaceKey,
    int64_t userKey, string userEmail,
	CodeType codeType,
    bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder
)
{
	string		code;

	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		code = createCode(conn, &trans, workspaceKey, userKey, userEmail, codeType,
			admin, createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool, applicationRecorder);

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

    return code;
}

string MMSEngineDBFacade::createCode(
	shared_ptr<PostgresConnection> conn,
	transaction_base* trans,
    int64_t workspaceKey,
    int64_t userKey, string userEmail,
	CodeType codeType,
    bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder
)
{
	string		code;

    try
    {
		unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
		default_random_engine e(seed);
		code = to_string(e());

        {
			Json::Value permissionsRoot;
            {
				permissionsRoot["admin"] = admin;

				permissionsRoot["createRemoveWorkspace"] = createRemoveWorkspace;
				permissionsRoot["ingestWorkflow"] = ingestWorkflow;
				permissionsRoot["createProfiles"] = createProfiles;
				permissionsRoot["deliveryAuthorization"] = deliveryAuthorization;
				permissionsRoot["shareWorkspace"] = shareWorkspace;
				permissionsRoot["editMedia"] = editMedia;
				permissionsRoot["editConfiguration"] = editConfiguration;
				permissionsRoot["killEncoding"] = killEncoding;
				permissionsRoot["cancelIngestionJob"] = cancelIngestionJob;
				permissionsRoot["editEncodersPool"] = editEncodersPool;
				permissionsRoot["applicationRecorder"] = applicationRecorder;
			}
			string permissions = JSONUtils::toString(permissionsRoot);

			try
			{
				string sqlStatement = fmt::format( 
					"insert into MMS_Code (code, workspaceKey, userKey, userEmail, "
					"type, permissions, creationDate) values ("
					"{}, {}, {}, {}, {}, {}, NOW() at time zone 'utc')",
					trans->quote(code), workspaceKey,
					userKey == -1 ? "null" : to_string(userKey),
					userEmail == "" ? "null" : trans->quote(userEmail),
					trans->quote(toString(codeType)), trans->quote(permissions));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				trans->exec0(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}
			catch(sql::SQLException& se)
			{
				string exceptionMessage(se.what());

				throw se;
			}
        }
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return code;
}

pair<int64_t,string> MMSEngineDBFacade::registerActiveDirectoryUser(
    string userName,
    string userEmailAddress,
    string userCountry,
    string userTimezone,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder,
	string defaultWorkspaceKeys, int expirationInDaysWorkspaceDefaultValue,
    chrono::system_clock::time_point userExpirationLocalDate
)
{
	int64_t userKey;
	string apiKey;

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
			string userPassword = "";

			if (!isTimezoneValid(userTimezone))
				userTimezone = "CET";

			char        strExpirationUtcDate [64];
            {
                tm          tmDateTime;
                time_t utcTime = chrono::system_clock::to_time_t(userExpirationLocalDate);
                
                gmtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);
            }
            string sqlStatement = fmt::format( 
                "insert into MMS_User (name, eMailAddress, password, country, timezone, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, {}, NOW() at time zone 'utc', false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"NULL) returning userKey",
				trans.quote(userName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry), trans.quote(userTimezone),
				trans.quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			userKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
		// create the API of the user for each existing Workspace
        {
			_logger->info(__FILEREF__ + "Creating API for the default workspaces"
				+ ", defaultWorkspaceKeys: " + defaultWorkspaceKeys
			);
			stringstream ssDefaultWorkspaceKeys(defaultWorkspaceKeys);                                                                                  
			string defaultWorkspaceKey;
			char separator = ',';
			while (getline(ssDefaultWorkspaceKeys, defaultWorkspaceKey, separator))
			{
				if (!defaultWorkspaceKey.empty())
				{
					int64_t llDefaultWorkspaceKey = stoll(defaultWorkspaceKey);

					_logger->info(__FILEREF__ + "Creating API for the default workspace"
						+ ", llDefaultWorkspaceKey: " + to_string(llDefaultWorkspaceKey)
					);
					string localApiKey = createAPIKeyForActiveDirectoryUser(
						conn,
						&trans,
						userKey,
						userEmailAddress,
						createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
						shareWorkspace, editMedia,
						editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
						applicationRecorder,
						llDefaultWorkspaceKey, expirationInDaysWorkspaceDefaultValue);
					if (apiKey.empty())
						apiKey = localApiKey;
				}
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

    
    return make_pair(userKey, apiKey);
}

string MMSEngineDBFacade::createAPIKeyForActiveDirectoryUser(
    int64_t userKey,
    string userEmailAddress,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
	string apiKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		apiKey = createAPIKeyForActiveDirectoryUser(
			conn,
			&trans,
			userKey,
			userEmailAddress,
			createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
			applicationRecorder,
			workspaceKey, expirationInDaysWorkspaceDefaultValue);

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
    catch(APIKeyNotFoundOrExpired& e)
	{
		SPDLOG_ERROR("APIKeyNotFoundOrExpired, SQL exception"
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

	return apiKey;
}

string MMSEngineDBFacade::createAPIKeyForActiveDirectoryUser(
	shared_ptr<PostgresConnection> conn,
	transaction_base* trans,
    int64_t userKey,
    string userEmailAddress,
    bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
	bool shareWorkspace, bool editMedia,
	bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
	bool applicationRecorder,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
	string apiKey;
    
    try
    {
		// create the API of the user for each existing Workspace
        {
			Json::Value permissionsRoot;
            {
                bool admin = false;

				permissionsRoot["admin"] = admin;

				permissionsRoot["createRemoveWorkspace"] = createRemoveWorkspace;
				permissionsRoot["ingestWorkflow"] = ingestWorkflow;
				permissionsRoot["createProfiles"] = createProfiles;
				permissionsRoot["deliveryAuthorization"] = deliveryAuthorization;
				permissionsRoot["shareWorkspace"] = shareWorkspace;
				permissionsRoot["editMedia"] = editMedia;
				permissionsRoot["editConfiguration"] = editConfiguration;
				permissionsRoot["killEncoding"] = killEncoding;
				permissionsRoot["cancelIngestionJob"] = cancelIngestionJob;
				permissionsRoot["editEncodersPool"] = editEncodersPool;
				permissionsRoot["applicationRecorder"] = applicationRecorder;
            }
			string permissions = JSONUtils::toString(permissionsRoot);

            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
			default_random_engine e(seed);

			string sourceApiKey = userEmailAddress + "__SEP__" + to_string(e());
			apiKey = Encrypt::opensslEncrypt(sourceApiKey);

			bool isOwner = false;
			bool isDefault = false;
          
			char        strExpirationUtcDate [64];
			{
				chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now()
					+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

				tm          tmDateTime;
				time_t utcTime = chrono::system_clock::to_time_t(apiKeyExpirationDate);

				gmtime_r (&utcTime, &tmDateTime);

				sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);
			}
			string sqlStatement = fmt::format(
				"insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
				"{}, {}, {}, {}, {}, {}, NOW() at time zone 'utc', "
				"to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'))",
				trans->quote(apiKey), userKey, workspaceKey, isOwner, isDefault,
				trans->quote(permissions), trans->quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			addWorkspaceForAdminUsers(conn, trans,
				workspaceKey, expirationInDaysWorkspaceDefaultValue);
        }
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    
    return apiKey;
}

pair<int64_t,string> MMSEngineDBFacade::addWorkspace(
        shared_ptr<PostgresConnection> conn,
		work& trans,
        int64_t userKey,
        bool admin, bool createRemoveWorkspace, bool ingestWorkflow, bool createProfiles, bool deliveryAuthorization,
        bool shareWorkspace, bool editMedia,
        bool editConfiguration, bool killEncoding, bool cancelIngestionJob, bool editEncodersPool,
		bool applicationRecorder,
        string workspaceName,
        WorkspaceType workspaceType,
        string deliveryURL,
        EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
        long maxIngestionsNumber,
        long maxStorageInMB,
        string languageCode,
        chrono::system_clock::time_point userExpirationLocalDate
)
{
    int64_t         workspaceKey;
    string          confirmationCode;
	Json::Value		workspaceRoot;
    
    try
    {
        {
            bool enabled = false;
			string workspaceDirectoryName = "tempName";

			string sqlStatement = fmt::format( 
				"insert into MMS_Workspace ("
				"creationDate,              name, directoryName, workspaceType, "
				"deliveryURL, enabled, maxEncodingPriority, encodingPeriod, "
				"maxIngestionsNumber, languageCode) values ("
				"NOW() at time zone 'utc',  {},   {},            {}, "
				"{},          {},     {},                   {}, "
				"{},                  {}) returning workspaceKey",
				trans.quote(workspaceName), trans.quote(workspaceDirectoryName),
				static_cast<int>(workspaceType),
				deliveryURL == "" ? "null" : trans.quote(deliveryURL),
				enabled, trans.quote(toString(maxEncodingPriority)),
				trans.quote(toString(encodingPeriod)), maxIngestionsNumber,
				trans.quote(languageCode));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			workspaceKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

		{
			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_Workspace set directoryName = {} where workspaceKey = {} "
				"returning 1) select count(*) from rows",
				trans.quote(to_string(workspaceKey)), workspaceKey);
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
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
			}
		}

		{
			string sqlStatement = fmt::format(
				"insert into MMS_WorkspaceCost ("
					"workspaceKey, maxStorageInGB, currentCostForStorage, "
					"dedicatedEncoder_power_1, currentCostForDedicatedEncoder_power_1, "
					"dedicatedEncoder_power_2, currentCostForDedicatedEncoder_power_2, "
					"dedicatedEncoder_power_3, currentCostForDedicatedEncoder_power_3, "
					"CDN_type_1, currentCostForCDN_type_1, "
					"support_type_1, currentCostForSupport_type_1 "
					") values ("
					"{},           {},             0, "
					"0,                        0, "
					"0,                        0, "
					"0,                        0, "
					"0,          0, "
					"false,          0) ",
					workspaceKey, maxStorageInMB / 1000);
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

		confirmationCode = MMSEngineDBFacade::createCode(
			conn,
			&trans,
			workspaceKey,
			userKey, "",	// userEmail,
			CodeType::UserRegistration,
			admin, createRemoveWorkspace, ingestWorkflow, createProfiles, deliveryAuthorization,
			shareWorkspace, editMedia,
			editConfiguration, killEncoding, cancelIngestionJob, editEncodersPool,
			applicationRecorder);

        {
            string sqlStatement = fmt::format( 
                    "insert into MMS_WorkspaceMoreInfo (workspaceKey, currentDirLevel1, "
					"currentDirLevel2, currentDirLevel3, startDateTime, endDateTime, "
					"currentIngestionsNumber) values ("
                    "{}, 0, 0, 0, NOW() at time zone 'utc', NOW() at time zone 'utc', 0)",
					workspaceKey);
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

		// insert EncodersPool with label null.
		// This is used for the default encoders pool for the actual workspace
		// The default encoders will be all the internal encoders associated to the workspace
		// These encoders will not be saved in MMS_EncoderEncodersPoolMapping but they
		// will be retrieved directly by MMS_EncoderWorkspaceMapping
        {
            string sqlStatement = fmt::format( 
                "insert into MMS_EncodersPool(workspaceKey, label, lastEncoderIndexUsed) "
				"values ({}, NULL, 0)",
				workspaceKey);
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
        int64_t territoryKey = addTerritory(
                conn,
                workspaceKey,
                _defaultTerritoryName);
        */        
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    pair<int64_t,string> workspaceKeyAndConfirmationCode = 
            make_pair(workspaceKey, confirmationCode);

    return workspaceKeyAndConfirmationCode;
}

tuple<string,string,string> MMSEngineDBFacade::confirmRegistration(
    string confirmationCode, int expirationInDaysWorkspaceDefaultValue
)
{
    string      apiKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        int64_t     userKey;
        string		permissions;
        int64_t     workspaceKey;
		CodeType codeType;
        {
            string sqlStatement = fmt::format( 
                "select userKey, permissions, workspaceKey, type "
				"from MMS_Code "
				"where code = {} and type in ({}, {}) and "
				"creationDate + INTERVAL '{} DAY' >= NOW() at time zone 'utc'",
				trans.quote(confirmationCode), trans.quote(toString(CodeType::UserRegistration)),
				trans.quote(toString(CodeType::UserRegistrationComingFromShareWorkspace)),
				_confirmationCodeRetentionInDays);
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
				userKey = res[0]["userKey"].as<int64_t>();
				permissions = res[0]["permissions"].as<string>();
                workspaceKey = res[0]["workspaceKey"].as<int64_t>();
				codeType = toCodeType(res[0]["type"].as<string>());
            }
            else
            {
                string errorMessage = __FILEREF__ + "Confirmation Code not found or expired"
                    + ", confirmationCode: " + confirmationCode
					+ ", type: " + toString(codeType)
                    + ", _confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
		}

        // check if the apiKey is already present (maybe this is the second time the confirmRegistration API is called
        bool apiKeyAlreadyPresent = false;
        {
            string sqlStatement = fmt::format( 
                "select apiKey from MMS_APIKey where userKey = {} and workspaceKey = {}",
				userKey, workspaceKey);
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
                apiKey = res[0]["apiKey"].as<string>();
                apiKeyAlreadyPresent = true;
            }
        }

        if (!apiKeyAlreadyPresent)
        {
			// questa condizione fa si che solo nel caso di UserRegistration il Workspace sia enabled,
			// mentre nel caso di UserRegistration proveniente da un ShareWorkspace (UserRegistrationComingFromShareWorkspace)
			// non bisogna fare nulla sul Workspace di cui non si è proprietario
            if (codeType == CodeType::UserRegistration)
            {
                bool enabled = true;

                string sqlStatement = fmt::format( 
                    "WITH rows AS (update MMS_Workspace set enabled = {} where workspaceKey = {} "
					"returning 1) select count(*) from rows",
					enabled, workspaceKey);
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
                            + ", enabled: " + to_string(enabled)
                            + ", workspaceKey: " + to_string(workspaceKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", sqlStatement: " + sqlStatement
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
        }

        string emailAddress;
        string name;
        {
            string sqlStatement = fmt::format( 
                "select name, eMailAddress from MMS_User where userKey = {}",
				userKey);
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
                name = res[0]["name"].as<string>();
                emailAddress = res[0]["eMailAddress"].as<string>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "User are not present"
                    + ", userKey: " + to_string(userKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        if (!apiKeyAlreadyPresent)
        {
            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
            default_random_engine e(seed);

            string sourceApiKey = emailAddress + "__SEP__" + to_string(e());
            apiKey = Encrypt::opensslEncrypt(sourceApiKey);
			_logger->info(__FILEREF__ + "Encrypt::opensslEncrypt"
				+ ", sourceApiKey: " + sourceApiKey
				+ ", apiKey: '" + apiKey + "'"
			);

            bool isOwner = codeType == CodeType::UserRegistration ? true : false;
			bool isDefault = false;
            
			char        strExpirationUtcDate [64];
            {
                chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now()
					+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

				tm tmDateTime;
				time_t utcTime = chrono::system_clock::to_time_t(apiKeyExpirationDate);

				gmtime_r (&utcTime, &tmDateTime);

				sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);
            }
            string sqlStatement = fmt::format( 
                "insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
                "                        {},      {},       {},            {},       {}, "
				"{},           NOW() at time zone 'utc',        to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'))",
				trans.quote(apiKey), userKey, workspaceKey, isOwner, isDefault,
				trans.quote(permissions), trans.quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			addWorkspaceForAdminUsers(conn, &trans,
				workspaceKey, expirationInDaysWorkspaceDefaultValue);
		}

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(apiKey, name, emailAddress);
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

void MMSEngineDBFacade::addWorkspaceForAdminUsers(
	shared_ptr<PostgresConnection> conn, transaction_base* trans,
	int64_t workspaceKey, int expirationInDaysWorkspaceDefaultValue
)
{
	string apiKey;

    try
    {
		Json::Value permissionsRoot;
		{
			bool admin = true;

			permissionsRoot["admin"] = admin;
		}
		string permissions = JSONUtils::toString(permissionsRoot);

		bool isOwner = false;
		bool isDefault = false;

		for(string adminEmailAddress: _adminEmailAddresses)
        {
			int64_t userKey;
			{
				string sqlStatement = fmt::format( 
					"select userKey from MMS_User "
					"where eMailAddress = {}",
					trans->quote(adminEmailAddress));
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans->exec(sqlStatement);
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (!empty(res))
					userKey = res[0]["userKey"].as<int64_t>();
				else
				{
					string errorMessage = __FILEREF__ + "Admin email address was not found"
						+ ", adminEmailAddress: " + adminEmailAddress
						+ ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}

			bool apiKeyAlreadyPresentForAdminUser = false;
			{
				string sqlStatement = fmt::format( 
					"select count(*) from MMS_APIKey "
					"where userKey = {} and workspaceKey = {}",
					userKey, workspaceKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				apiKeyAlreadyPresentForAdminUser = trans->exec1(sqlStatement)[0].as<int>() != 0 ? true : false;
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			if (apiKeyAlreadyPresentForAdminUser)
				continue;

			unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
			default_random_engine e(seed);

			string sourceApiKey = adminEmailAddress + "__SEP__" + to_string(e());
			apiKey = Encrypt::opensslEncrypt(sourceApiKey);

			char        strExpirationUtcDate [64];
			{
				chrono::system_clock::time_point apiKeyExpirationDate =
					chrono::system_clock::now()
					+ chrono::hours(24 * expirationInDaysWorkspaceDefaultValue);

				tm          tmDateTime;
				time_t utcTime = chrono::system_clock::to_time_t(apiKeyExpirationDate);

				gmtime_r (&utcTime, &tmDateTime);

				sprintf (strExpirationUtcDate, "%04d-%02d-%02d %02d:%02d:%02d",
					tmDateTime. tm_year + 1900,
					tmDateTime. tm_mon + 1,
					tmDateTime. tm_mday,
					tmDateTime. tm_hour,
					tmDateTime. tm_min,
					tmDateTime. tm_sec);
			}
			string sqlStatement = fmt::format( 
				"insert into MMS_APIKey (apiKey, userKey, workspaceKey, isOwner, isDefault, "
				"permissions, creationDate, expirationDate) values ("
				"{}, {}, {}, {}, {}, {}, NOW() at time zone 'utc', to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'))",
				trans->quote(apiKey), userKey, workspaceKey, isOwner, isDefault,
				trans->quote(permissions), trans->quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans->exec0(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

vector<tuple<int64_t, string, string>> MMSEngineDBFacade::deleteWorkspace(
	int64_t userKey,
	int64_t workspaceKey)
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
        // check if ADMIN flag is already present
        bool admin = false;
		bool isOwner = false;
        {
            string sqlStatement = fmt::format( 
                "select isOwner, permissions "
				"from MMS_APIKey where workspaceKey = {} and userKey = {}",
				workspaceKey, userKey);
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
				string permissions = res[0]["permissions"].as<string>();
				Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);

				admin = JSONUtils::asBool(permissionsRoot, "admin", false);
                isOwner = res[0]["isOwner"].as<bool>();
            }
        }

		if (!isOwner && !admin)
		{
			string errorMessage = __FILEREF__ + "The user requesting the deletion does not have the ownership rights and the delete cannot be done"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2023-03-01: è necessario eliminare tutti gli user aventi solamente questo workspace
		//	calcoliamo questi user
		vector<tuple<int64_t, string, string>> usersToBeRemoved;
        {
            string sqlStatement = fmt::format( 
				"select u.userKey, u.name, u.eMailAddress from MMS_APIKey ak, MMS_User u "
				"where ak.userKey = u.userKey and ak.workspaceKey = {}",
				workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                int64_t userKey = row["userKey"].as<int64_t>();
                string name = row["name"].as<string>();
                string eMailAddress = row["eMailAddress"].as<string>();

				string sqlStatement = fmt::format( 
					"select count(*) from MMS_APIKey where userKey = {}",
					userKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int count = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (count == 1)
				{
					// it means the user has ONLY the workspace that will be removed
					// So the user will be removed too

					usersToBeRemoved.push_back(make_tuple(userKey, name, eMailAddress));
				}
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        {
			// in all the tables depending from Workspace we have 'on delete cascade'
			// So all should be removed automatically from DB
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_Workspace where workspaceKey = {} returning 1) select count(*) from rows",
				workspaceKey);
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

        // the users do not have any other workspace will be removed
		if (usersToBeRemoved.size() > 0)
        {
			string sUsers;
			for(tuple<int64_t, string, string> userDetails: usersToBeRemoved)
			{
				int64_t userKey;

				tie(userKey, ignore, ignore) = userDetails;

				if (sUsers == "")
					sUsers = to_string(userKey);
				else
					sUsers += (", " + to_string(userKey));
			}

			// in all the tables depending from User we have 'on delete cascade'
			// So all should be removed automatically from DB
			string sqlStatement = fmt::format( 
				"WITH rows AS (delete from MMS_User where userKey in ({}) "
				"returning 1) select count(*) from rows",
				sUsers);
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

		return usersToBeRemoved;
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

tuple<bool, string, string> MMSEngineDBFacade::unshareWorkspace(
	int64_t userKey,
	int64_t workspaceKey)
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
        // check if ADMIN flag is already present
        bool admin = false;
		bool isOwner = false;
        {
            string sqlStatement = fmt::format( 
                "select isOwner, permissions "
				"from MMS_APIKey where workspaceKey = {} and userKey = {}",
				workspaceKey, userKey);
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
				string permissions = res[0]["permissions"].as<string>();
				Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);

				admin = JSONUtils::asBool(permissionsRoot, "admin", false);
                isOwner = res[0]["isOwner"].as<bool>();
            }
        }

		if (isOwner)
		{
			string errorMessage = __FILEREF__ + "The user requesting the unshare has the ownership rights. In this case he should call removeWorkspace"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		// 2023-03-01: nel caso in cui l'utente ha solamente questo workspace, è necessario eliminarlo
		//	perchè non puo rimanere un utente senza workspace
		bool userToBeRemoved = false;
		string name;
		string eMailAddress;
		{
			{
				string sqlStatement = fmt::format( 
					"select name, eMailAddress from MMS_User "
					"where userKey = {}", userKey);
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
					string errorMessage = fmt::format("The user does not exist"
						", userKey: {}", userKey
					);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				name = res[0]["name"].as<string>();
				eMailAddress = res[0]["eMailAddress"].as<string>();
			}
			{
				string sqlStatement = fmt::format( 
					"select count(*) from MMS_APIKey where userKey = {}",
					userKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int count = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (count == 1)
				{
					// it means the user has ONLY the workspace that will be removed
					// So the user will be removed too

					userToBeRemoved = true;
				}
			}
        }

        {
            string sqlStatement = fmt::format(
                "WITH rows AS (delete from MMS_APIKey where userKey = {} and workspaceKey = {} returning 1) "
				"select count(*) from rows",
				userKey, workspaceKey);
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

        // the users do not have any other workspace will be removed
		if (userToBeRemoved)
        {
			// in all the tables depending from User we have 'on delete cascade'
			// So all should be removed automatically from DB
			string sqlStatement = fmt::format( 
				"WITH rows AS (delete from MMS_User where userKey = {} "
				"returning 1) select count(*) from rows",
				userKey);
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

		return make_tuple(userToBeRemoved, name, eMailAddress);
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

tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>
	MMSEngineDBFacade::checkAPIKey (string apiKey, bool fromMaster)
{
    shared_ptr<Workspace> workspace;
    int64_t         userKey;
    Json::Value		permissionsRoot;

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
        int64_t         workspaceKey;
        
        {
            string sqlStatement = fmt::format( 
                "select userKey, workspaceKey, permissions from MMS_APIKey "
				"where apiKey = {} and expirationDate >= NOW() at time zone 'utc'",
				trans.quote(apiKey));
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
                userKey = res[0]["userKey"].as<int64_t>();
                workspaceKey = res[0]["workspaceKey"].as<int64_t>();
                string permissions = res[0]["permissions"].as<string>();
				permissionsRoot = JSONUtils::toJson(-1, -1, permissions);
            }
            else
            {
                string errorMessage = __FILEREF__ + "apiKey is not present or it is expired"
                    + ", apiKey: " + apiKey
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw APIKeyNotFoundOrExpired();
            }
        }

        workspace = getWorkspace(workspaceKey);

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
    catch(APIKeyNotFoundOrExpired& e)
	{
		SPDLOG_ERROR("APIKeyNotFoundOrExpired, SQL exception"
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

    return make_tuple(userKey, workspace,
		JSONUtils::asBool(permissionsRoot, "admin", false),
		JSONUtils::asBool(permissionsRoot, "createRemoveWorkspace", false),
		JSONUtils::asBool(permissionsRoot, "ingestWorkflow", false),
		JSONUtils::asBool(permissionsRoot, "createProfiles", false),
		JSONUtils::asBool(permissionsRoot, "deliveryAuthorization", false),
		JSONUtils::asBool(permissionsRoot, "shareWorkspace", false),
		JSONUtils::asBool(permissionsRoot, "editMedia", false),
		JSONUtils::asBool(permissionsRoot, "editConfiguration", false),
		JSONUtils::asBool(permissionsRoot, "killEncoding", false),
		JSONUtils::asBool(permissionsRoot, "cancelIngestionJob", false),
		JSONUtils::asBool(permissionsRoot, "editEncodersPool", false),
		JSONUtils::asBool(permissionsRoot, "applicationRecorder", false)
    );
}

Json::Value MMSEngineDBFacade::login (
	string eMailAddress, string password)
{
    Json::Value     loginDetailsRoot;

	// 2023-02-22: in questo metodo viene:
	//	1. controllato l'utente
	//	2. aggiornato il campo lastSuccessfulLogin
	// Poichè si cerca di far funzionare il piu possibile anche in caso di failure del master,
	// abbiamo separato le due attività, solo l'update viene fatta con connessione al master e,
	// se quest'ultima fallisce, comunque non viene bloccato il login
	int64_t userKey = -1;
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
			{
				string sqlStatement = fmt::format( 
					"select userKey, name, country, timezone, insolvent, "
					"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate, "
					"to_char(expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate "
					"from MMS_User where eMailAddress = {} and password = {} "
					"and expirationDate > NOW() at time zone 'utc'",
					trans.quote(eMailAddress), trans.quote(password));
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
					userKey = res[0]["userKey"].as<int64_t>();

					string field = "userKey";
					loginDetailsRoot[field] = userKey;

					field = "name";
					loginDetailsRoot[field] = res[0]["name"].as<string>();

					field = "email";
					loginDetailsRoot[field] = eMailAddress;

					field = "country";
					loginDetailsRoot[field] = res[0]["country"].as<string>();

					field = "timezone";
					loginDetailsRoot[field] = res[0]["timezone"].as<string>();

					field = "creationDate";
					loginDetailsRoot[field] = res[0]["creationDate"].as<string>();

					field = "insolvent";
					loginDetailsRoot[field] = res[0]["insolvent"].as<bool>();

					field = "expirationDate";
					loginDetailsRoot[field] = res[0]["expirationDate"].as<string>();

					/*
					{
						sqlStatement = 
							"update MMS_User set lastSuccessfulLogin = NOW() "
							"where userKey = ?";

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(sqlStatement));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, userKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", sqlStatement: " + sqlStatement
							+ ", userKey: " + to_string(userKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						if (rowsUpdated != 1)
						{
							string errorMessage = __FILEREF__ + "no update was done"
									+ ", userKey: " + to_string(userKey)
									+ ", rowsUpdated: " + to_string(rowsUpdated)
									+ ", sqlStatement: " + sqlStatement
							;
							_logger->warn(errorMessage);

							// throw runtime_error(errorMessage);
						}
					}
					*/
				}
				else
				{
					string errorMessage = __FILEREF__ + "email and/or password are wrong or user expired"
						+ ", eMailAddress: " + eMailAddress
						+ ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw LoginFailed();
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
		catch(LoginFailed& e)
		{
			SPDLOG_ERROR("LoginFailed, SQL exception"
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
					"WITH rows AS (update MMS_User set lastSuccessfulLogin = NOW() at time zone 'utc' "
					"where userKey = {} returning 1) select count(*) from rows",
					userKey);
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
							+ ", userKey: " + to_string(userKey)
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

			// throw e;
		}
		catch(LoginFailed& e)
		{
			SPDLOG_ERROR("LoginFailed SQL exception"
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

			// throw e;
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


			// throw e;
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

			// throw e;
		}
	}
    
    return loginDetailsRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceList (
	int64_t userKey, bool admin, bool costDetails)
{
	Json::Value workspaceListRoot;
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
			Json::Value requestParametersRoot;

			field = "userKey";
			requestParametersRoot[field] = userKey;

			/*
			field = "start";
			requestParametersRoot[field] = start;

			field = "rows";
			requestParametersRoot[field] = rows;
			*/

			field = "requestParameters";
			workspaceListRoot[field] = requestParametersRoot;
		}

		Json::Value responseRoot;
		{
			string sqlStatement;
			if (admin)
				sqlStatement = fmt::format(
					"select count(*) from MMS_Workspace w, MMS_APIKey a "
					"where w.workspaceKey = a.workspaceKey "
					"and a.userKey = {}",
					userKey);
			else
				sqlStatement = fmt::format(
					"select count(*) from MMS_Workspace w, MMS_APIKey a "
					"where w.workspaceKey = a.workspaceKey "
					"and a.userKey = {} "
					"and w.enabled = true and NOW() at time zone 'utc' < a.expirationDate",
					userKey);
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

		Json::Value workspacesRoot(Json::arrayValue);
        {
			string sqlStatement;
			if (admin)
				sqlStatement = fmt::format( 
					"select w.workspaceKey, w.enabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, "
					"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
					"to_char(a.expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
					"a.permissions, "
					"to_char(w.creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and userKey = {}",
					userKey);
			else
				sqlStatement = fmt::format( 
					"select w.workspaceKey, w.enabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, "
					"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
					"to_char(a.expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
					"a.permissions, "
					"to_char(w.creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
                    "from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and userKey = {} "
					"and w.enabled = true and NOW() at time zone 'utc' < a.expirationDate",
					userKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			bool userAPIKeyInfo = true;
			for (auto row: res)
            {
                Json::Value workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, trans, row, userAPIKeyInfo, costDetails);

                workspacesRoot.append(workspaceDetailRoot);                        
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }
        
		field = "workspaces";
		responseRoot[field] = workspacesRoot;

		field = "response";
		workspaceListRoot[field] = responseRoot;

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
    
	return workspaceListRoot;
}

Json::Value MMSEngineDBFacade::getLoginWorkspace(int64_t userKey, bool fromMaster)
{
	Json::Value loginWorkspaceRoot;

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
			// if admin returns all the workspaces of the user (even the one not enabled)
			// if NOT admin returns only the one having isEnabled = 1
			string sqlStatement = fmt::format( 
				"select w.workspaceKey, w.enabled, w.name, w.maxEncodingPriority, "
				"w.encodingPeriod, w.maxIngestionsNumber, "
				"w.languageCode, "
				"a.apiKey, a.isOwner, a.isDefault, "
				"to_char(a.expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
				"a.permissions, "
				"to_char(w.creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
				"from MMS_APIKey a, MMS_Workspace w "
				"where a.workspaceKey = w.workspaceKey "
				"and a.userKey = {} and a.isDefault = true "
				"and NOW() at time zone 'utc' < a.expirationDate "
				"and (a.permissions ->> 'admin' = 'true' or w.enabled = true) "
				"limit 1",
				userKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			bool userAPIKeyInfo = true;
			// 2024-01-05: Siamo in fase di login, probabilmente costDetails non servono ma li ho aggiunti
			//	perchè invece serve il campo maxStorageInGB. Serve alla GUI per mostrare lo storage
			bool costDetails = true;
			if (!empty(res))
            {
				auto row = res[0];
                loginWorkspaceRoot = getWorkspaceDetailsRoot (
					conn, trans, row, userAPIKeyInfo, costDetails);
            }
			else
			{
				string sqlStatement = fmt::format( 
					"select w.workspaceKey, w.enabled, w.name, w.maxEncodingPriority, "
					"w.encodingPeriod, w.maxIngestionsNumber, w.languageCode, "
					"a.apiKey, a.isOwner, a.isDefault, "
					"to_char(a.expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
					"a.permissions, "
					"to_char(w.creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
					"from MMS_APIKey a, MMS_Workspace w "
					"where a.workspaceKey = w.workspaceKey "
					"and a.userKey = {} "
					"and NOW() at time zone 'utc' < a.expirationDate "
					"and (a.permissions ->> 'admin' = 'true' or w.enabled = true) "
					"limit 1",
					userKey);
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
					auto row = res[0];
					loginWorkspaceRoot = getWorkspaceDetailsRoot (
						conn, trans, row, userAPIKeyInfo, costDetails);
				}
				else
				{
					string errorMessage = __FILEREF__ + "No workspace found"
						+ ", userKey: " + to_string(userKey)
						// + ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					// no exception, just return an empty loginWorkspaceRoot
					// throw runtime_error(errorMessage);
				}
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
    
	return loginWorkspaceRoot;
}

Json::Value MMSEngineDBFacade::getWorkspaceDetailsRoot (
	shared_ptr<PostgresConnection> conn,
	nontransaction& trans,
	row& row,
	bool userAPIKeyInfo,
	bool costDetails
	)
{
    Json::Value     workspaceDetailRoot;

    try
    {
		int64_t workspaceKey = row["workspaceKey"].as<int64_t>();

		string field = "workspaceKey";
		workspaceDetailRoot[field] = workspaceKey;
                
		field = "enabled";
		workspaceDetailRoot[field] = row["enabled"].as<bool>();

		field = "workspaceName";
		workspaceDetailRoot[field] = row["name"].as<string>();

		field = "maxEncodingPriority";
		workspaceDetailRoot[field] = row["maxEncodingPriority"].as<string>();

		field = "encodingPeriod";
		workspaceDetailRoot[field] = row["encodingPeriod"].as<string>();

		field = "maxIngestionsNumber";
		workspaceDetailRoot[field] = row["maxIngestionsNumber"].as<int>();

		if (costDetails)
		{
			field = "cost";
			workspaceDetailRoot[field] = getWorkspaceCost(conn, trans, workspaceKey);
		}

		{
			int64_t workSpaceUsageInBytes;

			pair<int64_t,int64_t> workSpaceUsageInBytesAndMaxStorageInMB =
				getWorkspaceUsage(conn, trans, workspaceKey);
			tie(workSpaceUsageInBytes, ignore) = workSpaceUsageInBytesAndMaxStorageInMB;

			int64_t workSpaceUsageInMB = workSpaceUsageInBytes / 1000000;

			field = "workSpaceUsageInMB";
			workspaceDetailRoot[field] = workSpaceUsageInMB;
		}

		field = "languageCode";
		workspaceDetailRoot[field] = row["languageCode"].as<string>();

		field = "creationDate";
		workspaceDetailRoot[field] = row["creationDate"].as<string>();

		if (userAPIKeyInfo)
		{
			Json::Value     userAPIKeyRoot;

			field = "apiKey";
			userAPIKeyRoot[field] = row["apiKey"].as<string>();

			field = "owner";
			userAPIKeyRoot[field] = row["isOwner"].as<bool>();

			field = "default";
			userAPIKeyRoot[field] = row["isDefault"].as<bool>();

			field = "expirationDate";
			userAPIKeyRoot[field] = row["expirationDate"].as<string>();

			string permissions = row["permissions"].as<string>();
			Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);

			field = "admin";
			bool admin = JSONUtils::asBool(permissionsRoot, "admin", false);
			userAPIKeyRoot[field] = admin;

			field = "createRemoveWorkspace";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "createRemoveWorkspace", false);

			field = "ingestWorkflow";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "ingestWorkflow", false);

			field = "createProfiles";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "createProfiles", false);

			field = "deliveryAuthorization";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "deliveryAuthorization", false);

			field = "shareWorkspace";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "shareWorkspace", false);

			field = "editMedia";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editMedia", false);
                
			field = "editConfiguration";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editConfiguration", false);

			field = "killEncoding";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "killEncoding", false);

			field = "cancelIngestionJob";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "cancelIngestionJob", false);

			field = "editEncodersPool";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "editEncodersPool", false);

			field = "applicationRecorder";
			if(admin)
				userAPIKeyRoot[field] = true;
			else
				userAPIKeyRoot[field] = JSONUtils::asBool(permissionsRoot, "applicationRecorder", false);

			field = "userAPIKey";
			workspaceDetailRoot[field] = userAPIKeyRoot;

			if (admin)
			{
				{
					string sqlStatement = fmt::format(
						"select u.userKey as workspaceOwnerUserKey, u.name as workspaceOwnerUserName "
						"from MMS_APIKey ak, MMS_User u "
						"where ak.userKey = u.userKey "
						"and ak.workspaceKey = {} and ak.isOwner = true",
						workspaceKey);
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
						field = "workspaceOwnerUserKey";
						workspaceDetailRoot[field] = res[0]["workspaceOwnerUserKey"].as<int64_t>();

						field = "workspaceOwnerUserName";
						workspaceDetailRoot[field] = res[0]["workspaceOwnerUserName"].as<string>();
					}
				}
			}
		}
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return workspaceDetailRoot;
}

Json::Value MMSEngineDBFacade::updateWorkspaceDetails (
	int64_t userKey,
	int64_t workspaceKey,
	bool enabledChanged, bool newEnabled,
	bool nameChanged, string newName,
	bool maxEncodingPriorityChanged, string newMaxEncodingPriority,
	bool encodingPeriodChanged, string newEncodingPeriod,
	bool maxIngestionsNumberChanged, int64_t newMaxIngestionsNumber,
	bool languageCodeChanged, string newLanguageCode,
	bool expirationDateChanged, string newExpirationUtcDate,

	bool maxStorageInGBChanged, int64_t maxStorageInGB,
	bool currentCostForStorageChanged, int64_t currentCostForStorage,
	bool dedicatedEncoder_power_1Changed, int64_t dedicatedEncoder_power_1,
	bool currentCostForDedicatedEncoder_power_1Changed, int64_t currentCostForDedicatedEncoder_power_1,
	bool dedicatedEncoder_power_2Changed, int64_t dedicatedEncoder_power_2,
	bool currentCostForDedicatedEncoder_power_2Changed, int64_t currentCostForDedicatedEncoder_power_2,
	bool dedicatedEncoder_power_3Changed, int64_t dedicatedEncoder_power_3,
	bool currentCostForDedicatedEncoder_power_3Changed, int64_t currentCostForDedicatedEncoder_power_3,
	bool CDN_type_1Changed, int64_t CDN_type_1,
	bool currentCostForCDN_type_1Changed, int64_t currentCostForCDN_type_1,
	bool support_type_1Changed, bool support_type_1,
	bool currentCostForSupport_type_1Changed, int64_t currentCostForSupport_type_1,

	bool newCreateRemoveWorkspace,
	bool newIngestWorkflow,
	bool newCreateProfiles,
	bool newDeliveryAuthorization,
	bool newShareWorkspace,
	bool newEditMedia,
	bool newEditConfiguration,
	bool newKillEncoding,
	bool newCancelIngestionJob,
	bool newEditEncodersPool,
	bool newApplicationRecorder)
{
    Json::Value		workspaceDetailRoot;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        // check if ADMIN flag is already present
        bool admin = false;
		bool isOwner = false;
        {
            string sqlStatement = fmt::format( 
                "select isOwner, permissions from MMS_APIKey "
				"where workspaceKey = {} and userKey = {}",
				workspaceKey, userKey);
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
                string permissions = res[0]["permissions"].as<string>();
				Json::Value permissionsRoot = JSONUtils::toJson(-1, -1, permissions);
                
                admin = JSONUtils::asBool(permissionsRoot, "admin", false);
                isOwner = res[0]["isOwner"].as<bool>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "user/workspace are not found"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", userKey: " + to_string(userKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

		if (!admin && !isOwner)
		{
			string errorMessage = __FILEREF__ + "The user requesting the update does not have neither the admin nor the ownership rights and the update cannot be done"
				+ ", workspaceKey: " + to_string(workspaceKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		if (admin)
		{
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (enabledChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("enabled = {}", newEnabled);
				oneParameterPresent = true;
			}

			if (maxEncodingPriorityChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("maxEncodingPriority = {}", trans.quote(newMaxEncodingPriority));
				oneParameterPresent = true;
			}

			if (encodingPeriodChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("encodingPeriod = {}", trans.quote(newEncodingPeriod));
				oneParameterPresent = true;
			}

			if (maxIngestionsNumberChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("maxIngestionsNumber = {}", newMaxIngestionsNumber);
				oneParameterPresent = true;
			}

			if (oneParameterPresent)
			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_Workspace {} "
					"where workspaceKey = {} returning 1) select count(*) from rows",
					setSQL, workspaceKey);
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
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newEnabled: " + to_string(newEnabled)
                        + ", newMaxEncodingPriority: " + newMaxEncodingPriority
                        + ", newEncodingPeriod: " + newEncodingPeriod
                        + ", newMaxIngestionsNumber: " + to_string(newMaxIngestionsNumber)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}

			if (expirationDateChanged)
			{
				// 2023-02-13: nel caso in cui un admin vuole cambiare la data di scadenza di un workspace,
				//		questo cambiamento deve avvenire per tutte le chiavi presenti
				string sqlStatement = fmt::format(
					"WITH rows AS (update MMS_APIKey "
					"set expirationDate = to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') "
					"where workspaceKey = {} returning 1) select count(*) from rows",
					trans.quote(newExpirationUtcDate), workspaceKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated == 0)
				{
					string errorMessage = __FILEREF__ + "no update was done"
						+ ", newExpirationUtcDate: " + newExpirationUtcDate
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}
        }

        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (nameChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("name = {}", trans.quote(newName));
				oneParameterPresent = true;
			}

			if (languageCodeChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("languageCode = {}", trans.quote(newLanguageCode));
				oneParameterPresent = true;
			}

			if (oneParameterPresent)
			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_Workspace {} "
					"where workspaceKey = {} returning 1) select count(*) from rows",
					setSQL, workspaceKey);
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
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newName: " + newName
                        + ", newLanguageCode: " + newLanguageCode
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}
        }

		// 2024-01-05: questi cambiamenti possono essere fatti da chiunque tramite la pagina dei costi
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (maxStorageInGBChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("maxStorageInGB = {}", maxStorageInGB);
				oneParameterPresent = true;
			}

			if (currentCostForStorageChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForStorage = {}", currentCostForStorage);
				oneParameterPresent = true;
			}

			if (dedicatedEncoder_power_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("dedicatedEncoder_power_1 = {}", dedicatedEncoder_power_1);
				oneParameterPresent = true;
			}

			if (currentCostForDedicatedEncoder_power_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForDedicatedEncoder_power_1 = {}", currentCostForDedicatedEncoder_power_1);
				oneParameterPresent = true;
			}

			if (dedicatedEncoder_power_2Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("dedicatedEncoder_power_2 = {}", dedicatedEncoder_power_2);
				oneParameterPresent = true;
			}

			if (currentCostForDedicatedEncoder_power_2Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForDedicatedEncoder_power_2 = {}", currentCostForDedicatedEncoder_power_2);
				oneParameterPresent = true;
			}

			if (dedicatedEncoder_power_3Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("dedicatedEncoder_power_3 = {}", dedicatedEncoder_power_3);
				oneParameterPresent = true;
			}

			if (currentCostForDedicatedEncoder_power_3Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForDedicatedEncoder_power_3 = {}", currentCostForDedicatedEncoder_power_3);
				oneParameterPresent = true;
			}

			if (CDN_type_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("CDN_type_1 = {}", CDN_type_1);
				oneParameterPresent = true;
			}

			if (currentCostForCDN_type_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForCDN_type_1 = {}", currentCostForCDN_type_1);
				oneParameterPresent = true;
			}

			if (support_type_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("support_type_1 = {}", support_type_1);
				oneParameterPresent = true;
			}

			if (currentCostForSupport_type_1Changed)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("currentCostForSupport_type_1 = {}", currentCostForSupport_type_1);
				oneParameterPresent = true;
			}

			if (oneParameterPresent)
			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_WorkspaceCost {} "
					"where workspaceKey = {} returning 1) select count(*) from rows",
					setSQL, workspaceKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans.exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", rowsUpdated: {}"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, rowsUpdated, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = __FILEREF__ + "no update was done"
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", newName: " + newName
                        + ", newLanguageCode: " + newLanguageCode
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
					;
					_logger->warn(errorMessage);

					// throw runtime_error(errorMessage);
				}
			}
        }

        {
			Json::Value permissionsRoot;
			permissionsRoot["admin"] = admin;
			permissionsRoot["createRemoveWorkspace"] = newCreateRemoveWorkspace;
			permissionsRoot["ingestWorkflow"] = newIngestWorkflow;
			permissionsRoot["createProfiles"] = newCreateProfiles;
			permissionsRoot["deliveryAuthorization"] = newDeliveryAuthorization;
			permissionsRoot["shareWorkspace"] = newShareWorkspace;
			permissionsRoot["editMedia"] = newEditMedia;
			permissionsRoot["editConfiguration"] = newEditConfiguration;
			permissionsRoot["killEncoding"] = newKillEncoding;
			permissionsRoot["cancelIngestionJob"] = newCancelIngestionJob;
			permissionsRoot["editEncodersPool"] = newEditEncodersPool;
			permissionsRoot["applicationRecorder"] = newApplicationRecorder;

			string permissions = JSONUtils::toString(permissionsRoot);

			string sqlStatement = fmt::format(
				"WITH rows AS (update MMS_APIKey set permissions = {} "
				"where workspaceKey = {} and userKey = {} returning 1) select count(*) from rows",
				trans.quote(permissions), workspaceKey, userKey);
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
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", userKey: " + to_string(userKey)
                        + ", permissions: " + permissions
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }

        {
			string sqlStatement = fmt::format( 
				"select w.workspaceKey, w.enabled, w.name, w.maxEncodingPriority, "
				"w.encodingPeriod, w.maxIngestionsNumber, "
				"w.languageCode, a.apiKey, a.isOwner, a.isDefault, "
				"to_char(a.expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
				"a.permissions, "
				"to_char(w.creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
				"from MMS_APIKey a, MMS_Workspace w "
				"where a.workspaceKey = w.workspaceKey "
				"and a.workspaceKey = {} "
				"and userKey = {}",
				workspaceKey, userKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
			bool userAPIKeyInfo = true;
			bool costDetails = false;
			if (!empty(res))
            {
				auto row = res[0];
				workspaceDetailRoot = getWorkspaceDetailsRoot (
					conn, trans, row, userAPIKeyInfo, costDetails);
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
    
    return workspaceDetailRoot;
}

Json::Value MMSEngineDBFacade::setWorkspaceAsDefault (
        int64_t userKey,
        int64_t workspaceKey,
		int64_t workspaceKeyToBeSetAsDefault)
{
    Json::Value workspaceDetailRoot;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		string apiKey;
        {
            string sqlStatement = fmt::format( 
                "select apiKey from MMS_APIKey where workspaceKey = {} and userKey = {}",
				workspaceKeyToBeSetAsDefault, userKey);
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
				apiKey = res[0]["apiKey"].as<string>();
            else
            {
                string errorMessage = __FILEREF__ + "user/workspace are not found"
                    + ", workspaceKey: " + to_string(workspaceKeyToBeSetAsDefault)
                    + ", userKey: " + to_string(userKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_APIKey set isDefault = false "
                "where userKey = {} returning 1) select count(*) from rows",
				userKey);
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

        {
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_APIKey set isDefault = true "
                "where apiKey = {} returning 1) select count(*) from rows", trans.quote(apiKey));
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
    
    return workspaceDetailRoot;
}

pair<int64_t,int64_t> MMSEngineDBFacade::getWorkspaceUsage(
        int64_t workspaceKey)
{
	pair<int64_t,int64_t>	workspaceUsage;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		workspaceUsage = getWorkspaceUsage(conn, trans, workspaceKey);

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
    
    return workspaceUsage;
}

Json::Value MMSEngineDBFacade::getWorkspaceCost(
	shared_ptr<PostgresConnection> conn,
	nontransaction& trans,
	int64_t workspaceKey)
{
	Json::Value workspaceCost;

    try
    {
		{
			string sqlStatement = fmt::format( 
				"select maxStorageInGB, currentCostForStorage, "
				"dedicatedEncoder_power_1, currentCostForDedicatedEncoder_power_1, "
				"dedicatedEncoder_power_2, currentCostForDedicatedEncoder_power_2, "
				"dedicatedEncoder_power_3, currentCostForDedicatedEncoder_power_3, "
				"CDN_type_1, currentCostForCDN_type_1, "
				"support_type_1, currentCostForSupport_type_1 "
				"from MMS_WorkspaceCost "
                "where workspaceKey = {} ",
				workspaceKey);
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
				string errorMessage = fmt::format("no workspace cost found"
					", workspaceKey: {}", workspaceKey);
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			workspaceCost["maxStorageInGB"] = res[0]["maxStorageInGB"].as<int>();
			workspaceCost["currentCostForStorage"] = res[0]["currentCostForStorage"].as<int>();
			workspaceCost["dedicatedEncoder_power_1"] = res[0]["dedicatedEncoder_power_1"].as<int>();
			workspaceCost["currentCostForDedicatedEncoder_power_1"] = res[0]["currentCostForDedicatedEncoder_power_1"].as<int>();
			workspaceCost["dedicatedEncoder_power_2"] = res[0]["dedicatedEncoder_power_2"].as<int>();
			workspaceCost["currentCostForDedicatedEncoder_power_2"] = res[0]["currentCostForDedicatedEncoder_power_2"].as<int>();
			workspaceCost["dedicatedEncoder_power_3"] = res[0]["dedicatedEncoder_power_3"].as<int>();
			workspaceCost["currentCostForDedicatedEncoder_power_3"] = res[0]["currentCostForDedicatedEncoder_power_3"].as<int>();
			workspaceCost["CDN_type_1"] = res[0]["CDN_type_1"].as<int>();
			workspaceCost["currentCostForCDN_type_1"] = res[0]["currentCostForCDN_type_1"].as<int>();
			workspaceCost["support_type_1"] = res[0]["support_type_1"].as<bool>();
			workspaceCost["currentCostForSupport_type_1"] = res[0]["currentCostForSupport_type_1"].as<int>();
        }
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return workspaceCost;
}

pair<int64_t,int64_t> MMSEngineDBFacade::getWorkspaceUsage(
	shared_ptr<PostgresConnection> conn,
	nontransaction& trans,
	int64_t workspaceKey)
{
    int64_t         totalSizeInBytes;
    int64_t         maxStorageInMB;
    
    try
    {
        {
            string sqlStatement = fmt::format( 
                "select SUM(pp.sizeInBytes) as totalSizeInBytes from MMS_MediaItem mi, MMS_PhysicalPath pp "
                "where mi.mediaItemKey = pp.mediaItemKey and mi.workspaceKey = {} "
				"and externalReadOnlyStorage = false",
				workspaceKey);
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
                if (res[0]["totalSizeInBytes"].is_null())
                    totalSizeInBytes = -1;
                else
                    totalSizeInBytes = res[0]["totalSizeInBytes"].as<int64_t>();
            }
        }
        
        {
            string sqlStatement = fmt::format(
                "select maxStorageInGB from MMS_WorkspaceCost where workspaceKey = {}",
				workspaceKey);
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
                maxStorageInMB = res[0]["maxStorageInGB"].as<int>() * 1000;
            else
            {
                string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        return make_pair(totalSizeInBytes, maxStorageInMB);
    }
	catch(sql_error const &e)
	{
		SPDLOG_ERROR("SQL exception"
			", query: {}"
			", exceptionMessage: {}"
			", conn: {}",
			e.query(), e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

pair<string, string> MMSEngineDBFacade::getUserDetails (int64_t userKey)
{
    string emailAddress;
    string name;
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
                "select name, eMailAddress from MMS_User where userKey = {}",
				userKey);
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
                name = res[0]["name"].as<string>();
                emailAddress = res[0]["eMailAddress"].as<string>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "User are not present"
                    + ", userKey: " + to_string(userKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

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
    catch(APIKeyNotFoundOrExpired& e)
	{
		SPDLOG_ERROR("APIKeyNotFoundOrExpired, SQL exception"
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

    pair<string, string> emailAddressAndName = make_pair(emailAddress, name);
            
    return emailAddressAndName;
}

pair<int64_t, string> MMSEngineDBFacade::getUserDetailsByEmail (string email, bool warningIfError)
{
	int64_t			userKey;
	string			name;
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
                "select userKey, name from MMS_User where eMailAddress = {}",
				trans.quote(email));
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
                userKey = res[0]["userKey"].as<int64_t>();
                name = res[0]["name"].as<string>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "User is not present"
                    + ", email: " + email
                    + ", sqlStatement: " + sqlStatement
                ;
				if (warningIfError)
					_logger->warn(errorMessage);
				else
					_logger->error(errorMessage);

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
		if (warningIfError)
			SPDLOG_WARN("runtime_error"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
		else
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

	pair<int64_t, string> userDetails = make_pair(userKey, name);

    return userDetails;
}

Json::Value MMSEngineDBFacade::updateUser (
		bool admin,
		bool ldapEnabled,
        int64_t userKey,
        bool nameChanged, string name,
        bool emailChanged, string email, 
        bool countryChanged, string country,
        bool timezoneChanged, string timezone,
        bool insolventChanged, bool insolvent,
        bool expirationDateChanged, string expirationUtcDate,
		bool passwordChanged, string newPassword, string oldPassword)
{
    Json::Value     loginDetailsRoot;
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
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (passwordChanged)
			{
				string savedPassword;
				{
					string sqlStatement = fmt::format( 
						"select password from MMS_User where userKey = {}",
						userKey);
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
						savedPassword = res[0]["password"].as<string>();
					else
					{
						string errorMessage = __FILEREF__ + "User is not present"
							+ ", userKey: " + to_string(userKey)
							+ ", sqlStatement: " + sqlStatement
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
				}

				if (savedPassword != oldPassword
						|| newPassword == "")
				{
					string errorMessage = __FILEREF__
						+ "old password is wrong or newPassword is not valid"
						+ ", userKey: " + to_string(userKey)
					;
					_logger->warn(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("password = {}", trans.quote(newPassword));
				oneParameterPresent = true;
			}

			if (nameChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("name = {}", trans.quote(name));
				oneParameterPresent = true;
			}

			if (emailChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("eMailAddress = {}", trans.quote(email));
				oneParameterPresent = true;
			}

			if (countryChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("country = {}", trans.quote(country));
				oneParameterPresent = true;
			}

			if (timezoneChanged)
			{
				if (!isTimezoneValid(timezone))
					timezone = "CET";

				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("timezone = {}", trans.quote(timezone));
				oneParameterPresent = true;
			}

			if (admin && insolventChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("insolvent = {}", insolvent);
				oneParameterPresent = true;
			}

			if (admin && expirationDateChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("expirationDate = {}", trans.quote(expirationUtcDate));
				oneParameterPresent = true;
			}

			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_User {} "
				"where userKey = {} returning 1) select count(*) from rows",
				setSQL, userKey);
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
                        + ", userKey: " + to_string(userKey)
                        + ", name: " + name
                        + ", country: " + country
                        + ", email: " + email
                        // + ", password: " + password
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
        }

		{
            string sqlStatement = fmt::format( 
                "select "
				"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate, "
				"to_char(expirationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as expirationDate, "
				"userKey, name, eMailAddress, country, timezone, insolvent "
                "from MMS_User where userKey = {}",
				userKey);
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
                string field = "creationDate";
                loginDetailsRoot[field] = res[0]["creationDate"].as<string>();

                field = "expirationDate";
                loginDetailsRoot[field] = res[0]["expirationDate"].as<string>();

				field = "userKey";
				loginDetailsRoot[field] = res[0]["userKey"].as<int64_t>();

				field = "name";
				loginDetailsRoot[field] = res[0]["name"].as<string>();

				field = "email";
				loginDetailsRoot[field] = res[0]["eMailAddress"].as<string>();

				field = "country";
				loginDetailsRoot[field] = res[0]["country"].as<string>();
                
				field = "timezone";
				loginDetailsRoot[field] = res[0]["timezone"].as<string>();
                
				field = "insolvent";
				loginDetailsRoot[field] = res[0]["insolvent"].as<bool>();

				field = "ldapEnabled";
				loginDetailsRoot[field] = ldapEnabled;
            }
            else
            {
                string errorMessage = __FILEREF__ + "userKey is wrong"
                    + ", userKey: " + to_string(userKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

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
    
    return loginDetailsRoot;
}

string MMSEngineDBFacade::createResetPasswordToken(
	int64_t userKey
)
{
	string		resetPasswordToken;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
		resetPasswordToken = to_string(e());

        {
			string sqlStatement = fmt::format( 
				"insert into MMS_ResetPasswordToken (token, userKey, creationDate) "
				"values ({}, {}, NOW() at time zone 'utc')",
				trans.quote(resetPasswordToken), userKey);
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

    return resetPasswordToken;
}

pair<string, string> MMSEngineDBFacade::resetPassword(
	string resetPasswordToken,
	string newPassword
)
{
	string		name;
	string		email;
    
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		int resetPasswordRetentionInHours = 24;

		int userKey;
        {
            string sqlStatement = fmt::format( 
				"select u.name, u.eMailAddress, u.userKey "
				"from MMS_ResetPasswordToken rp, MMS_User u "
				"where rp.userKey = u.userKey and rp.token = {} "
				"and rp.creationDate + INTERVAL '{} HOUR') >= NOW() at time zone 'utc'",
				trans.quote(resetPasswordToken), resetPasswordRetentionInHours);
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
                name = res[0]["name"].as<string>();
                email = res[0]["eMailAddress"].as<string>();
                userKey = res[0]["userKey"].as<int64_t>();
            }
            else
            {
                string errorMessage = __FILEREF__
					+ "reset password token is not present or is expired"
                    + ", resetPasswordToken: " + resetPasswordToken
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_User set password = {} "
				"where userKey = {} returning 1) select count(*) from rows",
				trans.quote(newPassword), userKey);
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
				string errorMessage = __FILEREF__ + "no update was done"
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);                    
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

	pair<string, string> details = make_pair(name, email);

    return details;
}

bool MMSEngineDBFacade::isTimezoneValid(string timezone)
{
    set<string> timezones = {
		// (GMT-12:00)
		"Etc/GMT+12",
		// (GMT-11:00)
		"Etc/GMT+11",
		// (GMT-11:00)
		"Pacific/Midway",
		// (GMT-11:00)
		"Pacific/Niue",
		// (GMT-11:00)
		"Pacific/Pago_Pago",
		// (GMT-11:00)
		"Pacific/Samoa",
		// (GMT-11:00)
		"US/Samoa",
		// (GMT-10:00)
		"America/Adak",
		// (GMT-10:00)
		"America/Atka",
		// (GMT-10:00)
		"Etc/GMT+10",
		// (GMT-10:00)
		"HST",
		// (GMT-10:00)
		"Pacific/Honolulu",
		// (GMT-10:00)
		"Pacific/Johnston",
		// (GMT-10:00)
		"Pacific/Rarotonga",
		// (GMT-10:00)
		"Pacific/Tahiti",
		// (GMT-10:00)
		"SystemV/HST10",
		// (GMT-10:00)
		"US/Aleutian",
		// (GMT-10:00)
		"US/Hawaii",
		// (GMT-9:30)
		"Pacific/Marquesas",
		// (GMT-9:00)
		"AST",
		// (GMT-9:00)
		"America/Anchorage",
		// (GMT-9:00)
		"America/Juneau",
		// (GMT-9:00)
		"America/Nome",
		// (GMT-9:00)
		"America/Sitka",
		// (GMT-9:00)
		"America/Yakutat",
		// (GMT-9:00)
		"Etc/GMT+9",
		// (GMT-9:00)
		"Pacific/Gambier",
		// (GMT-9:00)
		"SystemV/YST9",
		// (GMT-9:00)
		"SystemV/YST9YDT",
		// (GMT-9:00)
		"US/Alaska",
		// (GMT-8:00)
		"America/Dawson",
		// (GMT-8:00)
		"America/Ensenada",
		// (GMT-8:00)
		"America/Los_Angeles",
		// (GMT-8:00)
		"America/Metlakatla",
		// (GMT-8:00)
		"America/Santa_Isabel",
		// (GMT-8:00)
		"America/Tijuana",
		// (GMT-8:00)
		"America/Vancouver",
		// (GMT-8:00)
		"America/Whitehorse",
		// (GMT-8:00)
		"Canada/Pacific",
		// (GMT-8:00)
		"Canada/Yukon",
		// (GMT-8:00)
		"Etc/GMT+8",
		// (GMT-8:00)
		"Mexico/BajaNorte",
		// (GMT-8:00)
		"PST",
		// (GMT-8:00)
		"PST8PDT",
		// (GMT-8:00)
		"Pacific/Pitcairn",
		// (GMT-8:00)
		"SystemV/PST8",
		// (GMT-8:00)
		"SystemV/PST8PDT",
		// (GMT-8:00)
		"US/Pacific",
		// (GMT-8:00)
		"US/Pacific-New",
		// (GMT-7:00)
		"America/Boise",
		// (GMT-7:00)
		"America/Cambridge_Bay",
		// (GMT-7:00)
		"America/Chihuahua",
		// (GMT-7:00)
		"America/Creston",
		// (GMT-7:00)
		"America/Dawson_Creek",
		// (GMT-7:00)
		"America/Denver",
		// (GMT-7:00)
		"America/Edmonton",
		// (GMT-7:00)
		"America/Hermosillo",
		// (GMT-7:00)
		"America/Inuvik",
		// (GMT-7:00)
		"America/Mazatlan",
		// (GMT-7:00)
		"America/Ojinaga",
		// (GMT-7:00)
		"America/Phoenix",
		// (GMT-7:00)
		"America/Shiprock",
		// (GMT-7:00)
		"America/Yellowknife",
		// (GMT-7:00)
		"Canada/Mountain",
		// (GMT-7:00)
		"Etc/GMT+7",
		// (GMT-7:00)
		"MST",
		// (GMT-7:00)
		"MST7MDT",
		// (GMT-7:00)
		"Mexico/BajaSur",
		// (GMT-7:00)
		"Navajo",
		// (GMT-7:00)
		"PNT",
		// (GMT-7:00)
		"SystemV/MST7",
		// (GMT-7:00)
		"SystemV/MST7MDT",
		// (GMT-7:00)
		"US/Arizona",
		// (GMT-7:00)
		"US/Mountain",
		// (GMT-6:00)
		"America/Bahia_Banderas",
		// (GMT-6:00)
		"America/Belize",
		// (GMT-6:00)
		"America/Cancun",
		// (GMT-6:00)
		"America/Chicago",
		// (GMT-6:00)
		"America/Costa_Rica",
		// (GMT-6:00)
		"America/El_Salvador",
		// (GMT-6:00)
		"America/Guatemala",
		// (GMT-6:00)
		"America/Indiana/Knox",
		// (GMT-6:00)
		"America/Indiana/Tell_City",
		// (GMT-6:00)
		"America/Knox_IN",
		// (GMT-6:00)
		"America/Managua",
		// (GMT-6:00)
		"America/Matamoros",
		// (GMT-6:00)
		"America/Menominee",
		// (GMT-6:00)
		"America/Merida",
		// (GMT-6:00)
		"America/Mexico_City",
		// (GMT-6:00)
		"America/Monterrey",
		// (GMT-6:00)
		"America/North_Dakota/Beulah",
		// (GMT-6:00)
		"America/North_Dakota/Center",
		// (GMT-6:00)
		"America/North_Dakota/New_Salem",
		// (GMT-6:00)
		"America/Rainy_River",
		// (GMT-6:00)
		"America/Rankin_Inlet",
		// (GMT-6:00)
		"America/Regina",
		// (GMT-6:00)
		"America/Resolute",
		// (GMT-6:00)
		"America/Swift_Current",
		// (GMT-6:00)
		"America/Tegucigalpa",
		// (GMT-6:00)
		"America/Winnipeg",
		// (GMT-6:00)
		"CST",
		// (GMT-6:00)
		"CST6CDT",
		// (GMT-6:00)
		"Canada/Central",
		// (GMT-6:00)
		"Canada/East-Saskatchewan",
		// (GMT-6:00)
		"Canada/Saskatchewan",
		// (GMT-6:00)
		"Chile/EasterIsland",
		// (GMT-6:00)
		"Etc/GMT+6",
		// (GMT-6:00)
		"Mexico/General",
		// (GMT-6:00)
		"Pacific/Easter",
		// (GMT-6:00)
		"Pacific/Galapagos",
		// (GMT-6:00)
		"SystemV/CST6",
		// (GMT-6:00)
		"SystemV/CST6CDT",
		// (GMT-6:00)
		"US/Central",
		// (GMT-6:00)
		"US/Indiana-Starke",
		// (GMT-5:00)
		"America/Atikokan",
		// (GMT-5:00)
		"America/Bogota",
		// (GMT-5:00)
		"America/Cayman",
		// (GMT-5:00)
		"America/Coral_Harbour",
		// (GMT-5:00)
		"America/Detroit",
		// (GMT-5:00)
		"America/Eirunepe",
		// (GMT-5:00)
		"America/Fort_Wayne",
		// (GMT-5:00)
		"America/Grand_Turk",
		// (GMT-5:00)
		"America/Guayaquil",
		// (GMT-5:00)
		"America/Havana",
		// (GMT-5:00)
		"America/Indiana/Indianapolis",
		// (GMT-5:00)
		"America/Indiana/Marengo",
		// (GMT-5:00)
		"America/Indiana/Petersburg",
		// (GMT-5:00)
		"America/Indiana/Vevay",
		// (GMT-5:00)
		"America/Indiana/Vincennes",
		// (GMT-5:00)
		"America/Indiana/Winamac",
		// (GMT-5:00)
		"America/Indianapolis",
		// (GMT-5:00)
		"America/Iqaluit",
		// (GMT-5:00)
		"America/Jamaica",
		// (GMT-5:00)
		"America/Kentucky/Louisville",
		// (GMT-5:00)
		"America/Kentucky/Monticello",
		// (GMT-5:00)
		"America/Lima",
		// (GMT-5:00)
		"America/Louisville",
		// (GMT-5:00)
		"America/Montreal",
		// (GMT-5:00)
		"America/Nassau",
		// (GMT-5:00)
		"America/New_York",
		// (GMT-5:00)
		"America/Nipigon",
		// (GMT-5:00)
		"America/Panama",
		// (GMT-5:00)
		"America/Pangnirtung",
		// (GMT-5:00)
		"America/Port-au-Prince",
		// (GMT-5:00)
		"America/Porto_Acre",
		// (GMT-5:00)
		"America/Rio_Branco",
		// (GMT-5:00)
		"America/Thunder_Bay",
		// (GMT-5:00)
		"America/Toronto",
		// (GMT-5:00)
		"Brazil/Acre",
		// (GMT-5:00)
		"Canada/Eastern",
		// (GMT-5:00)
		"Cuba",
		// (GMT-5:00)
		"EST",
		// (GMT-5:00)
		"EST5EDT",
		// (GMT-5:00)
		"Etc/GMT+5",
		// (GMT-5:00)
		"IET",
		// (GMT-5:00)
		"Jamaica",
		// (GMT-5:00)
		"SystemV/EST5",
		// (GMT-5:00)
		"SystemV/EST5EDT",
		// (GMT-5:00)
		"US/East-Indiana",
		// (GMT-5:00)
		"US/Eastern",
		// (GMT-5:00)
		"US/Michigan",
		// (GMT-4:30)
		"America/Caracas",
		// (GMT-4:00)
		"America/Anguilla",
		// (GMT-4:00)
		"America/Antigua",
		// (GMT-4:00)
		"America/Aruba",
		// (GMT-4:00)
		"America/Asuncion",
		// (GMT-4:00)
		"America/Barbados",
		// (GMT-4:00)
		"America/Blanc-Sablon",
		// (GMT-4:00)
		"America/Boa_Vista",
		// (GMT-4:00)
		"America/Campo_Grande",
		// (GMT-4:00)
		"America/Cuiaba",
		// (GMT-4:00)
		"America/Curacao",
		// (GMT-4:00)
		"America/Dominica",
		// (GMT-4:00)
		"America/Glace_Bay",
		// (GMT-4:00)
		"America/Goose_Bay",
		// (GMT-4:00)
		"America/Grenada",
		// (GMT-4:00)
		"America/Guadeloupe",
		// (GMT-4:00)
		"America/Guyana",
		// (GMT-4:00)
		"America/Halifax",
		// (GMT-4:00)
		"America/Kralendijk",
		// (GMT-4:00)
		"America/La_Paz",
		// (GMT-4:00)
		"America/Lower_Princes",
		// (GMT-4:00)
		"America/Manaus",
		// (GMT-4:00)
		"America/Marigot",
		// (GMT-4:00)
		"America/Martinique",
		// (GMT-4:00)
		"America/Moncton",
		// (GMT-4:00)
		"America/Montserrat",
		// (GMT-4:00)
		"America/Port_of_Spain",
		// (GMT-4:00)
		"America/Porto_Velho",
		// (GMT-4:00)
		"America/Puerto_Rico",
		// (GMT-4:00)
		"America/Santiago",
		// (GMT-4:00)
		"America/Santo_Domingo",
		// (GMT-4:00)
		"America/St_Barthelemy",
		// (GMT-4:00)
		"America/St_Kitts",
		// (GMT-4:00)
		"America/St_Lucia",
		// (GMT-4:00)
		"America/St_Thomas",
		// (GMT-4:00)
		"America/St_Vincent",
		// (GMT-4:00)
		"America/Thule",
		// (GMT-4:00)
		"America/Tortola",
		// (GMT-4:00)
		"America/Virgin",
		// (GMT-4:00)
		"Antarctica/Palmer",
		// (GMT-4:00)
		"Atlantic/Bermuda",
		// (GMT-4:00)
		"Brazil/West",
		// (GMT-4:00)
		"Canada/Atlantic",
		// (GMT-4:00)
		"Chile/Continental",
		// (GMT-4:00)
		"Etc/GMT+4",
		// (GMT-4:00)
		"PRT",
		// (GMT-4:00)
		"SystemV/AST4",
		// (GMT-4:00)
		"SystemV/AST4ADT",
		// (GMT-3:30)
		"America/St_Johns",
		// (GMT-3:30)
		"CNT",
		// (GMT-3:30)
		"Canada/Newfoundland",
		// (GMT-3:00)
		"AGT",
		// (GMT-3:00)
		"America/Araguaina",
		// (GMT-3:00)
		"America/Argentina/Buenos_Aires",
		// (GMT-3:00)
		"America/Argentina/Catamarca",
		// (GMT-3:00)
		"America/Argentina/ComodRivadavia",
		// (GMT-3:00)
		"America/Argentina/Cordoba",
		// (GMT-3:00)
		"America/Argentina/Jujuy",
		// (GMT-3:00)
		"America/Argentina/La_Rioja",
		// (GMT-3:00)
		"America/Argentina/Mendoza",
		// (GMT-3:00)
		"America/Argentina/Rio_Gallegos",
		// (GMT-3:00)
		"America/Argentina/Salta",
		// (GMT-3:00)
		"America/Argentina/San_Juan",
		// (GMT-3:00)
		"America/Argentina/San_Luis",
		// (GMT-3:00)
		"America/Argentina/Tucuman",
		// (GMT-3:00)
		"America/Argentina/Ushuaia",
		// (GMT-3:00)
		"America/Bahia",
		// (GMT-3:00)
		"America/Belem",
		// (GMT-3:00)
		"America/Buenos_Aires",
		// (GMT-3:00)
		"America/Catamarca",
		// (GMT-3:00)
		"America/Cayenne",
		// (GMT-3:00)
		"America/Cordoba",
		// (GMT-3:00)
		"America/Fortaleza",
		// (GMT-3:00)
		"America/Godthab",
		// (GMT-3:00)
		"America/Jujuy",
		// (GMT-3:00)
		"America/Maceio",
		// (GMT-3:00)
		"America/Mendoza",
		// (GMT-3:00)
		"America/Miquelon",
		// (GMT-3:00)
		"America/Montevideo",
		// (GMT-3:00)
		"America/Paramaribo",
		// (GMT-3:00)
		"America/Recife",
		// (GMT-3:00)
		"America/Rosario",
		// (GMT-3:00)
		"America/Santarem",
		// (GMT-3:00)
		"America/Sao_Paulo",
		// (GMT-3:00)
		"Antarctica/Rothera",
		// (GMT-3:00)
		"Atlantic/Stanley",
		// (GMT-3:00)
		"BET",
		// (GMT-3:00)
		"Brazil/East",
		// (GMT-3:00)
		"Etc/GMT+3",
		// (GMT-2:00)
		"America/Noronha",
		// (GMT-2:00)
		"Atlantic/South_Georgia",
		// (GMT-2:00)
		"Brazil/DeNoronha",
		// (GMT-2:00)
		"Etc/GMT+2",
		// (GMT-1:00)
		"America/Scoresbysund",
		// (GMT-1:00)
		"Atlantic/Azores",
		// (GMT-1:00)
		"Atlantic/Cape_Verde",
		// (GMT-1:00)
		"Etc/GMT+1",
		// (GMT0:00)
		"Africa/Abidjan",
		// (GMT0:00)
		"Africa/Accra",
		// (GMT0:00)
		"Africa/Bamako",
		// (GMT0:00)
		"Africa/Banjul",
		// (GMT0:00)
		"Africa/Bissau",
		// (GMT0:00)
		"Africa/Casablanca",
		// (GMT0:00)
		"Africa/Conakry",
		// (GMT0:00)
		"Africa/Dakar",
		// (GMT0:00)
		"Africa/El_Aaiun",
		// (GMT0:00)
		"Africa/Freetown",
		// (GMT0:00)
		"Africa/Lome",
		// (GMT0:00)
		"Africa/Monrovia",
		// (GMT0:00)
		"Africa/Nouakchott",
		// (GMT0:00)
		"Africa/Ouagadougou",
		// (GMT0:00)
		"Africa/Sao_Tome",
		// (GMT0:00)
		"Africa/Timbuktu",
		// (GMT0:00)
		"America/Danmarkshavn",
		// (GMT0:00)
		"Antarctica/Troll",
		// (GMT0:00)
		"Atlantic/Canary",
		// (GMT0:00)
		"Atlantic/Faeroe",
		// (GMT0:00)
		"Atlantic/Faroe",
		// (GMT0:00)
		"Atlantic/Madeira",
		// (GMT0:00)
		"Atlantic/Reykjavik",
		// (GMT0:00)
		"Atlantic/St_Helena",
		// (GMT0:00)
		"Eire",
		// (GMT0:00)
		"Etc/GMT",
		// (GMT0:00)
		"Etc/GMT+0",
		// (GMT0:00)
		"Etc/GMT-0",
		// (GMT0:00)
		"Etc/GMT0",
		// (GMT0:00)
		"Etc/Greenwich",
		// (GMT0:00)
		"Etc/UCT",
		// (GMT0:00)
		"Etc/UTC",
		// (GMT0:00)
		"Etc/Universal",
		// (GMT0:00)
		"Etc/Zulu",
		// (GMT0:00)
		"Europe/Belfast",
		// (GMT0:00)
		"Europe/Dublin",
		// (GMT0:00)
		"Europe/Guernsey",
		// (GMT0:00)
		"Europe/Isle_of_Man",
		// (GMT0:00)
		"Europe/Jersey",
		// (GMT0:00)
		"Europe/Lisbon",
		// (GMT0:00)
		"Europe/London",
		// (GMT0:00)
		"GB",
		// (GMT0:00)
		"GB-Eire",
		// (GMT0:00)
		"GMT",
		// (GMT0:00)
		"GMT0",
		// (GMT0:00)
		"Greenwich",
		// (GMT0:00)
		"Iceland",
		// (GMT0:00)
		"Portugal",
		// (GMT0:00)
		"UCT",
		// (GMT0:00)
		"UTC",
		// (GMT0:00)
		"Universal",
		// (GMT0:00)
		"WET",
		// (GMT0:00)
		"Zulu",
		// (GMT+1:00)
		"Africa/Algiers",
		// (GMT+1:00)
		"Africa/Bangui",
		// (GMT+1:00)
		"Africa/Brazzaville",
		// (GMT+1:00)
		"Africa/Ceuta",
		// (GMT+1:00)
		"Africa/Douala",
		// (GMT+1:00)
		"Africa/Kinshasa",
		// (GMT+1:00)
		"Africa/Lagos",
		// (GMT+1:00)
		"Africa/Libreville",
		// (GMT+1:00)
		"Africa/Luanda",
		// (GMT+1:00)
		"Africa/Malabo",
		// (GMT+1:00)
		"Africa/Ndjamena",
		// (GMT+1:00)
		"Africa/Niamey",
		// (GMT+1:00)
		"Africa/Porto-Novo",
		// (GMT+1:00)
		"Africa/Tunis",
		// (GMT+1:00)
		"Africa/Windhoek",
		// (GMT+1:00)
		"Arctic/Longyearbyen",
		// (GMT+1:00)
		"Atlantic/Jan_Mayen",
		// (GMT+1:00)
		"CET",
		// (GMT+1:00)
		"ECT",
		// (GMT+1:00)
		"Etc/GMT-1",
		// (GMT+1:00)
		"Europe/Amsterdam",
		// (GMT+1:00)
		"Europe/Andorra",
		// (GMT+1:00)
		"Europe/Belgrade",
		// (GMT+1:00)
		"Europe/Berlin",
		// (GMT+1:00)
		"Europe/Bratislava",
		// (GMT+1:00)
		"Europe/Brussels",
		// (GMT+1:00)
		"Europe/Budapest",
		// (GMT+1:00)
		"Europe/Busingen",
		// (GMT+1:00)
		"Europe/Copenhagen",
		// (GMT+1:00)
		"Europe/Gibraltar",
		// (GMT+1:00)
		"Europe/Ljubljana",
		// (GMT+1:00)
		"Europe/Luxembourg",
		// (GMT+1:00)
		"Europe/Madrid",
		// (GMT+1:00)
		"Europe/Malta",
		// (GMT+1:00)
		"Europe/Monaco",
		// (GMT+1:00)
		"Europe/Oslo",
		// (GMT+1:00)
		"Europe/Paris",
		// (GMT+1:00)
		"Europe/Podgorica",
		// (GMT+1:00)
		"Europe/Prague",
		// (GMT+1:00)
		"Europe/Rome",
		// (GMT+1:00)
		"Europe/San_Marino",
		// (GMT+1:00)
		"Europe/Sarajevo",
		// (GMT+1:00)
		"Europe/Skopje",
		// (GMT+1:00)
		"Europe/Stockholm",
		// (GMT+1:00)
		"Europe/Tirane",
		// (GMT+1:00)
		"Europe/Vaduz",
		// (GMT+1:00)
		"Europe/Vatican",
		// (GMT+1:00)
		"Europe/Vienna",
		// (GMT+1:00)
		"Europe/Warsaw",
		// (GMT+1:00)
		"Europe/Zagreb",
		// (GMT+1:00)
		"Europe/Zurich",
		// (GMT+1:00)
		"MET",
		// (GMT+1:00)
		"Poland",
		// (GMT+2:00)
		"ART",
		// (GMT+2:00)
		"Africa/Blantyre",
		// (GMT+2:00)
		"Africa/Bujumbura",
		// (GMT+2:00)
		"Africa/Cairo",
		// (GMT+2:00)
		"Africa/Gaborone",
		// (GMT+2:00)
		"Africa/Harare",
		// (GMT+2:00)
		"Africa/Johannesburg",
		// (GMT+2:00)
		"Africa/Kigali",
		// (GMT+2:00)
		"Africa/Lubumbashi",
		// (GMT+2:00)
		"Africa/Lusaka",
		// (GMT+2:00)
		"Africa/Maputo",
		// (GMT+2:00)
		"Africa/Maseru",
		// (GMT+2:00)
		"Africa/Mbabane",
		// (GMT+2:00)
		"Africa/Tripoli",
		// (GMT+2:00)
		"Asia/Amman",
		// (GMT+2:00)
		"Asia/Beirut",
		// (GMT+2:00)
		"Asia/Damascus",
		// (GMT+2:00)
		"Asia/Gaza",
		// (GMT+2:00)
		"Asia/Hebron",
		// (GMT+2:00)
		"Asia/Istanbul",
		// (GMT+2:00)
		"Asia/Jerusalem",
		// (GMT+2:00)
		"Asia/Nicosia",
		// (GMT+2:00)
		"Asia/Tel_Aviv",
		// (GMT+2:00)
		"CAT",
		// (GMT+2:00)
		"EET",
		// (GMT+2:00)
		"Egypt",
		// (GMT+2:00)
		"Etc/GMT-2",
		// (GMT+2:00)
		"Europe/Athens",
		// (GMT+2:00)
		"Europe/Bucharest",
		// (GMT+2:00)
		"Europe/Chisinau",
		// (GMT+2:00)
		"Europe/Helsinki",
		// (GMT+2:00)
		"Europe/Istanbul",
		// (GMT+2:00)
		"Europe/Kiev",
		// (GMT+2:00)
		"Europe/Mariehamn",
		// (GMT+2:00)
		"Europe/Nicosia",
		// (GMT+2:00)
		"Europe/Riga",
		// (GMT+2:00)
		"Europe/Sofia",
		// (GMT+2:00)
		"Europe/Tallinn",
		// (GMT+2:00)
		"Europe/Tiraspol",
		// (GMT+2:00)
		"Europe/Uzhgorod",
		// (GMT+2:00)
		"Europe/Vilnius",
		// (GMT+2:00)
		"Europe/Zaporozhye",
		// (GMT+2:00)
		"Israel",
		// (GMT+2:00)
		"Libya",
		// (GMT+2:00)
		"Turkey",
		// (GMT+3:00)
		"Africa/Addis_Ababa",
		// (GMT+3:00)
		"Africa/Asmara",
		// (GMT+3:00)
		"Africa/Asmera",
		// (GMT+3:00)
		"Africa/Dar_es_Salaam",
		// (GMT+3:00)
		"Africa/Djibouti",
		// (GMT+3:00)
		"Africa/Juba",
		// (GMT+3:00)
		"Africa/Kampala",
		// (GMT+3:00)
		"Africa/Khartoum",
		// (GMT+3:00)
		"Africa/Mogadishu",
		// (GMT+3:00)
		"Africa/Nairobi",
		// (GMT+3:00)
		"Antarctica/Syowa",
		// (GMT+3:00)
		"Asia/Aden",
		// (GMT+3:00)
		"Asia/Baghdad",
		// (GMT+3:00)
		"Asia/Bahrain",
		// (GMT+3:00)
		"Asia/Kuwait",
		// (GMT+3:00)
		"Asia/Qatar",
		// (GMT+3:00)
		"Asia/Riyadh",
		// (GMT+3:00)
		"EAT",
		// (GMT+3:00)
		"Etc/GMT-3",
		// (GMT+3:00)
		"Europe/Kaliningrad",
		// (GMT+3:00)
		"Europe/Minsk",
		// (GMT+3:00)
		"Indian/Antananarivo",
		// (GMT+3:00)
		"Indian/Comoro",
		// (GMT+3:00)
		"Indian/Mayotte",
		// (GMT+3:07)
		"Asia/Riyadh87",
		// (GMT+3:07)
		"Asia/Riyadh88",
		// (GMT+3:07)
		"Asia/Riyadh89",
		// (GMT+3:07)
		"Mideast/Riyadh87",
		// (GMT+3:07)
		"Mideast/Riyadh88",
		// (GMT+3:07)
		"Mideast/Riyadh89",
		// (GMT+3:30)
		"Asia/Tehran",
		// (GMT+3:30)
		"Iran",
		// (GMT+4:00)
		"Asia/Baku",
		// (GMT+4:00)
		"Asia/Dubai",
		// (GMT+4:00)
		"Asia/Muscat",
		// (GMT+4:00)
		"Asia/Tbilisi",
		// (GMT+4:00)
		"Asia/Yerevan",
		// (GMT+4:00)
		"Etc/GMT-4",
		// (GMT+4:00)
		"Europe/Moscow",
		// (GMT+4:00)
		"Europe/Samara",
		// (GMT+4:00)
		"Europe/Simferopol",
		// (GMT+4:00)
		"Europe/Volgograd",
		// (GMT+4:00)
		"Indian/Mahe",
		// (GMT+4:00)
		"Indian/Mauritius",
		// (GMT+4:00)
		"Indian/Reunion",
		// (GMT+4:00)
		"NET",
		// (GMT+4:00)
		"W-SU",
		// (GMT+4:30)
		"Asia/Kabul",
		// (GMT+5:00)
		"Antarctica/Mawson",
		// (GMT+5:00)
		"Asia/Aqtau",
		// (GMT+5:00)
		"Asia/Aqtobe",
		// (GMT+5:00)
		"Asia/Ashgabat",
		// (GMT+5:00)
		"Asia/Ashkhabad",
		// (GMT+5:00)
		"Asia/Dushanbe",
		// (GMT+5:00)
		"Asia/Karachi",
		// (GMT+5:00)
		"Asia/Oral",
		// (GMT+5:00)
		"Asia/Samarkand",
		// (GMT+5:00)
		"Asia/Tashkent",
		// (GMT+5:00)
		"Etc/GMT-5",
		// (GMT+5:00)
		"Indian/Kerguelen",
		// (GMT+5:00)
		"Indian/Maldives",
		// (GMT+5:00)
		"PLT",
		// (GMT+5:30)
		"Asia/Calcutta",
		// (GMT+5:30)
		"Asia/Colombo",
		// (GMT+5:30)
		"Asia/Kolkata",
		// (GMT+5:30)
		"IST",
		// (GMT+5:45)
		"Asia/Kathmandu",
		// (GMT+5:45)
		"Asia/Katmandu",
		// (GMT+6:00)
		"Antarctica/Vostok",
		// (GMT+6:00)
		"Asia/Almaty",
		// (GMT+6:00)
		"Asia/Bishkek",
		// (GMT+6:00)
		"Asia/Dacca",
		// (GMT+6:00)
		"Asia/Dhaka",
		// (GMT+6:00)
		"Asia/Qyzylorda",
		// (GMT+6:00)
		"Asia/Thimbu",
		// (GMT+6:00)
		"Asia/Thimphu",
		// (GMT+6:00)
		"Asia/Yekaterinburg",
		// (GMT+6:00)
		"BST",
		// (GMT+6:00)
		"Etc/GMT-6",
		// (GMT+6:00)
		"Indian/Chagos",
		// (GMT+6:30)
		"Asia/Rangoon",
		// (GMT+6:30)
		"Indian/Cocos",
		// (GMT+7:00)
		"Antarctica/Davis",
		// (GMT+7:00)
		"Asia/Bangkok",
		// (GMT+7:00)
		"Asia/Ho_Chi_Minh",
		// (GMT+7:00)
		"Asia/Hovd",
		// (GMT+7:00)
		"Asia/Jakarta",
		// (GMT+7:00)
		"Asia/Novokuznetsk",
		// (GMT+7:00)
		"Asia/Novosibirsk",
		// (GMT+7:00)
		"Asia/Omsk",
		// (GMT+7:00)
		"Asia/Phnom_Penh",
		// (GMT+7:00)
		"Asia/Pontianak",
		// (GMT+7:00)
		"Asia/Saigon",
		// (GMT+7:00)
		"Asia/Vientiane",
		// (GMT+7:00)
		"Etc/GMT-7",
		// (GMT+7:00)
		"Indian/Christmas",
		// (GMT+7:00)
		"VST",
		// (GMT+8:00)
		"Antarctica/Casey",
		// (GMT+8:00)
		"Asia/Brunei",
		// (GMT+8:00)
		"Asia/Choibalsan",
		// (GMT+8:00)
		"Asia/Chongqing",
		// (GMT+8:00)
		"Asia/Chungking",
		// (GMT+8:00)
		"Asia/Harbin",
		// (GMT+8:00)
		"Asia/Hong_Kong",
		// (GMT+8:00)
		"Asia/Kashgar",
		// (GMT+8:00)
		"Asia/Krasnoyarsk",
		// (GMT+8:00)
		"Asia/Kuala_Lumpur",
		// (GMT+8:00)
		"Asia/Kuching",
		// (GMT+8:00)
		"Asia/Macao",
		// (GMT+8:00)
		"Asia/Macau",
		// (GMT+8:00)
		"Asia/Makassar",
		// (GMT+8:00)
		"Asia/Manila",
		// (GMT+8:00)
		"Asia/Shanghai",
		// (GMT+8:00)
		"Asia/Singapore",
		// (GMT+8:00)
		"Asia/Taipei",
		// (GMT+8:00)
		"Asia/Ujung_Pandang",
		// (GMT+8:00)
		"Asia/Ulaanbaatar",
		// (GMT+8:00)
		"Asia/Ulan_Bator",
		// (GMT+8:00)
		"Asia/Urumqi",
		// (GMT+8:00)
		"Australia/Perth",
		// (GMT+8:00)
		"Australia/West",
		// (GMT+8:00)
		"CTT",
		// (GMT+8:00)
		"Etc/GMT-8",
		// (GMT+8:00)
		"Hongkong",
		// (GMT+8:00)
		"PRC",
		// (GMT+8:00)
		"Singapore",
		// (GMT+8:45)
		"Australia/Eucla",
		// (GMT+9:00)
		"Asia/Dili",
		// (GMT+9:00)
		"Asia/Irkutsk",
		// (GMT+9:00)
		"Asia/Jayapura",
		// (GMT+9:00)
		"Asia/Pyongyang",
		// (GMT+9:00)
		"Asia/Seoul",
		// (GMT+9:00)
		"Asia/Tokyo",
		// (GMT+9:00)
		"Etc/GMT-9",
		// (GMT+9:00)
		"JST",
		// (GMT+9:00)
		"Japan",
		// (GMT+9:00)
		"Pacific/Palau",
		// (GMT+9:00)
		"ROK",
		// (GMT+9:30)
		"ACT",
		// (GMT+9:30)
		"Australia/Adelaide",
		// (GMT+9:30)
		"Australia/Broken_Hill",
		// (GMT+9:30)
		"Australia/Darwin",
		// (GMT+9:30)
		"Australia/North",
		// (GMT+9:30)
		"Australia/South",
		// (GMT+9:30)
		"Australia/Yancowinna",
		// (GMT+10:00)
		"AET",
		// (GMT+10:00)
		"Antarctica/DumontDUrville",
		// (GMT+10:00)
		"Asia/Khandyga",
		// (GMT+10:00)
		"Asia/Yakutsk",
		// (GMT+10:00)
		"Australia/ACT",
		// (GMT+10:00)
		"Australia/Brisbane",
		// (GMT+10:00)
		"Australia/Canberra",
		// (GMT+10:00)
		"Australia/Currie",
		// (GMT+10:00)
		"Australia/Hobart",
		// (GMT+10:00)
		"Australia/Lindeman",
		// (GMT+10:00)
		"Australia/Melbourne",
		// (GMT+10:00)
		"Australia/NSW",
		// (GMT+10:00)
		"Australia/Queensland",
		// (GMT+10:00)
		"Australia/Sydney",
		// (GMT+10:00)
		"Australia/Tasmania",
		// (GMT+10:00)
		"Australia/Victoria",
		// (GMT+10:00)
		"Etc/GMT-10",
		// (GMT+10:00)
		"Pacific/Chuuk",
		// (GMT+10:00)
		"Pacific/Guam",
		// (GMT+10:00)
		"Pacific/Port_Moresby",
		// (GMT+10:00)
		"Pacific/Saipan",
		// (GMT+10:00)
		"Pacific/Truk",
		// (GMT+10:00)
		"Pacific/Yap",
		// (GMT+10:30)
		"Australia/LHI",
		// (GMT+10:30)
		"Australia/Lord_Howe",
		// (GMT+11:00)
		"Antarctica/Macquarie",
		// (GMT+11:00)
		"Asia/Sakhalin",
		// (GMT+11:00)
		"Asia/Ust-Nera",
		// (GMT+11:00)
		"Asia/Vladivostok",
		// (GMT+11:00)
		"Etc/GMT-11",
		// (GMT+11:00)
		"Pacific/Efate",
		// (GMT+11:00)
		"Pacific/Guadalcanal",
		// (GMT+11:00)
		"Pacific/Kosrae",
		// (GMT+11:00)
		"Pacific/Noumea",
		// (GMT+11:00)
		"Pacific/Pohnpei",
		// (GMT+11:00)
		"Pacific/Ponape",
		// (GMT+11:00)
		"SST",
		// (GMT+11:30)
		"Pacific/Norfolk",
		// (GMT+12:00)
		"Antarctica/McMurdo",
		// (GMT+12:00)
		"Antarctica/South_Pole",
		// (GMT+12:00)
		"Asia/Anadyr",
		// (GMT+12:00)
		"Asia/Kamchatka",
		// (GMT+12:00)
		"Asia/Magadan",
		// (GMT+12:00)
		"Etc/GMT-12",
		// (GMT+12:00)
		"Kwajalein",
		// (GMT+12:00)
		"NST",
		// (GMT+12:00)
		"NZ",
		// (GMT+12:00)
		"Pacific/Auckland",
		// (GMT+12:00)
		"Pacific/Fiji",
		// (GMT+12:00)
		"Pacific/Funafuti",
		// (GMT+12:00)
		"Pacific/Kwajalein",
		// (GMT+12:00)
		"Pacific/Majuro",
		// (GMT+12:00)
		"Pacific/Nauru",
		// (GMT+12:00)
		"Pacific/Tarawa",
		// (GMT+12:00)
		"Pacific/Wake",
		// (GMT+12:00)
		"Pacific/Wallis",
		// (GMT+12:45)
		"NZ-CHAT",
		// (GMT+12:45)
		"Pacific/Chatham",
		// (GMT+13:00)
		"Etc/GMT-13",
		// (GMT+13:00)
		"MIT",
		// (GMT+13:00)
		"Pacific/Apia",
		// (GMT+13:00)
		"Pacific/Enderbury",
		// (GMT+13:00)
		"Pacific/Fakaofo",
		// (GMT+13:00)
		"Pacific/Tongatapu",
		// (GMT+14:00)
		"Etc/GMT-14",
		// (GMT+14:00)
		"Pacific/Kiritimati"
    };

	set<string>::iterator itr = timezones.find(timezone);
    
	return itr != timezones.end();
}

