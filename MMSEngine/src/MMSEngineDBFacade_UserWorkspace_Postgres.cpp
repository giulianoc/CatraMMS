
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
			"select w.workspaceKey, w.name, w.directoryName, w.maxStorageInMB, w.maxEncodingPriority, "
			"wc.maxStorageInGB, wc.currentCostForStorage, "
			"wc.dedicatedEncoder_power_1, wc.currentCostForDedicatedEncoder_power_1, "
			"wc.dedicatedEncoder_power_2, wc.currentCostForDedicatedEncoder_power_2, "
			"wc.dedicatedEncoder_power_3, wc.currentCostForDedicatedEncoder_power_3, "
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
			workspace->_maxStorageInMB = res[0]["maxStorageInMB"].as<int>();
			workspace->_maxEncodingPriority = static_cast<int>(toEncodingPriority(res[0]["maxEncodingPriority"].as<string>()));

			workspace->_maxStorageInGB = res[0]["maxStorageInGB"].as<int>();
			workspace->_currentCostForStorage = res[0]["currentCostForStorage"].as<int>();
			workspace->_dedicatedEncoder_power_1 = res[0]["dedicatedEncoder_power_1"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_1 = res[0]["currentCostForDedicatedEncoder_power_1"].as<int>();
			workspace->_dedicatedEncoder_power_2 = res[0]["dedicatedEncoder_power_2"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_2 = res[0]["currentCostForDedicatedEncoder_power_2"].as<int>();
			workspace->_dedicatedEncoder_power_3 = res[0]["dedicatedEncoder_power_3"].as<int>();
			workspace->_currentCostForDedicatedEncoder_power_3 = res[0]["currentCostForDedicatedEncoder_power_3"].as<int>();
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
			"select w.workspaceKey, w.name, w.directoryName, w.maxStorageInMB, w.maxEncodingPriority "
			"wc.maxStorageInGB, wc.currentCostForStorage, "
			"wc.dedicatedEncoder_power_1, wc.currentCostForDedicatedEncoder_power_1, "
			"wc.dedicatedEncoder_power_2, wc.currentCostForDedicatedEncoder_power_2, "
			"wc.dedicatedEncoder_power_3, wc.currentCostForDedicatedEncoder_power_3, "
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
			workspace->_maxStorageInMB = res[0]["maxStorageInMB"].as<int>();
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
                "insert into MMS_User (name, eMailAddress, password, country, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, NOW() at time zone 'utc', false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"NULL) returning userKey",
				trans.quote(trimUserName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry),
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
                "insert into MMS_User (name, eMailAddress, password, country, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, NOW() at time zone 'utc', "
				"false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), NULL) returning userKey",
				trans.quote(trimUserName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry),
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
				trans->exec1(sqlStatement);
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
                "insert into MMS_User (name, eMailAddress, password, country, "
				"creationDate, insolvent, expirationDate, lastSuccessfulLogin) values ("
                "{}, {}, {}, {}, NOW() at time zone 'utc', false, to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'), "
				"NULL) returning userKey",
				trans.quote(userName), trans.quote(userEmailAddress),
				trans.quote(userPassword), trans.quote(userCountry),
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
			trans->exec1(sqlStatement);
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
				"creationDate, name, directoryName, workspaceType, "
				"deliveryURL, enabled, maxEncodingPriority, encodingPeriod, "
				"maxIngestionsNumber, maxStorageInMB, languageCode) values ("
				"NOW() at time zone 'utc',         {},    {},             {}, "
				"{},           {},         {},                   {}, "
				"{},                   {},            {}) returning workspaceKey",
				trans.quote(workspaceName), trans.quote(workspaceDirectoryName),
				static_cast<int>(workspaceType),
				deliveryURL == "" ? "null" : trans.quote(deliveryURL),
				enabled, trans.quote(toString(maxEncodingPriority)),
				trans.quote(toString(encodingPeriod)), maxIngestionsNumber, maxStorageInMB,
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
					"support_type_1, currentCostForSupport_type_1 "
					") values ("
					"{},           {},             0, "
					"0,                        0, "
					"0,                        0, "
					"0,                        0, "
					"false,          0) ",
					workspaceKey, maxStorageInMB / 1000);
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
			trans.exec1(sqlStatement);
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
			trans.exec1(sqlStatement);
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
                "                        {},      {},       {},            {},       {}, "
				"{},           NOW() at time zone 'utc',        to_timestamp({}, 'YYYY-MM-DD HH24:MI:SS'))",
				trans.quote(apiKey), userKey, workspaceKey, isOwner, isDefault,
				trans.quote(permissions), trans.quote(strExpirationUtcDate));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			trans.exec1(sqlStatement);
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
			trans->exec1(sqlStatement);
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
    
    // return workspaceKeyUserKeyAndConfirmationCode;
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
					"select userKey, name, country, insolvent, "
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
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
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
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
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
				"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
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
			bool costDetails = false;
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
					"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, w.languageCode, "
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
				bool userAPIKeyInfo = true;
				bool costDetails = false;
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

		field = "maxStorageInMB";
		workspaceDetailRoot[field] = row["maxStorageInMB"].as<int>();

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
	bool maxStorageInMBChanged, int64_t newMaxStorageInMB,
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

			if (maxStorageInMBChanged)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += fmt::format("maxStorageInMB = {}", newMaxStorageInMB);
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
                        + ", newMaxStorageInMB: " + to_string(newMaxStorageInMB)
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
				"w.encodingPeriod, w.maxIngestionsNumber, w.maxStorageInMB, "
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
                "select maxStorageInMB from MMS_Workspace where workspaceKey = {}",
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
                maxStorageInMB = res[0]["maxStorageInMB"].as<int>();
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

pair<int64_t, string> MMSEngineDBFacade::getUserDetailsByEmail (string email)
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
				"userKey, name, eMailAddress, country, insolvent "
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
			trans.exec1(sqlStatement);
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

int64_t MMSEngineDBFacade::saveLoginStatistics(
	int userKey, string ip,
	string continent, string continentCode, string country, string countryCode,
	string region, string city, string org, string isp, int timezoneGMTOffset
)
{
	int64_t loginStatisticKey;
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
				"insert into MMS_LoginStatistic (userKey, ip, continent, continentCode, "
				"country, countryCode, region, city, org, isp, timezoneGMTOffset, successfulLogin) values ("
				                             "{},       {},  {},         {}, "
			    "{},       {},           {},      {},    {},   {},   {},          NOW() at time zone 'utc') "
				"returning loginStatisticKey",
				userKey,
				ip == "" ? "null" : trans.quote(ip),
				continent == "" ? "null" : trans.quote(continent),
				continentCode == "" ? "null" : trans.quote(continentCode),
				country == "" ? "null" : trans.quote(country),
				countryCode == "" ? "null" : trans.quote(countryCode),
				region == "" ? "null" : trans.quote(region),
				city == "" ? "null" : trans.quote(city),
				org == "" ? "null" : trans.quote(org),
				isp == "" ? "null" : trans.quote(isp),
				timezoneGMTOffset == -1 ? "null" : to_string(timezoneGMTOffset));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			loginStatisticKey = trans.exec1(sqlStatement)[0].as<int64_t>();
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

	return loginStatisticKey;
}

