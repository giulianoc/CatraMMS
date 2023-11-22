
#include "MMSEngineDBFacade.h"
#include "JSONUtils.h"
#include "catralibraries/StringUtils.h"


void MMSEngineDBFacade::getExpiredMediaItemKeysCheckingDependencies(
        string processorMMS,
        vector<tuple<shared_ptr<Workspace>,int64_t, int64_t>>& mediaItemKeyOrPhysicalPathKeyToBeRemoved,
        int maxEntriesNumber)
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
		// 2021-09-23: I removed TRANSACTION and FOR UPDATE because I saw we may have deadlock when a MediaItem is added

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies (MediaItemKeys expired)"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);

		// 1. MediaItemKeys expired
        int start = 0;
        bool noMoreRowsReturned = false;
        while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber &&
                !noMoreRowsReturned)
        {
            string sqlStatement = fmt::format( 
				"select workspaceKey, mediaItemKey, ingestionJobKey, retentionInMinutes, title, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem where "
				"ingestionDate + INTERVAL '1 minute' * retentionInMinutes < NOW() at time zone 'utc' "
				"and processorMMSForRetention is null "
				"limit {} offset {}",	// for update"; see comment marked as 2021-09-23
				maxEntriesNumber, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
            noMoreRowsReturned = true;
            start += maxEntriesNumber;
			for (auto row: res)
            {
                noMoreRowsReturned = false;
                
                int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
                int64_t workspaceKey = row["workspaceKey"].as<int64_t>();
                int64_t mediaItemKey = row["mediaItemKey"].as<int64_t>();
                int64_t retentionInMinutes = row["retentionInMinutes"].as<int64_t>();
                string ingestionDate = row["ingestionDate"].as<string>();
                string title = row["title"].as<string>();
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(conn, trans, ingestionJobKey)
						> 0)
					ingestionDependingOnMediaItemKey = true;

                if (!ingestionDependingOnMediaItemKey)
                {
                    {
                        string sqlStatement = fmt::format( 
                            "WITH rows AS (update MMS_MediaItem set processorMMSForRetention = {} "
							"where mediaItemKey = {} and processorMMSForRetention is null "
							"returning 1) select count(*) from rows",
							trans.quote(processorMMS), mediaItemKey);
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
							// may be another processor doing the same activity updates it
							// Really it should never happen because of the 'for update'
							// 2021-09-23: we do not have for update anymore

							continue;
							/*
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", processorMMS: " + processorMMS
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", sqlStatement: " + sqlStatement
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
							*/
                        }
                    }

                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    tuple<shared_ptr<Workspace>,int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey =
                            make_tuple(workspace, mediaItemKey, -1);

                    mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
                }
                else
                {
                    _logger->info(__FILEREF__ + "Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
                        + "ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", title: " + title
                        + ", ingestionDate: " + ingestionDate
                        + ", retentionInMinutes: " + to_string(retentionInMinutes)
                    );
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

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies (PhysicalPathKeys expired)"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);

		// 1. PhysicalPathKeys expired
        start = 0;
        noMoreRowsReturned = false;
        while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber &&
                !noMoreRowsReturned)
        {
            string sqlStatement = fmt::format( 
				"select mi.workspaceKey, mi.mediaItemKey, p.physicalPathKey, mi.ingestionJobKey, "
				"p.retentionInMinutes, mi.title, "
				"to_char(mi.ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p where "
				"mi.mediaItemKey = p.mediaItemKey "
				"and p.retentionInMinutes is not null "
				// PhysicalPathKey expired
				"and mi.ingestionDate + INTERVAL '1 minute' * p.retentionInMinutes < NOW() at time zone 'utc' "
				// MediaItemKey not expired
				"and mi.ingestionDate + INTERVAL '1 minute' * mi.retentionInMinutes > NOW() at time zone 'utc' "
				"and processorMMSForRetention is null "
				"limit {} offset {}",	// for update"; see comment marked as 2021-09-23
				maxEntriesNumber, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
            noMoreRowsReturned = true;
            start += maxEntriesNumber;
			for (auto row: res)
            {
                noMoreRowsReturned = false;
                
                int64_t ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
                int64_t workspaceKey = row["workspaceKey"].as<int64_t>();
                int64_t mediaItemKey = row["mediaItemKey"].as<int64_t>();
                int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();
                int64_t physicalPathKeyRetentionInMinutes = row["retentionInMinutes"].as<int64_t>();
                string ingestionDate = row["ingestionDate"].as<string>();
                string title = row["title"].as<string>();
                
                // check if there is still an ingestion depending on the ingestionJobKey
                bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(conn, trans, ingestionJobKey)
						> 0)
					ingestionDependingOnMediaItemKey = true;

                if (!ingestionDependingOnMediaItemKey)
                {
                    {
                        string sqlStatement = fmt::format( 
                            "WITH rows AS (update MMS_MediaItem set processorMMSForRetention = {} "
							"where mediaItemKey = {} and processorMMSForRetention is null "
							"returning 1) select count(*) from rows",
							trans.quote(processorMMS), mediaItemKey);
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
							// may be another processor doing the same activity updates it
							// Really it should never happen because of the 'for update'
							// 2021-09-23: we do not have for update anymore

							continue;
							/*
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", processorMMS: " + processorMMS
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", sqlStatement: " + sqlStatement
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
							*/
                        }
                    }

                    shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

                    tuple<shared_ptr<Workspace>,int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey =
                            make_tuple(workspace, mediaItemKey, physicalPathKey);

                    mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
                }
                else
                {
                    _logger->info(__FILEREF__ + "Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
                        + "ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", title: " + title
                        + ", ingestionDate: " + ingestionDate
                        + ", physicalPathKeyRetentionInMinutes: " + to_string(physicalPathKeyRetentionInMinutes)
                    );
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

		_logger->info(__FILEREF__
			+ "getExpiredMediaItemKeysCheckingDependencies"
			+ ", processorMMS: " + processorMMS
			+ ", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: "
				+ to_string(mediaItemKeyOrPhysicalPathKeyToBeRemoved.size())
			+ ", maxEntriesNumber: " + to_string(maxEntriesNumber)
		);

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

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
	int64_t ingestionJobKey, bool fromMaster
)
{
    int			dependenciesNumber;
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
		dependenciesNumber = getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
			conn, trans, ingestionJobKey);

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

    return dependenciesNumber;
}

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(
	shared_ptr<PostgresConnection> conn, nontransaction& trans,
	int64_t ingestionJobKey
)
{
    int			dependenciesNumber;

    try
    {
		{
			string sqlStatement = fmt::format( 
				"select count(*) from MMS_IngestionJobDependency ijd, MMS_IngestionJob ij where "
				"ijd.ingestionJobKey = ij.ingestionJobKey "
				"and ijd.dependOnIngestionJobKey = {} "
				"and ij.status not like 'End_%'",
				ingestionJobKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			dependenciesNumber = trans.exec1(sqlStatement)[0].as<int>();
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return dependenciesNumber;
}

Json::Value MMSEngineDBFacade::updateMediaItem (
	int64_t workspaceKey,
	int64_t mediaItemKey,
	bool titleModified, string newTitle,
	bool userDataModified, string newUserData,
	bool retentionInMinutesModified, int64_t newRetentionInMinutes,
	bool tagsModified, Json::Value tagsRoot,
	bool uniqueNameModified, string newUniqueName,
	bool admin
	)
{
    Json::Value mediaItemRoot;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

    try
    {
		if (titleModified || userDataModified || retentionInMinutesModified || tagsModified)
        {
			string setSQL;

			if (titleModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("title = {}", trans.quote(newTitle));
			}

			if (userDataModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				if (newUserData == "")
					setSQL += ("userData = null");
				else
					setSQL += fmt::format("userData = {}", trans.quote(newUserData));
			}

			if (retentionInMinutesModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("retentionInMinutes = {}", newRetentionInMinutes);
			}

			if (tagsModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += fmt::format("tags = {}", getPostgresArray(tagsRoot, &trans));
			}

			setSQL = "set " + setSQL + " ";

			string sqlStatement = fmt::format( 
				"WITH rows AS (update MMS_MediaItem {} "
				"where mediaItemKey = {} "
				// 2021-02: in case the user is not the owner and it is a shared workspace
				//		the workspacekey will not match
				// 2021-03: I think the above comment is wrong, the user, in that case,
				//		will use an APIKey of the correct workspace, even if this is shared.
				//		So let's add again the below sql condition
				"and workspaceKey = {} "
				"returning 1) select count(*) from rows",
				setSQL, mediaItemKey, workspaceKey);
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
						+ ", workspaceKey: " + to_string(workspaceKey)
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", newTitle: " + newTitle
						+ ", newUserData: " + newUserData
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
            }
			*/
		}

		if (uniqueNameModified)
        {
			bool allowUniqueNameOverride = false;

			manageExternalUniqueName(
				conn, &trans, workspaceKey,
				mediaItemKey,

				allowUniqueNameOverride,
				newUniqueName
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
    
	string uniqueName;
	int64_t physicalPathKey = -1;
	vector<int64_t> otherMediaItemsKey;
	int start = 0;
	int rows = 1;
	bool contentTypePresent = false;
	ContentType contentType;
	// bool startAndEndIngestionDatePresent = false;
	string startIngestionDate;
	string endIngestionDate;
	string title;
	int liveRecordingChunk = -1;
	int64_t recordingCode = -1;
	int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
	int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
	string jsonCondition;
	vector<string> tagsIn;
	vector<string> tagsNotIn;
	string orderBy;
	string jsonOrderBy;
	Json::Value responseFields = Json::nullValue;

	Json::Value mediaItemsListRoot = getMediaItemsList (
		workspaceKey, mediaItemKey, uniqueName, physicalPathKey,
		otherMediaItemsKey,
        start, rows,
        contentTypePresent, contentType,
        // startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate,
        title, liveRecordingChunk,
		recordingCode,
		utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		jsonCondition,
		tagsIn, tagsNotIn,
        orderBy,
		jsonOrderBy,
		responseFields,
		admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true);

    return mediaItemsListRoot;
}

Json::Value MMSEngineDBFacade::updatePhysicalPath (
	int64_t workspaceKey,
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	int64_t newRetentionInMinutes,
	bool admin
	)
{
    Json::Value mediaItemRoot;
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
                "WITH rows AS (update MMS_PhysicalPath set retentionInMinutes = {} "
                "where physicalPathKey = {} and mediaItemKey = {} "
				"returning 1) select count(*) from rows",
				newRetentionInMinutes == -1 ? "null" : to_string(newRetentionInMinutes),
				physicalPathKey, mediaItemKey);
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
                        + ", physicalPathKey: " + to_string(physicalPathKey)
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);
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
    
	string uniqueName;
	int64_t localPhysicalPathKey = -1;
	vector<int64_t> otherMediaItemsKey;
	int start = 0;
	int rows = 1;
	bool contentTypePresent = false;
	ContentType contentType;
	// bool startAndEndIngestionDatePresent = false;
	string startIngestionDate;
	string endIngestionDate;
	string title;
	int liveRecordingChunk = -1;
	int64_t recordingCode = -1;
	int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
	int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
	string jsonCondition;
	vector<string> tagsIn;
	vector<string> tagsNotIn;
	string orderBy;
	string jsonOrderBy;
	Json::Value responseFields = Json::nullValue;

	Json::Value mediaItemsListRoot = getMediaItemsList (
		workspaceKey, mediaItemKey, uniqueName, localPhysicalPathKey,
		otherMediaItemsKey,
        start, rows,
        contentTypePresent, contentType,
        // startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate,
        title, liveRecordingChunk,
		recordingCode,
		utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		jsonCondition,
		tagsIn, tagsNotIn,
        orderBy,
		jsonOrderBy,
        responseFields,
		admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true);

    return mediaItemsListRoot;
}

Json::Value MMSEngineDBFacade::getMediaItemsList (
        int64_t workspaceKey, int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey,
		vector<int64_t>& otherMediaItemsKey,
        int start, int rows,
        bool contentTypePresent, ContentType contentType,
        // bool startAndEndIngestionDatePresent,
		string startIngestionDate, string endIngestionDate,
        string title, int liveRecordingChunk,
		int64_t recordingCode,
		int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
		string jsonCondition,
		vector<string>& tagsIn, vector<string>& tagsNotIn,
        string orderBy,			// i.e.: "", mi.ingestionDate desc, mi.title asc
		string jsonOrderBy,		// i.e.: "", JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc
        Json::Value responseFields,
		bool admin,
		bool fromMaster
)
{
    Json::Value mediaItemsListRoot;

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
        string field;
        
        _logger->info(__FILEREF__ + "getMediaItemsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", mediaItemKey: " + to_string(mediaItemKey)
            + ", uniqueName: " + uniqueName
            + ", physicalPathKey: " + to_string(physicalPathKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
            // + ", startAndEndIngestionDatePresent: " + to_string(startAndEndIngestionDatePresent)
            + ", startIngestionDate: " + startIngestionDate
            + ", endIngestionDate: " + endIngestionDate
            + ", title: " + title
            + ", tagsIn.size(): " + to_string(tagsIn.size())
            + ", tagsNotIn.size(): " + to_string(tagsNotIn.size())
            + ", otherMediaItemsKey.size(): " + to_string(otherMediaItemsKey.size())
            + ", liveRecordingChunk: " + to_string(liveRecordingChunk)
            + ", recordingCode: " + to_string(recordingCode)
            + ", jsonCondition: " + jsonCondition
            + ", orderBy: " + orderBy
            + ", jsonOrderBy: " + jsonOrderBy
        );
        
        {
            Json::Value requestParametersRoot;
            
            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            if (mediaItemKey != -1)
            {
                field = "mediaItemKey";
                requestParametersRoot[field] = mediaItemKey;
            }
            
            if (uniqueName != "")
            {
                field = "uniqueName";
                requestParametersRoot[field] = uniqueName;
            }
            
            if (physicalPathKey != -1)
            {
                field = "physicalPathKey";
                requestParametersRoot[field] = physicalPathKey;
            }

            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }
            
            if (startIngestionDate != "")
            {
                field = "startIngestionDate";
                requestParametersRoot[field] = startIngestionDate;
            }
            if (endIngestionDate != "")
            {
                field = "endIngestionDate";
                requestParametersRoot[field] = endIngestionDate;
            }
            
            if (title != "")
            {
                field = "title";
                requestParametersRoot[field] = title;
            }

            if (tagsIn.size() > 0)
			{
				Json::Value tagsRoot(Json::arrayValue);

				for (int tagIndex = 0; tagIndex < tagsIn.size(); tagIndex++)
					tagsRoot.append(tagsIn[tagIndex]);

                field = "tagsIn";
                requestParametersRoot[field] = tagsRoot;
			}

            if (tagsNotIn.size() > 0)
			{
				Json::Value tagsRoot(Json::arrayValue);

				for (int tagIndex = 0; tagIndex < tagsNotIn.size(); tagIndex++)
					tagsRoot.append(tagsNotIn[tagIndex]);

                field = "tagsNotIn";
                requestParametersRoot[field] = tagsRoot;
			}

            if (otherMediaItemsKey.size() > 0)
			{
				Json::Value otherMediaItemsKeyRoot(Json::arrayValue);

				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					otherMediaItemsKeyRoot.append(otherMediaItemsKey[mediaItemIndex]);

                field = "otherMediaItemsKey";
                requestParametersRoot[field] = otherMediaItemsKeyRoot;
			}

            if (liveRecordingChunk != -1)
            {
                field = "liveRecordingChunk";
                requestParametersRoot[field] = liveRecordingChunk;
            }

            if (jsonCondition != "")
            {
                field = "jsonCondition";
                requestParametersRoot[field] = jsonCondition;
            }

            if (recordingCode != -1)
            {
                field = "recordingCode";
                requestParametersRoot[field] = recordingCode;
            }

            if (orderBy != "")
            {
                field = "orderBy";
                requestParametersRoot[field] = orderBy;
            }

            if (jsonOrderBy != "")
            {
                field = "jsonOrderBy";
                requestParametersRoot[field] = jsonOrderBy;
            }

            field = "requestParameters";
            mediaItemsListRoot[field] = requestParametersRoot;
        }
        
        int64_t newMediaItemKey = mediaItemKey;
        if (mediaItemKey == -1)
        {
			if (physicalPathKey != -1)
			{
				string sqlStatement = fmt::format( 
					"select mediaItemKey from MMS_PhysicalPath where physicalPathKey = {}",
					physicalPathKey);
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
					newMediaItemKey = res[0]["mediaItemKey"].as<int64_t>();
				else
				{
					string errorMessage (__FILEREF__ + "getMediaItemsList: requested physicalPathKey does not exist"
						+ ", physicalPathKey: " + to_string(physicalPathKey)
						);
					_logger->error(errorMessage);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0;	// let's force a MIK that does not exist
				}
			}
			else if (uniqueName != "")
			{
				string sqlStatement = fmt::format(
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = {} and uniqueName = {}",
					workspaceKey, trans.quote(uniqueName));
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
					newMediaItemKey = res[0]["mediaItemKey"].as<int64_t>();
				else
				{
					string errorMessage (__FILEREF__ + "getMediaItemsList: requested uniqueName does not exist"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", uniqueName: " + uniqueName
						);
					_logger->error(errorMessage);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0;	// let's force a MIK that does not exist
				}
			}
        }

        string sqlWhere;
		sqlWhere = fmt::format("where mi.workspaceKey = {} and mi.markedAsRemoved = false ", workspaceKey);
        if (mediaItemKey != -1)
		{
			if (otherMediaItemsKey.size() > 0)
			{
				sqlWhere += ("and mi.mediaItemKey in (");
				sqlWhere += to_string(mediaItemKey);
				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					sqlWhere += (", " + to_string(otherMediaItemsKey[mediaItemIndex]));
				sqlWhere += ") ";
			}
			else
				sqlWhere += fmt::format("and mi.mediaItemKey = {} ", mediaItemKey);
		}
        if (contentTypePresent)
            sqlWhere += fmt::format("and mi.contentType = {} ", trans.quote(toString(contentType)));
        if (startIngestionDate != "")
            sqlWhere += fmt::format("and mi.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(startIngestionDate));
        if (endIngestionDate != "")
            sqlWhere += fmt::format("and mi.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ",
				trans.quote(endIngestionDate));
        if (title != "")
            sqlWhere += fmt::format("and LOWER(mi.title) like LOWER({}) ", trans.quote("%" + title + "%"));		// LOWER was used because the column is using utf8_bin that is case sensitive
		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk
		 *  1: look for liveRecordingChunk
		 */
        if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
				sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is null ");
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is not null ");
				// sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
		}
		if (recordingCode != -1)
			sqlWhere += fmt::format("and mi.recordingCode_virtual = {} ", recordingCode);

		if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
		{
			// SC: Start Chunk
			// PS: Playout Start, PE: Playout End
			// --------------SC--------------SC--------------SC--------------SC
			//                       PS-------------------------------PE
			
			sqlWhere += ("and ( ");

			// first chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodStartTimeInMilliSeconds
			sqlWhere += fmt::format("(mi.utcStartTimeInMilliSecs_virtual <= {} and {} < mi.utcEndTimeInMilliSecs_virtual) ",
				utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodStartTimeInMilliSeconds);

			sqlWhere += ("or ");

			// internal chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += fmt::format("({} <= mi.utcStartTimeInMilliSecs_virtual and mi.utcEndTimeInMilliSecs_virtual <= {}) ",
				utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);

			sqlWhere += ("or ");

			// last chunk of the cut
			// utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += fmt::format("(mi.utcStartTimeInMilliSecs_virtual < {} and {} <= mi.utcEndTimeInMilliSecs_virtual) ",
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond);

			sqlWhere += (") ");
		}

		if (tagsIn.size() > 0)
		{
			// &&: Gli array si sovrappongono, cioè hanno qualche elemento in comune?
			sqlWhere += fmt::format("and mi.tags && {} = true ", getPostgresArray(tagsIn, &trans));
		}
		if (tagsNotIn.size() > 0)
		{
			// &&: Gli array si sovrappongono, cioè hanno qualche elemento in comune?
			sqlWhere += fmt::format("and mi.tags && {} = false ", getPostgresArray(tagsNotIn, &trans));
		}

		if (jsonCondition != "")
			sqlWhere += ("and " + jsonCondition + " ");

		int64_t numFound;
        {
			string sqlStatement = fmt::format( 
				"select count(*) from MMS_MediaItem mi {}",
				sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			numFound = trans.exec1(sqlStatement)[0].as<int>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		string orderByCondition;
		if (orderBy == "" && jsonOrderBy == "")
			orderByCondition = " ";
		else if (orderBy == "" && jsonOrderBy != "")
			orderByCondition = "order by " + jsonOrderBy + " ";
		else if (orderBy != "" && jsonOrderBy == "")
			orderByCondition = "order by " + orderBy + " ";
		else // if (orderBy != "" && jsonOrderBy != "")
			orderByCondition = "order by " + jsonOrderBy + ", " + orderBy + " ";

         	string sqlStatement = fmt::format( 
          		"select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, mi.contentProviderKey, "
			"to_char(mi.ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate, "
			"to_char(mi.startPublishing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as startPublishing, "
			"to_char(mi.endPublishing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as endPublishing, "
          		"mi.contentType, mi.retentionInMinutes, mi.tags from MMS_MediaItem mi {} {} "
          		"limit {} offset {}",
          		sqlWhere, orderByCondition, rows, start);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans.exec(sqlStatement);

        Json::Value responseRoot;
        {
			field = "numFound";
			responseRoot[field] = numFound;
        }

        Json::Value mediaItemsRoot(Json::arrayValue);
        {
			chrono::system_clock::time_point startSqlResultSet = chrono::system_clock::now();
			for (auto row: res)
            {
                Json::Value mediaItemRoot;

				int64_t localMediaItemKey = row["mediaItemKey"].as<int64_t>();

                field = "mediaItemKey";
                mediaItemRoot[field] = localMediaItemKey;

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "title"))
				{
					string localTitle = row["title"].as<string>();

					// a printf is used to pring into the output, so % has to be changed to %%
					for (int titleIndex = localTitle.length() - 1; titleIndex >= 0; titleIndex--)
					{
						if (localTitle[titleIndex] == '%')
							localTitle.replace(titleIndex, 1, "%%");
					}

					field = "title";
					mediaItemRoot[field] = localTitle;
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "deliveryFileName"))
				{
					field = "deliveryFileName";
					if (row["deliveryFileName"].is_null())
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = row["deliveryFileName"].as<string>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "ingester"))
				{
					field = "ingester";
					if (row["ingester"].is_null())
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = row["ingester"].as<string>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "userData"))
				{
					field = "userData";
					if (row["userData"].is_null())
						mediaItemRoot[field] = Json::nullValue;
					else
						mediaItemRoot[field] = row["userData"].as<string>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "ingestionDate"))
				{
					field = "ingestionDate";
					mediaItemRoot[field] = row["ingestionDate"].as<string>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "startPublishing"))
				{
					field = "startPublishing";
					mediaItemRoot[field] = row["startPublishing"].as<string>();
				}
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "endPublishing"))
				{
					field = "endPublishing";
					mediaItemRoot[field] = row["endPublishing"].as<string>();
				}

				ContentType contentType = MMSEngineDBFacade::toContentType(
					row["contentType"].as<string>());
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "contentType"))
				{
					field = "contentType";
					mediaItemRoot[field] = row["contentType"].as<string>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "retentionInMinutes"))
				{
					field = "retentionInMinutes";
					mediaItemRoot[field] = row["retentionInMinutes"].as<int64_t>();
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "tags"))
				{
					Json::Value mediaItemTagsRoot(Json::arrayValue);

					{
						// pqxx::array<string> tagsArray = row["tags"].as<array>();
						auto tagsArray = row["tags"].as_array();
						pair<pqxx::array_parser::juncture, string> elem;
						do
						{
							elem = tagsArray.get_next();
							if (elem.first == pqxx::array_parser::juncture::string_value)
								mediaItemTagsRoot.append(elem.second);
						}
						while (elem.first != pqxx::array_parser::juncture::done);
					}
                   
					field = "tags";
					mediaItemRoot[field] = mediaItemTagsRoot;
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "providerName"))
				{
					int64_t contentProviderKey = row["contentProviderKey"].as<int64_t>();
                
					{
						string sqlStatement = fmt::format( 
							"select name from MMS_ContentProvider where contentProviderKey = {}",
							contentProviderKey);
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
							field = "providerName";
							mediaItemRoot[field] = res[0]["name"].as<string>();
						}
						else
						{
							string errorMessage = string("content provider does not exist")
								+ ", contentProviderKey: " + to_string(contentProviderKey)
							;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "uniqueName"))
				{
					{
						string sqlStatement = fmt::format( 
							"select uniqueName from MMS_ExternalUniqueName "
							"where workspaceKey = {} and mediaItemKey = {}",
							workspaceKey, localMediaItemKey);
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
							field = "uniqueName";
							mediaItemRoot[field] = res[0]["uniqueName"].as<string>();
						}
						else
						{
							field = "uniqueName";
							mediaItemRoot[field] = string("");
						}
					}
				}

				// CrossReferences
				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "crossReferences"))
				{
					// if (contentType == ContentType::Video)
					{
						Json::Value mediaItemReferencesRoot(Json::arrayValue);
                    
						{
							string sqlStatement = fmt::format( 
								"select sourceMediaItemKey, type, parameters "
								"from MMS_CrossReference "
								"where targetMediaItemKey = {}",
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";
								localMediaItemKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							result res = trans.exec(sqlStatement);
							for (auto row: res)
							{
								Json::Value crossReferenceRoot;

								field = "sourceMediaItemKey";
								crossReferenceRoot[field] = row["sourceMediaItemKey"].as<int64_t>();

								field = "type";
								crossReferenceRoot[field] = row["type"].as<string>();

								if (!row["parameters"].is_null())
								{
									string crossReferenceParameters = row["parameters"].as<string>();
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot
											= JSONUtils::toJson(-1, -1, crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.append(crossReferenceRoot);
							}
						}
                    
						{
							string sqlStatement = fmt::format( 
								"select type, targetMediaItemKey, parameters "
								"from MMS_CrossReference "
								"where sourceMediaItemKey = {}",
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";
								localMediaItemKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							result res = trans.exec(sqlStatement);
							for (auto row: res)
							{
								Json::Value crossReferenceRoot;

								field = "type";
								crossReferenceRoot[field] = row["type"].as<string>();

								field = "targetMediaItemKey";
								crossReferenceRoot[field] = row["targetMediaItemKey"].as<int64_t>();

								if (!row["parameters"].is_null())
								{
									string crossReferenceParameters = row["parameters"].as<string>();
									if (crossReferenceParameters != "")
									{
										Json::Value crossReferenceParametersRoot
											= JSONUtils::toJson(-1, -1, crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.append(crossReferenceRoot);
							}
						}

						field = "crossReferences";
						mediaItemRoot[field] = mediaItemReferencesRoot;
					}
					/*
					else if (contentType == ContentType::Audio)
					{
					}
					*/
				}

				if (responseFields == Json::nullValue
					|| JSONUtils::isMetadataPresent(responseFields, "physicalPaths"))
                {
                    Json::Value mediaItemProfilesRoot(Json::arrayValue);
                    
                    string sqlStatement = fmt::format( 
                        "select physicalPathKey, durationInMilliSeconds, bitRate, externalReadOnlyStorage, "
						"deliveryInfo ->> 'externalDeliveryTechnology' as externalDeliveryTechnology, "
						"deliveryInfo ->> 'externalDeliveryURL' as externalDeliveryURL, "
						"fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, retentionInMinutes, "
						"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
                        "from MMS_PhysicalPath where mediaItemKey = {}",
						localMediaItemKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.exec(sqlStatement);
					for (auto row: res)
                    {
                        Json::Value profileRoot;
                        
                        int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();

                        field = "physicalPathKey";
                        profileRoot[field] = physicalPathKey;

                        field = "durationInMilliSeconds";
						if (row["durationInMilliSeconds"].is_null())
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = row["durationInMilliSeconds"].as<int64_t>();

                        field = "bitRate";
						if (row["bitRate"].is_null())
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = row["bitRate"].as<int64_t>();

                        field = "fileFormat";
                        string fileName = row["fileName"].as<string>();
                        size_t extensionIndex = fileName.find_last_of(".");
						string fileExtension;
                        if (extensionIndex == string::npos)
                            profileRoot[field] = Json::nullValue;
                        else
						{
							fileExtension = fileName.substr(extensionIndex + 1);
							if (fileExtension == "m3u8")
								profileRoot[field] = "hls";
							else
								profileRoot[field] = fileExtension;
						}

						if (admin)
						{
							field = "partitionNumber";
							profileRoot[field] = row["partitionNumber"].as<int>();

							field = "relativePath";
							profileRoot[field] = row["relativePath"].as<string>();

							field = "fileName";
							profileRoot[field] = fileName;
						}

						field = "externalReadOnlyStorage";
						profileRoot[field] = (row["externalReadOnlyStorage"].as<bool>());

						field = "externalDeliveryTechnology";
						string externalDeliveryTechnology;
                        if (row["externalDeliveryTechnology"].is_null())
                            profileRoot[field] = Json::nullValue;
                        else
						{
							externalDeliveryTechnology = row["externalDeliveryTechnology"].as<string>();
                            profileRoot[field] = externalDeliveryTechnology;
						}

						field = "externalDeliveryURL";
                        if (row["externalDeliveryURL"].is_null())
                            profileRoot[field] = Json::nullValue;
                        else
                            profileRoot[field] = row["externalDeliveryURL"].as<string>();

                        field = "encodingProfileKey";
                        if (row["encodingProfileKey"].is_null())
						{
                            profileRoot[field] = Json::nullValue;

							field = "deliveryTechnology";
							if (externalDeliveryTechnology == "hls")
							{
								profileRoot[field] =
									MMSEngineDBFacade::toString(MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming);
							}
							else
							{
								MMSEngineDBFacade::DeliveryTechnology deliveryTechnology =
									MMSEngineDBFacade::fileFormatToDeliveryTechnology(fileExtension);
								profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);
							}

							field = "encodingProfileLabel";
                            profileRoot[field] = Json::nullValue;
						}
                        else
						{
							int64_t encodingProfileKey = row["encodingProfileKey"].as<int64_t>();

                            profileRoot[field] = encodingProfileKey;

							string label;
							MMSEngineDBFacade::ContentType contentType;
							MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;

                            tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string>
                                encodingProfileDetails = getEncodingProfileDetailsByKey(workspaceKey, encodingProfileKey);

                            tie(label, contentType, deliveryTechnology, ignore) = encodingProfileDetails;

							field = "deliveryTechnology";
                            profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);

							field = "encodingProfileLabel";
                            profileRoot[field] = label;
						}

                        field = "sizeInBytes";
                        profileRoot[field] = row["sizeInBytes"].as<int64_t>();

                        field = "creationDate";
                        profileRoot[field] = row["creationDate"].as<string>();

                        field = "retentionInMinutes";
						if (row["retentionInMinutes"].is_null())
							profileRoot[field] = Json::nullValue;
						else
							profileRoot[field] = row["retentionInMinutes"].as<int64_t>();

                        if (contentType == ContentType::Video)
                        {
							vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							getVideoDetails(localMediaItemKey, physicalPathKey, fromMaster,
								videoTracks, audioTracks);
                            _logger->info(__FILEREF__ + "getVideoDetails"
                                + ", mediaItemKey: " + to_string(localMediaItemKey)
                                + ", physicalPathKey: " + to_string(physicalPathKey)
                                + ", videoTracks.size: " + to_string(videoTracks.size())
                                + ", audioTracks.size: " + to_string(audioTracks.size())
							);

							{
								Json::Value videoTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack: videoTracks)
								{
									int64_t videoTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									int width;
									int height;
									string avgFrameRate;
									string codecName;
									long bitRate;
									string profile;

									tie(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
										avgFrameRate, codecName, bitRate, profile) = videoTrack;

									Json::Value videoTrackRoot;

									field = "videoTrackKey";
									videoTrackRoot[field] = videoTrackKey;

									field = "trackIndex";
									videoTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									videoTrackRoot[field] = durationInMilliSeconds;

									field = "width";
									videoTrackRoot[field] = width;

									field = "height";
									videoTrackRoot[field] = height;

									field = "avgFrameRate";
									videoTrackRoot[field] = avgFrameRate;

									field = "codecName";
									videoTrackRoot[field] = codecName;

									field = "bitRate";
									videoTrackRoot[field] = (int64_t) bitRate;

									field = "profile";
									videoTrackRoot[field] = profile;

									videoTracksRoot.append(videoTrackRoot);
								}

								field = "videoTracks";
								profileRoot[field] = videoTracksRoot;
							}

							{
								Json::Value audioTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack: audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName,
										sampleRate, channels, language) = audioTrack;

									Json::Value audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t) bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t) sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t) channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.append(audioTrackRoot);
								}

								field = "audioTracks";
								profileRoot[field] = audioTracksRoot;
							}
                        }
                        else if (contentType == ContentType::Audio)
                        {
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							getAudioDetails(localMediaItemKey, physicalPathKey, fromMaster, audioTracks);

							{
								Json::Value audioTracksRoot(Json::arrayValue);

								for(tuple<int64_t, int, int64_t, long, string, long, int, string>
									audioTrack: audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate,
										codecName, sampleRate, channels, language) = audioTrack;

									Json::Value audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t) bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t) sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t) channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.append(audioTrackRoot);
								}

								field = "audioTracks";
								profileRoot[field] = audioTracksRoot;
							}
                        }
                        else if (contentType == ContentType::Image)
                        {
                            int width;
                            int height;
                            string format;
                            int quality;

                            tuple<int,int,string,int>
                                imageDetails = getImageDetails(localMediaItemKey, physicalPathKey,
									fromMaster);

                            tie(width, height, format, quality) 
                                    = imageDetails;

                            Json::Value imageDetailsRoot;

                            field = "width";
                            imageDetailsRoot[field] = width;

                            field = "height";
                            imageDetailsRoot[field] = height;

                            field = "format";
                            imageDetailsRoot[field] = format;

                            field = "quality";
                            imageDetailsRoot[field] = quality;


                            field = "imageDetails";
                            profileRoot[field] = imageDetailsRoot;
                        }
                        else
                        {
                            string errorMessage = __FILEREF__ + "ContentType unmanaged"
                                + ", mediaItemKey: " + to_string(localMediaItemKey)
                                + ", sqlStatement: " + sqlStatement
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);  
                        }

                        mediaItemProfilesRoot.append(profileRoot);
                    }
                    
                    field = "physicalPaths";
                    mediaItemRoot[field] = mediaItemProfilesRoot;
                }

                mediaItemsRoot.append(mediaItemRoot);
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "mediaItems";
        responseRoot[field] = mediaItemsRoot;

        field = "response";
        mediaItemsListRoot[field] = responseRoot;

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

    return mediaItemsListRoot;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
    int64_t referenceMediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having 'ts' file format and bigger size in case there are more than one)
	int64_t encodingProfileKey,
	bool warningIfMissing, bool fromMaster
)
{
    int64_t physicalPathKey = -1;

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
		if (encodingProfileKey != -1)
        {
			string sqlStatement = fmt::format(
				"select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} "
				"and encodingProfileKey = {}",
				referenceMediaItemKey, encodingProfileKey);
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
                physicalPathKey = res[0]["physicalPathKey"].as<int64_t>();

			if (physicalPathKey == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/encodingProfileKey are not found"
                    + ", mediaItemKey: " + to_string(referenceMediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }
        }
		else
		{
			tuple<int64_t, int, string, string, int64_t, bool, int64_t> sourcePhysicalPathDetails =
				getSourcePhysicalPath(referenceMediaItemKey, warningIfMissing, fromMaster);
			tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = sourcePhysicalPathDetails;
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

    return physicalPathKey;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
        int64_t workspaceKey,
        int64_t mediaItemKey, ContentType contentType,
        string encodingProfileLabel,
		bool warningIfMissing,
		bool fromMaster
)
{
    int64_t physicalPathKey = -1;

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
        int64_t encodingProfileKey = -1;
        {
            string sqlStatement = fmt::format( 
                "select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and "
				"contentType = {} and label = {}",
				workspaceKey, trans.quote(toString(contentType)), trans.quote(encodingProfileLabel));
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
                encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "encodingProfileKey is not found"
                    + ", workspaceKey: " + to_string(workspaceKey)
                    + ", contentType: " + toString(contentType)
                    + ", encodingProfileLabel: " + encodingProfileLabel
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        {
            string sqlStatement = fmt::format( 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} "
				"and encodingProfileKey = {}",
				mediaItemKey, encodingProfileKey);
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
                physicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey/encodingProfileKey are not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", encodingProfileKey: " + to_string(encodingProfileKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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
    
    return physicalPathKey;
}

tuple<int64_t, int, string, string, int64_t, bool, int64_t> MMSEngineDBFacade::getSourcePhysicalPath(
    int64_t mediaItemKey, bool warningIfMissing, bool fromMaster
)
{
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
		int64_t physicalPathKeyWithEncodingProfile = -1;
		int64_t physicalPathKeyWithoutEncodingProfile = -1;

        int mmsPartitionNumberWithEncodingProfile;
        int mmsPartitionNumberWithoutEncodingProfile;

        bool externalReadOnlyStorageWithEncodingProfile;
        bool externalReadOnlyStorageWithoutEncodingProfile;

        string relativePathWithEncodingProfile;
        string relativePathWithoutEncodingProfile;

        string fileNameWithEncodingProfile;
        string fileNameWithoutEncodingProfile;

        int64_t sizeInBytesWithEncodingProfile;
        int64_t sizeInBytesWithoutEncodingProfile;

        int64_t durationInMilliSecondsWithEncodingProfile = 0;
        int64_t durationInMilliSecondsWithoutEncodingProfile = 0;

		int64_t maxSizeInBytesWithEncodingProfile = -1;
		int64_t maxSizeInBytesWithoutEncodingProfile = -1;

        {
			// 2023-01-23: l'ultima modifica fatta permette di inserire 'source content' specificando
			//	l'encoding profile (questo quando siamo sicuri che sia stato generato
			//	con uno specifico profilo). In questo nuovo scenario, molti 'source content' hanno specificato
			//	encodingProfileKey nella tabella MMS_PhysicalPath.
			//	Per cui viene tolta la condizione
			//		encodingProfileKey is null
			//	dalla select e viene cercato il source nel seguente modo:
			//	- se esiste un 'cource content' avente encodingProfileKey null viene considerato
			//		questo contenuto come 'source'
			//	- se non esiste viene cercato il source tra quelli con encodingProfileKey inizializzato
			string sqlStatement = fmt::format(
				"select physicalPathKey, sizeInBytes, fileName, relativePath, partitionNumber, "
				"externalReadOnlyStorage, durationInMilliSeconds, encodingProfileKey "
				"from MMS_PhysicalPath where mediaItemKey = {}",
				mediaItemKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			string selectedFileFormatWithEncodingProfile;
			string selectedFileFormatWithoutEncodingProfile;
			for (auto row: res)
            {
				int64_t localSizeInBytes = row["sizeInBytes"].as<int64_t>();

				string localFileName = row["fileName"].as<string>();
				string localFileFormat;
				size_t extensionIndex = localFileName.find_last_of(".");
				if (extensionIndex != string::npos)
					localFileFormat = localFileName.substr(extensionIndex + 1);

				if (row["encodingProfileKey"].is_null())
				{
					if (maxSizeInBytesWithoutEncodingProfile != -1)
					{
						// this is the second or third... physicalPath
						// we are fore sure in the scenario encodingProfileKey == -1
						// So, in case we have more than one "source" physicalPath, we will select the 'ts' one
						// We prefer 'ts' because is easy and safe do activities like cut or concat
						if (selectedFileFormatWithoutEncodingProfile == "ts")
						{
							if (localFileFormat == "ts")
							{
								if (localSizeInBytes <= maxSizeInBytesWithoutEncodingProfile)
									continue;
							}
							else
							{
								continue;
							}
						}
						else
						{
							if (localSizeInBytes <= maxSizeInBytesWithoutEncodingProfile)
								continue;
						}
					}

					physicalPathKeyWithoutEncodingProfile = row["physicalPathKey"].as<int64_t>();
					externalReadOnlyStorageWithoutEncodingProfile = row["externalReadOnlyStorage"].as<bool>();
					mmsPartitionNumberWithoutEncodingProfile = row["partitionNumber"].as<int>();
					relativePathWithoutEncodingProfile = row["relativePath"].as<string>();
					fileNameWithoutEncodingProfile = row["fileName"].as<string>();
					sizeInBytesWithoutEncodingProfile = row["sizeInBytes"].as<int64_t>();
					if (!row["durationInMilliSeconds"].is_null())
						durationInMilliSecondsWithoutEncodingProfile = row["durationInMilliSeconds"].as<int64_t>();

					fileNameWithoutEncodingProfile = localFileName;
					maxSizeInBytesWithoutEncodingProfile = localSizeInBytes;
					selectedFileFormatWithoutEncodingProfile = localFileFormat;
				}
				else
				{
					if (maxSizeInBytesWithEncodingProfile != -1)
					{
						// this is the second or third... physicalPath
						// we are fore sure in the scenario encodingProfileKey == -1
						// So, in case we have more than one "source" physicalPath, we will select the 'ts' one
						// We prefer 'ts' because is easy and safe do activities like cut or concat
						if (selectedFileFormatWithEncodingProfile == "ts")
						{
							if (localFileFormat == "ts")
							{
								if (localSizeInBytes <= maxSizeInBytesWithEncodingProfile)
									continue;
							}
							else
							{
								continue;
							}
						}
						else
						{
							if (localSizeInBytes <= maxSizeInBytesWithEncodingProfile)
								continue;
						}
					}

					physicalPathKeyWithEncodingProfile = row["physicalPathKey"].as<int64_t>();
					externalReadOnlyStorageWithEncodingProfile = row["externalReadOnlyStorage"].as<bool>();
					mmsPartitionNumberWithEncodingProfile = row["partitionNumber"].as<int>();
					relativePathWithEncodingProfile = row["relativePath"].as<string>();
					fileNameWithEncodingProfile = row["fileName"].as<string>();
					sizeInBytesWithEncodingProfile = row["sizeInBytes"].as<int64_t>();
					if (!row["durationInMilliSeconds"].is_null())
						durationInMilliSecondsWithEncodingProfile = row["durationInMilliSeconds"].as<int64_t>();

					fileNameWithEncodingProfile = localFileName;
					maxSizeInBytesWithEncodingProfile = localSizeInBytes;
					selectedFileFormatWithEncodingProfile = localFileFormat;
				}
            }
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			if (maxSizeInBytesWithoutEncodingProfile == -1 && maxSizeInBytesWithEncodingProfile == -1)
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

		// senza encoding profile ha priorità rispetto a 'con encoding profile'
		if (maxSizeInBytesWithoutEncodingProfile != -1)
			return make_tuple(physicalPathKeyWithoutEncodingProfile, mmsPartitionNumberWithoutEncodingProfile,
				relativePathWithoutEncodingProfile, fileNameWithoutEncodingProfile,
				sizeInBytesWithoutEncodingProfile, externalReadOnlyStorageWithoutEncodingProfile,
				durationInMilliSecondsWithoutEncodingProfile);
		else
			return make_tuple(physicalPathKeyWithEncodingProfile, mmsPartitionNumberWithEncodingProfile,
				relativePathWithEncodingProfile, fileNameWithEncodingProfile,
				sizeInBytesWithEncodingProfile, externalReadOnlyStorageWithEncodingProfile,
				durationInMilliSecondsWithEncodingProfile);
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
	MMSEngineDBFacade::getMediaItemKeyDetails(
    int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing,
	bool fromMaster
)
{
    tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
		contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

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
            string sqlStatement = fmt::format( 
                "select contentType, title, userData, ingestionJobKey, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate, "
				"EXTRACT(EPOCH FROM (ingestionDate + INTERVAL '1 minute' * retentionInMinutes - NOW() at time zone 'utc')) as willBeRemovedInSeconds "
				"from MMS_MediaItem where workspaceKey = {} and mediaItemKey = {}",
				workspaceKey, mediaItemKey);
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
                MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());

                string title;
                if (!res[0]["title"].is_null())
                    title = res[0]["title"].as<string>();
                
                string userData;
                if (!res[0]["userData"].is_null())
                    userData = res[0]["userData"].as<string>();

                int64_t ingestionJobKey = res[0]["ingestionJobKey"].as<int64_t>();

                string ingestionDate;
                if (!res[0]["ingestionDate"].is_null())
                    ingestionDate = res[0]["ingestionDate"].as<string>();

                int64_t willBeRemovedInSeconds = res[0]["willBeRemovedInSeconds"].as<float>();

                contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey =
					make_tuple(contentType, title, userData, ingestionDate, willBeRemovedInSeconds, ingestionJobKey);
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        return contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t,
	string, string, int64_t>
	MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(
	int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing,
	bool fromMaster
)
{
    tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t,
		string, string, int64_t> mediaItemDetails;

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
            string sqlStatement = fmt::format( 
                "select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, "
				"mi.ingestionJobKey, p.fileName, p.relativePath, p.durationInMilliSeconds, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
                "where mi.workspaceKey = {} and mi.mediaItemKey = p.mediaItemKey "
				"and p.physicalPathKey = {}",
				workspaceKey, physicalPathKey);
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
                int64_t mediaItemKey = res[0]["mediaItemKey"].as<int64_t>();
                MMSEngineDBFacade::ContentType contentType
					= MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());

                string userData;
                if (!res[0]["userData"].is_null())
                    userData = res[0]["userData"].as<string>();
                
                string title;
                if (!res[0]["title"].is_null())
                    title = res[0]["title"].as<string>();

                string fileName = res[0]["fileName"].as<string>();
                string relativePath = res[0]["relativePath"].as<string>();

                string ingestionDate;
                if (!res[0]["ingestionDate"].is_null())
                    ingestionDate = res[0]["ingestionDate"].as<string>();

                int64_t ingestionJobKey = res[0]["ingestionJobKey"].as<int64_t>();
				int64_t durationInMilliSeconds = 0;
				if (!res[0]["durationInMilliSeconds"].is_null())
					durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();

                mediaItemDetails = make_tuple(mediaItemKey, contentType, title,
					userData, ingestionDate, ingestionJobKey, fileName, relativePath,
					durationInMilliSeconds);
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", physicalPathKey: " + to_string(physicalPathKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        return mediaItemDetails;
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

void MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey(
	int64_t workspaceKey, int64_t referenceIngestionJobKey, 
	int maxLastMediaItemsToBeReturned,
	vector<tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType>>& mediaItemsDetails,
	bool warningIfMissing, bool fromMaster
)
{
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
		IngestionType ingestionType;
        {
			string sqlStatement = fmt::format(
				"select ingestionType from MMS_IngestionJob "
				"where ingestionJobKey = {} ",
				referenceIngestionJobKey);
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
				ingestionType     = MMSEngineDBFacade::toIngestionType(res[0]["ingestionType"].as<string>());
			else
			{
				string errorMessage = __FILEREF__ + "IngestionJob is not found"
					+ ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
					+ ", sqlStatement: " + sqlStatement
				;
				_logger->error(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);                    
			}
		}

        {
            // order by in the next select is important  to have the right order in case of dependency in a workflow
			/*
            lastSQLCommand = 
                "select ijo.mediaItemKey, ijo.physicalPathKey from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
				"where ijo.mediaItemKey = mi.mediaItemKey and ijo.ingestionJobKey = ? and "
				"(JSON_EXTRACT(userData, '$.mmsData.validated') is null or "	// in case of no live chunk MIK
					"JSON_EXTRACT(userData, '$.mmsData.validated') = true) "	// in case of live chunk MIK, we get only the one validated
				"order by ijo.mediaItemKey";
			*/
			// 2019-09-20: The Live-Recorder task now updates the Ingestion Status at the end of the task,
			// when main and backup management is finished (no MIKs with valitaded==false are present)
			// So we do not need anymore the above check
			string orderBy;
			if (ingestionType == MMSEngineDBFacade::IngestionType::LiveRecorder)
			{
				string segmenterType = "hlsSegmenter";
				// string segmenterType = "streamSegmenter";
				if (segmenterType == "hlsSegmenter")
					orderBy = "order by mi.utcStartTimeInMilliSecs_virtual desc ";
				else
					orderBy = "order by JSON_EXTRACT(mi.userData, '$.mmsData.utcChunkStartTime') desc ";
			}
			else
				orderBy = "order by ijo.mediaItemKey desc ";

			string sqlStatement = fmt::format(
				"select ijo.mediaItemKey, ijo.physicalPathKey "
				"from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
				"where mi.workspaceKey = {} and ijo.mediaItemKey = mi.mediaItemKey "
				"and ijo.ingestionJobKey = {} {} ",
				workspaceKey, referenceIngestionJobKey, orderBy);
			if (maxLastMediaItemsToBeReturned != -1)
				sqlStatement += ("limit " + to_string(maxLastMediaItemsToBeReturned));
			/*
			lastSQLCommand =
				"select mediaItemKey, physicalPathKey "
				"from MMS_IngestionJobOutput "
				"where ingestionJobKey = ? order by mediaItemKey";
			*/
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                int64_t mediaItemKey = row["mediaItemKey"].as<int64_t>();
                int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();

                ContentType contentType;
                {
                    string sqlStatement = fmt::format(
                        "select contentType from MMS_MediaItem where mediaItemKey = {}",
						mediaItemKey);
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
                        contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
                    else
                    {
                        string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                            + ", referenceIngestionJobKey: " + to_string(referenceIngestionJobKey)
                            + ", mediaItemKey: " + to_string(mediaItemKey)
                            + ", sqlStatement: " + sqlStatement
                        ;
                        if (warningIfMissing)
						{
                            _logger->warn(errorMessage);

							continue;
						}
                        else
						{
                            _logger->error(errorMessage);

							throw MediaItemKeyNotFound(errorMessage);                    
						}
                    }
                }

				tuple<int64_t,int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType 
					= make_tuple(mediaItemKey, physicalPathKey, contentType);
				mediaItemsDetails.insert(mediaItemsDetails.begin(),
					mediaItemKeyPhysicalPathKeyAndContentType);
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

pair<int64_t,MMSEngineDBFacade::ContentType>
	MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(
    int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing,
	bool fromMaster
)
{
    pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

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
            string sqlStatement = fmt::format( 
                "select mi.mediaItemKey, mi.contentType "
				"from MMS_MediaItem mi, MMS_ExternalUniqueName eun "
                "where mi.mediaItemKey = eun.mediaItemKey "
                "and eun.workspaceKey = {} and eun.uniqueName = {}",
				workspaceKey, trans.quote(referenceUniqueName));
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
                mediaItemKeyAndContentType.first = res[0]["mediaItemKey"].as<int64_t>();
                mediaItemKeyAndContentType.second = MMSEngineDBFacade::toContentType(
					res[0]["contentType"].as<string>());
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", referenceUniqueName: " + referenceUniqueName
                    + ", sqlStatement: " + sqlStatement
                ;
                if (warningIfMissing)
                    _logger->warn(errorMessage);
                else
                    _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
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
    catch(MediaItemKeyNotFound& e)
	{
        if (warningIfMissing)
			SPDLOG_WARN("MediaItemKeyNotFound SQL exception"
				", exceptionMessage: {}"
				", conn: {}",
				e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
			);
        else
			SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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
    
    return mediaItemKeyAndContentType;
}

int64_t MMSEngineDBFacade::getMediaDurationInMilliseconds(
	// mediaItemKey or physicalPathKey has to be initialized, the other has to be -1
	int64_t mediaItemKey,
	int64_t physicalPathKey,
	bool fromMaster
)
{
	int64_t durationInMilliSeconds;

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
        if (physicalPathKey == -1)
        {
            string sqlStatement = fmt::format( 
                "select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where mediaItemKey = {} and encodingProfileKey is null",
				mediaItemKey);
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
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());
				if (res[0]["durationInMilliSeconds"].is_null())
				{
					string errorMessage = __FILEREF__ + "duration is not found"
						+ ", mediaItemKey: " + to_string(mediaItemKey)
						+ ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}

				durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            string sqlStatement = fmt::format( 
				"select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where physicalPathKey = {}",
				physicalPathKey);
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
				if (res[0]["durationInMilliSeconds"].is_null())
				{
					string errorMessage = __FILEREF__ + "duration is not found"
						+ ", physicalPathKey: " + to_string(physicalPathKey)
						+ ", sqlStatement: " + sqlStatement
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}

				durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "physicalPathKey is not found"
					+ ", physicalPathKey: " + to_string(physicalPathKey)
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
    catch(MediaItemKeyNotFound& e)
	{
		SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

	return durationInMilliSeconds;
}

void MMSEngineDBFacade::getVideoDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	bool fromMaster,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>>& videoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
	)
{
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
        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            string sqlStatement = fmt::format( 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null",
				mediaItemKey);
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
                localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
		videoTracks.clear();
		audioTracks.clear();

        {
            string sqlStatement = fmt::format( 
                "select videoTrackKey, trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
                "codecName, profile, bitRate "
                "from MMS_VideoTrack where physicalPathKey = {}",
				localPhysicalPathKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                int64_t videoTrackKey = row["videoTrackKey"].as<int64_t>();
                int trackIndex = -1;
				if (!row["trackIndex"].is_null())
					trackIndex = row["trackIndex"].as<int>();
                int64_t durationInMilliSeconds = -1;
				if (!row["durationInMilliSeconds"].is_null())
					durationInMilliSeconds = row["durationInMilliSeconds"].as<int64_t>();
                long bitRate = -1;
				if (!row["bitRate"].is_null())
					bitRate = row["bitRate"].as<int>();
                string codecName;
				if (!row["codecName"].is_null())
					codecName = row["codecName"].as<string>();
                string profile;
				if (!row["profile"].is_null())
					profile = row["profile"].as<string>();
                int width = -1;
				if (!row["width"].is_null())
					width = row["width"].as<int>();
                int height = -1;
				if (!row["height"].is_null())
					height = row["height"].as<int>();
                string avgFrameRate;
				if (!row["avgFrameRate"].is_null())
					avgFrameRate = row["avgFrameRate"].as<string>();

				videoTracks.push_back(make_tuple(videoTrackKey, trackIndex, durationInMilliSeconds, width, height,
					avgFrameRate, codecName, bitRate, profile));
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
            string sqlStatement = fmt::format( 
                "select audioTrackKey, trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language "
                "from MMS_AudioTrack where physicalPathKey = {}",
				localPhysicalPathKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
            {
                int64_t audioTrackKey = row["audioTrackKey"].as<int64_t>();
                int trackIndex;
				if (!row["trackIndex"].is_null())
					trackIndex = row["trackIndex"].as<int>();
                int64_t durationInMilliSeconds;
				if (!row["durationInMilliSeconds"].is_null())
					durationInMilliSeconds = row["durationInMilliSeconds"].as<int64_t>();
                long bitRate = -1;
				if (!row["bitRate"].is_null())
					bitRate = row["bitRate"].as<int>();
                string codecName;
				if (!row["codecName"].is_null())
					codecName = row["codecName"].as<string>();
                long sampleRate = -1;
				if (!row["sampleRate"].is_null())
					sampleRate = row["sampleRate"].as<int>();
                int channels = -1;
				if (!row["channels"].is_null())
					channels = row["channels"].as<int>();
                string language;
				if (!row["language"].is_null())
					language = row["language"].as<string>();

				audioTracks.push_back(make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language));
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
    catch(MediaItemKeyNotFound& e)
	{
		SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

void MMSEngineDBFacade::getAudioDetails(
	int64_t mediaItemKey, int64_t physicalPathKey,
	bool fromMaster,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& audioTracks
	)
{
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
        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            string sqlStatement = fmt::format(
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null",
				mediaItemKey);
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
                localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
		audioTracks.clear();

        {
            string sqlStatement = fmt::format( 
                "select audioTrackKey, trackIndex, durationInMilliSeconds, "
				"codecName, bitRate, sampleRate, channels, language "
                "from MMS_AudioTrack where physicalPathKey = {}",
				localPhysicalPathKey);
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
                int64_t audioTrackKey = res[0]["audioTrackKey"].as<int64_t>();
                int trackIndex;
				if (!res[0]["trackIndex"].is_null())
					trackIndex = res[0]["trackIndex"].as<int>();
                int64_t durationInMilliSeconds;
				if (!res[0]["durationInMilliSeconds"].is_null())
					durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();
                long bitRate = -1;
				if (!res[0]["bitRate"].is_null())
					bitRate = res[0]["bitRate"].as<int>();
                string codecName;
				if (!res[0]["codecName"].is_null())
					codecName = res[0]["codecName"].as<string>();
                long sampleRate = -1;
				if (!res[0]["sampleRate"].is_null())
					sampleRate = res[0]["sampleRate"].as<int>();
                int channels = -1;
				if (!res[0]["channels"].is_null())
					channels = res[0]["channels"].as<int>();
                string language;
				if (!res[0]["language"].is_null())
					language = res[0]["language"].as<string>();

				audioTracks.push_back(make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds,
					bitRate, codecName, sampleRate, channels, language));
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", localPhysicalPathKey: " + to_string(localPhysicalPathKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
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
    catch(MediaItemKeyNotFound& e)
	{
		SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

tuple<int,int,string,int> MMSEngineDBFacade::getImageDetails(
    int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster
)
{
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
        int64_t localPhysicalPathKey;
        
        if (physicalPathKey == -1)
        {
            string sqlStatement = fmt::format( 
                "select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null",
				mediaItemKey);
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
                localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        else
        {
            localPhysicalPathKey = physicalPathKey;
        }
        
        int width;
        int height;
        string format;
        int quality;

        {
            string sqlStatement = fmt::format( 
                "select width, height, format, quality "
                "from MMS_ImageItemProfile where physicalPathKey = {}",
				localPhysicalPathKey);
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
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());
                width = res[0]["width"].as<int>();
                height = res[0]["height"].as<int>();
                format = res[0]["format"].as<string>();
                quality = res[0]["quality"].as<int>();
            }
            else
            {
                string errorMessage = __FILEREF__ + "MediaItemKey is not found"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw MediaItemKeyNotFound(errorMessage);                    
            }            
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        return make_tuple(width, height, format, quality);
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
    catch(MediaItemKeyNotFound& e)
	{
		SPDLOG_ERROR("MediaItemKeyNotFound SQL exception"
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

pair<int64_t,int64_t> MMSEngineDBFacade::saveSourceContentMetadata(
        shared_ptr<Workspace> workspace,
        int64_t ingestionJobKey,
        bool ingestionRowToBeUpdatedAsSuccess,        
        MMSEngineDBFacade::ContentType contentType,
		int64_t encodingProfileKey,
        Json::Value parametersRoot,
		bool externalReadOnlyStorage,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,

        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
        )
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

	pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey;
	string title = "";
    try
    {
        _logger->info(__FILEREF__ + "Retrieving contentProviderKey");
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
			contentProviderName = JSONUtils::asString(parametersRoot, "contentProviderName", _defaultContentProviderName);

            string sqlStatement = fmt::format( 
                "select contentProviderKey from MMS_ContentProvider where workspaceKey = {} and name = {}",
				workspace->_workspaceKey, trans.quote(contentProviderName));
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
                contentProviderKey = res[0]["contentProviderKey"].as<int64_t>();
            else
            {
                string errorMessage = __FILEREF__ + "ContentProvider is not present"
                    + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                    + ", contentProviderName: " + contentProviderName
                    + ", sqlStatement: " + sqlStatement
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->info(__FILEREF__ + "Insert into MMS_MediaItem");
        int64_t mediaItemKey;
        {
            string ingester = "";
            string userData = "";
            string deliveryFileName = "";
            string sContentType;
            int64_t retentionInMinutes = _contentRetentionInMinutesDefaultValue;
            // string encodingProfilesSet;

            string field = "title";
            title = JSONUtils::asString(parametersRoot, field, "");
            
            field = "ingester";
			ingester = JSONUtils::asString(parametersRoot, field, "");

            field = "userData";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
				// 2020-03-15: when it is set by the GUI it arrive here as a string
				if ((parametersRoot[field]).type() == Json::stringValue)
					userData = JSONUtils::asString(parametersRoot, field, "");
				else
					userData = JSONUtils::toString(parametersRoot[field]);
            }

            field = "deliveryFileName";
			deliveryFileName = JSONUtils::asString(parametersRoot, field, "");

            field = "retention";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
                string retention = JSONUtils::asString(parametersRoot, field, "1d");
				retentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
            }

            string startPublishing = "NOW";
            string endPublishing = "FOREVER";
            {
                field = "publishing";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value publishingRoot = parametersRoot[field];

                    field = "startPublishing";
					startPublishing = JSONUtils::asString(publishingRoot, field, "NOW");

                    field = "endPublishing";
					endPublishing = JSONUtils::asString(publishingRoot, field, "FOREVER");
                }
                
                if (startPublishing == "NOW")
                {
                    tm          tmDateTime;
                    char        strUtcDateTime [64];

                    chrono::system_clock::time_point now = chrono::system_clock::now();
                    time_t utcTime = chrono::system_clock::to_time_t(now);

                	gmtime_r (&utcTime, &tmDateTime);

                    sprintf (strUtcDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);

                    startPublishing = strUtcDateTime;
                }

                if (endPublishing == "FOREVER")
                {
                    tm          tmDateTime;
                    char        strUtcDateTime [64];

                    chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 10);

                    time_t utcTime = chrono::system_clock::to_time_t(forever);

                	gmtime_r (&utcTime, &tmDateTime);

                    sprintf (strUtcDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                            tmDateTime. tm_year + 1900,
                            tmDateTime. tm_mon + 1,
                            tmDateTime. tm_mday,
                            tmDateTime. tm_hour,
                            tmDateTime. tm_min,
                            tmDateTime. tm_sec);

                    endPublishing = strUtcDateTime;
                }
            }

			string tags;
			{
				Json::Value tagsRoot;
				string field = "tags";
				if (JSONUtils::isMetadataPresent(parametersRoot, field))
					tagsRoot = parametersRoot[field];
				tags = getPostgresArray(tagsRoot, &trans);
			}

            string sqlStatement = fmt::format( 
                "insert into MMS_MediaItem (mediaItemKey, workspaceKey, contentProviderKey, title, ingester, userData, " 
                "deliveryFileName, ingestionJobKey, ingestionDate, contentType, "
				"startPublishing, endPublishing, "
				"retentionInMinutes, tags, markedAsRemoved, processorMMSForRetention) values ("
                                           "DEFAULT,      {},           {},                 {},     {},      {}, "
				"{},               {},              NOW() at time zone 'utc', {}, "
                "to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), "
                "{},                 {},   false,           NULL) returning mediaItemKey",
				workspace->_workspaceKey, contentProviderKey, trans.quote(title),
				ingester == "" ? "null" : trans.quote(ingester),
				userData == "" ? "null" : trans.quote(userData),
				deliveryFileName == "" ? "null" : trans.quote(deliveryFileName),
				ingestionJobKey, trans.quote(toString(contentType)),
				trans.quote(startPublishing), trans.quote(endPublishing), retentionInMinutes,
				tags
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			mediaItemKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        {
            string uniqueName;
            if (JSONUtils::isMetadataPresent(parametersRoot, "uniqueName"))
                uniqueName = JSONUtils::asString(parametersRoot, "uniqueName", "");

            if (uniqueName != "")
            {
				bool allowUniqueNameOverride = false;
				allowUniqueNameOverride =
					JSONUtils::asBool(parametersRoot, "allowUniqueNameOverride", false);

				manageExternalUniqueName(conn, &trans, workspace->_workspaceKey, mediaItemKey,
					allowUniqueNameOverride, uniqueName);
            }
        }

		// cross references
		{
			string field = "crossReference";
			if (JSONUtils::isMetadataPresent(parametersRoot, field))
			{
                Json::Value crossReferenceRoot = parametersRoot[field];

				field = "type";
				MMSEngineDBFacade::CrossReferenceType crossReferenceType =
					MMSEngineDBFacade::toCrossReferenceType(JSONUtils::asString(crossReferenceRoot, field, ""));

				int64_t sourceMediaItemKey;
				int64_t targetMediaItemKey;

				if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::VideoOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;

					targetMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else if (crossReferenceType == MMSEngineDBFacade::CrossReferenceType::AudioOfImage)
				{
					crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfAudio;

					targetMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}
				else
				{
					sourceMediaItemKey = mediaItemKey;

					field = "mediaItemKey";
					targetMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
				}

                Json::Value crossReferenceParametersRoot;
				field = "parameters";
				if (JSONUtils::isMetadataPresent(crossReferenceRoot, field))
				{
					crossReferenceParametersRoot = crossReferenceRoot[field];
				}

				addCrossReference (conn, &trans, ingestionJobKey,
						sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
						crossReferenceParametersRoot);
			}
		}

		string externalDeliveryTechnology;
		string externalDeliveryURL;
		{
            string field = "externalDeliveryTechnology";
			externalDeliveryTechnology = JSONUtils::asString(parametersRoot, field, "");

            field = "externalDeliveryURL";
			externalDeliveryURL = JSONUtils::asString(parametersRoot, field, "");
		}

		int64_t physicalItemRetentionInMinutes = -1;
		{
            string field = "physicalItemRetention";
            if (JSONUtils::isMetadataPresent(parametersRoot, field))
            {
                string retention = JSONUtils::asString(parametersRoot, field, "1d");
				physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
            }
		}

		int64_t physicalPathKey = -1;
		{
			int64_t sourceIngestionJobKey = -1;

			// 2023-08-10: ogni MediaItem viene generato da un IngestionJob
			//	e la tabella MMS_IngestionJobOutput viene aggiornata con la coppia
			//	(ingestionJobKey, mediaItemKey)
			//	Ci sono casi dove serve associare il mediaItemKey anche ad un altro ingestionJobKey, esempi:
			//	- il LiveRecording inserisce il chunk con il Task Add-Content (IngestionJob) ma serve
			//		anche associare il mediaItemKey con l'ingestionJob del Task Live-Recorder
			//	- il Live-Cut genera un workflow per la creazione del file (Task Cut). Il mediaItemKey, oltre al Task Cut
			//		deve essere associato all'ingestionJob del Live-Cut
			//	Questi due inserimenti vengono eseguiti dal metodo addIngestionJobOutput che riceve in input:
			//	- ingestionJobKey: è il Job che genera il mediaItem
			//	- sourceIngestionJobKey: è il Job iniziale che ha richiesto la generazione del MediaItem (es: Live-Recorder, Live-Cut, ...)

			{
                string field = "userData";
                if (JSONUtils::isMetadataPresent(parametersRoot, field))
                {
                    Json::Value userDataRoot = parametersRoot[field];

                    field = "mmsData";
                    if (JSONUtils::isMetadataPresent(userDataRoot, field))
					{
						Json::Value mmsDataRoot = userDataRoot[field];

						field = "ingestionJobKey";
						if (JSONUtils::isMetadataPresent(mmsDataRoot, "liveRecordingChunk"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveRecordingChunk"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "generatedFrame"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["generatedFrame"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "externalTranscoder"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["externalTranscoder"], field, -1);
						else if (JSONUtils::isMetadataPresent(mmsDataRoot, "liveCut"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveCut"], field, -1);
					}
				}
			}

			physicalPathKey = saveVariantContentMetadata(
				conn,
				trans,

				workspace->_workspaceKey,
				ingestionJobKey,
				sourceIngestionJobKey,
				mediaItemKey,
				externalReadOnlyStorage,
				externalDeliveryTechnology,
				externalDeliveryURL,
				mediaSourceFileName,
				relativePath,
				mmsPartitionIndexUsed,
				sizeInBytes,
				encodingProfileKey,
				physicalItemRetentionInMinutes,

				// video-audio
				mediaInfoDetails,
				videoTracks,
				audioTracks,

				// image
				imageWidth,
				imageHeight,
				imageFormat,
				imageQuality
			);
		}

        {
            int currentDirLevel1;
            int currentDirLevel2;
            int currentDirLevel3;

            {
                string sqlStatement = fmt::format( 
                    "select currentDirLevel1, currentDirLevel2, currentDirLevel3 "
                    "from MMS_WorkspaceMoreInfo where workspaceKey = {}",
					workspace->_workspaceKey);
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
                    currentDirLevel1 = res[0]["currentDirLevel1"].as<int>();
                    currentDirLevel2 = res[0]["currentDirLevel2"].as<int>();
                    currentDirLevel3 = res[0]["currentDirLevel3"].as<int>();
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Workspace is not present/configured"
                        + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                        + ", sqlStatement: " + sqlStatement
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }

            if (currentDirLevel3 >= 999)
            {
                currentDirLevel3		= 0;

                if (currentDirLevel2 >= 999)
                {
                    currentDirLevel2		= 0;

                    if (currentDirLevel1 >= 999)
                    {
                        currentDirLevel1		= 0;
                    }
                    else
                    {
                        currentDirLevel1++;
                    }
                }
                else
                {
                    currentDirLevel2++;
                }
            }
            else
            {
                currentDirLevel3++;
            }

            {
                string sqlStatement = fmt::format(
                    "WITH rows AS (update MMS_WorkspaceMoreInfo set currentDirLevel1 = {}, currentDirLevel2 = {}, "
                    "currentDirLevel3 = {}, currentIngestionsNumber = currentIngestionsNumber + 1 "
                    "where workspaceKey = {} "
					"returning 1) select count(*) from rows",
					currentDirLevel1, currentDirLevel2, currentDirLevel3, workspace->_workspaceKey);
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
                            + ", currentDirLevel1: " + to_string(currentDirLevel1)
                            + ", currentDirLevel2: " + to_string(currentDirLevel2)
                            + ", currentDirLevel3: " + to_string(currentDirLevel3)
                            + ", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", sqlStatement: " + sqlStatement
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
        }
        
        {
            if (ingestionRowToBeUpdatedAsSuccess)
            {
                // we can have two scenarios:
                //  1. this ingestion will generate just one output file (most of the cases)
                //      in this case we will 
                //          update the ingestionJobKey with the status
                //          will add the row in MMS_IngestionJobOutput
                //          will call manageIngestionJobStatusUpdate
                //  2. this ingestion will generate multiple files (i.e. Periodical-Frames task)
                IngestionStatus newIngestionStatus = IngestionStatus::End_TaskSuccess;

                string errorMessage;
                string processorMMS;
                _logger->info(__FILEREF__ + "Update IngestionJob"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", IngestionStatus: " + toString(newIngestionStatus)
                    + ", errorMessage: " + errorMessage
                    + ", processorMMS: " + processorMMS
                );                            
                updateIngestionJob (conn, &trans, ingestionJobKey, newIngestionStatus, errorMessage);
            }
        }

		trans.commit();
		connectionPool->unborrow(conn);
		conn = nullptr;

        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
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
    catch(DeadlockFound& e)
	{
		SPDLOG_ERROR("SQL (Deadlock) exception"
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

    return mediaItemKeyAndPhysicalPathKey;
}

int64_t MMSEngineDBFacade::parseRetention(string retention)
{
	int64_t retentionInMinutes = -1;

	string localRetention = StringUtils::trim(retention);

	if (localRetention == "0")
		retentionInMinutes = 0;
	else if (localRetention.length() > 1)
	{
		switch (localRetention.back())
		{
			case 's':   // seconds
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) / 60;

				break;
			case 'm':   // minutes
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1));

				break;
			case 'h':   // hours
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 60;

				break;
			case 'd':   // days
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 1440;

				break;
			case 'M':   // month
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 30);

				break;
			case 'y':   // year
				retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 365);

				break;
		}
	}

	return retentionInMinutes;
}

void MMSEngineDBFacade::manageExternalUniqueName(
	shared_ptr<PostgresConnection> conn,
	transaction_base* trans,
	int64_t workspaceKey,
	int64_t mediaItemKey,

	bool allowUniqueNameOverride,
	string uniqueName
)
{
	try
	{
		if (uniqueName == "")
		{
			/*
			string errorMessage = __FILEREF__ + "uniqueName is empty"
				+ ", uniqueName: " + uniqueName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
			*/

			// delete it if present
			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (delete from MMS_ExternalUniqueName "
					"where workspaceKey = {} and mediaItemKey = {} "
					"returning 1) select count(*) from rows",
					workspaceKey, mediaItemKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
			}

			return;
		}

		// look if it is an insert (we do NOT have one) or an update (we already have one)
		string currentUniqueName;
		{
			string sqlStatement = fmt::format( 
				"select uniqueName from MMS_ExternalUniqueName "
				"where workspaceKey = {} and mediaItemKey = {}",
				workspaceKey, mediaItemKey);
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
				currentUniqueName = res[0]["uniqueName"].as<string>();
		}

		if (currentUniqueName == "")
		{
			// insert

			if (allowUniqueNameOverride)
			{
				string sqlStatement = fmt::format( 
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = {} and uniqueName = {}",
					workspaceKey, trans->quote(uniqueName));
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
				{
					int64_t mediaItemKeyOfCurrentUniqueName = res[0]["mediaItemKey"].as<int64_t>();

					{
						string sqlStatement = fmt::format( 
							"WITH rows AS (update MMS_ExternalUniqueName "
							"set uniqueName = uniqueName || '-' || '{}' || '-' || '{}' "
							"where workspaceKey = {} and uniqueName = {} "
							"returning 1) select count(*) from rows",
							mediaItemKey, chrono::system_clock::now().time_since_epoch().count(),
							workspaceKey, trans->quote(uniqueName));
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
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
							"WITH rows AS (update MMS_MediaItem "
							"set markedAsRemoved = true "
							"where workspaceKey = {} and mediaItemKey = {} "
							"returning 1) select count(*) from rows",
							workspaceKey, mediaItemKeyOfCurrentUniqueName);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
						SPDLOG_INFO("SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, conn->getConnectionId(),
							chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
						);
					}
				}
			}

			{
				string sqlStatement = fmt::format( 
					"insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) "
					"values ({}, {}, {})", workspaceKey, mediaItemKey, trans->quote(uniqueName)
				);
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
		else
		{
			// update

			if (allowUniqueNameOverride)
			{
				string sqlStatement = fmt::format( 
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = {} and uniqueName = {}",
					workspaceKey, trans->quote(uniqueName));
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
				{
					int64_t mediaItemKeyOfCurrentUniqueName = res[0]["mediaItemKey"].as<int64_t>();

					if (mediaItemKeyOfCurrentUniqueName != mediaItemKey)
					{
						{
							string sqlStatement = fmt::format( 
								"WITH rows AS (update MMS_ExternalUniqueName "
								"set uniqueName = uniqueName || '-' || '{}' || '-' || '{}' "
								"where workspaceKey = {} and uniqueName = {} "
								"returning 1) select count(*) from rows",
								mediaItemKey, chrono::system_clock::now().time_since_epoch().count(),
								workspaceKey, trans->quote(uniqueName));
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
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
								"WITH rows AS (update MMS_MediaItem "
								"set markedAsRemoved = true "
								"where workspaceKey = {} and mediaItemKey = {} "
								"returning 1) select count(*) from rows",
								workspaceKey, mediaItemKeyOfCurrentUniqueName);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
							SPDLOG_INFO("SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, conn->getConnectionId(),
								chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
							);
						}
					}
				}
			}

			{
				string sqlStatement = fmt::format( 
					"WITH rows AS (update MMS_ExternalUniqueName "
					"set uniqueName = {} "
					"where workspaceKey = {} and mediaItemKey = {} "
					"returning 1) select count(*) from rows",
					trans->quote(uniqueName), workspaceKey, mediaItemKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				int rowsUpdated = trans->exec1(sqlStatement)[0].as<int>();
				SPDLOG_INFO("SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, conn->getConnectionId(),
					chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
				);
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t sourceIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
)
{
	int64_t     physicalPathKey;
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	work trans{*(conn->_sqlConnection)};

    try
    {
        physicalPathKey = saveVariantContentMetadata(
            conn, trans,

            workspaceKey,
			ingestionJobKey,
			sourceIngestionJobKey,
            mediaItemKey,
			externalReadOnlyStorage,
			externalDeliveryTechnology,
			externalDeliveryURL,
            encodedFileName,
            relativePath,
            mmsPartitionIndexUsed,
            sizeInBytes,
            encodingProfileKey,
			physicalItemRetentionPeriodInMinutes,

            // video-audio
			mediaInfoDetails,
			videoTracks,
			audioTracks,

            // image
            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
        );

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
    
    return physicalPathKey;
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
        shared_ptr<PostgresConnection> conn,
		work& trans,

        int64_t workspaceKey,
		int64_t ingestionJobKey,
		int64_t sourceIngestionJobKey,
        int64_t mediaItemKey,
		bool externalReadOnlyStorage,
		string externalDeliveryTechnology,
		string externalDeliveryURL,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey,
		int64_t physicalItemRetentionPeriodInMinutes,
        
        // video-audio
		pair<int64_t, long>& mediaInfoDetails,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks,

        // image
        int imageWidth,
        int imageHeight,
        string imageFormat,
        int imageQuality
)
{
    int64_t     physicalPathKey;

    try
    {
        MMSEngineDBFacade::ContentType contentType;
        {
            string sqlStatement = fmt::format( 
                "select contentType from MMS_MediaItem where mediaItemKey = {}",
				mediaItemKey);
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
                contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
            else
            {
                string errorMessage = __FILEREF__ + "no ContentType returned"
                    + ", mediaItemKey: " + to_string(mediaItemKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

		string deliveryInfo;
		{
			if (externalDeliveryTechnology != "" || externalDeliveryURL != "")
			{
				Json::Value deliveryInfoRoot;

				string field = "externalDeliveryTechnology";
				deliveryInfoRoot[field] = externalDeliveryTechnology;

				field = "externalDeliveryURL";
				deliveryInfoRoot[field] = externalDeliveryURL;

                deliveryInfo = JSONUtils::toString(deliveryInfoRoot);                        
			}
		}

        int64_t durationInMilliSeconds;
        long bitRate;

		tie(durationInMilliSeconds, bitRate) = mediaInfoDetails;

        {
            int drm = 0;

			_logger->info(__FILEREF__ + "insert into MMS_PhysicalPath"
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", relativePath: " + relativePath
					+ ", encodedFileName: " + encodedFileName
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", deliveryInfo: " + deliveryInfo
					+ ", physicalItemRetentionPeriodInMinutes: " + to_string(physicalItemRetentionPeriodInMinutes)
					);
            string sqlStatement = fmt::format( 
                "insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, externalReadOnlyStorage, "
				"fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, "
				"durationInMilliSeconds, bitRate, deliveryInfo, creationDate, retentionInMinutes) values ("
                "DEFAULT, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, NOW() at time zone 'utc', {}) returning physicalPathKey",
				mediaItemKey, drm, externalReadOnlyStorage, trans.quote(encodedFileName),
				trans.quote(relativePath), mmsPartitionIndexUsed, sizeInBytes,
				encodingProfileKey == -1 ? "null" : to_string(encodingProfileKey),
				durationInMilliSeconds == -1 ? "null" : to_string(durationInMilliSeconds),
				bitRate == -1 ? "null" : to_string(bitRate),
				deliveryInfo == "" ? "null" : trans.quote(deliveryInfo),
				physicalItemRetentionPeriodInMinutes == -1 ? "null" : to_string(physicalItemRetentionPeriodInMinutes)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			physicalPathKey = trans.exec1(sqlStatement)[0].as<int64_t>();
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
		}

		if (contentType == ContentType::Video || contentType == ContentType::Audio)
        {
			for (tuple<int, int64_t, string, string, int, int, string, long> videoTrack: videoTracks)
            {
				int videoTrackIndex;
				int64_t videoDurationInMilliSeconds;
				string videoCodecName;
				string videoProfile;
				int videoWidth;
				int videoHeight;
				string videoAvgFrameRate;
				long videoBitRate;

				tie(videoTrackIndex, videoDurationInMilliSeconds, videoCodecName, videoProfile,
					videoWidth, videoHeight, videoAvgFrameRate, videoBitRate) = videoTrack;

                string sqlStatement = fmt::format( 
                    "insert into MMS_VideoTrack (videoTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
					"codecName, bitRate, profile) values ("
                    "DEFAULT, {}, {}, {}, {}, {}, {}, {}, {}, {})",
					physicalPathKey,
					videoTrackIndex == -1 ? "null" : to_string(videoTrackIndex),
					videoDurationInMilliSeconds == -1 ? "null" : to_string(videoDurationInMilliSeconds),
					videoWidth == -1 ? "null" : to_string(videoWidth),
					videoHeight == -1 ? "null" : to_string(videoHeight),
					videoAvgFrameRate == "" ? "null" : trans.quote(videoAvgFrameRate),
					videoCodecName == "" ? "null" : trans.quote(videoCodecName),
					videoBitRate == -1 ? "null" : to_string(videoBitRate),
					videoProfile == "" ? "null" : trans.quote(videoProfile)
				);
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

			for(tuple<int, int64_t, string, long, int, long, string> audioTrack: audioTracks)
            {
				int audioTrackIndex;
				int64_t audioDurationInMilliSeconds;
				string audioCodecName;
				long audioSampleRate;
				int audioChannels;
				long audioBitRate;
				string language;


				tie(audioTrackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate,
					audioChannels, audioBitRate, language) = audioTrack;

                string sqlStatement = fmt::format( 
                    "insert into MMS_AudioTrack (audioTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language) values ("
                    "DEFAULT, {}, {}, {}, {}, {}, {}, {}, {})",
					physicalPathKey,
					audioTrackIndex == -1 ? "null" : to_string(audioTrackIndex),
					audioDurationInMilliSeconds == -1 ? "null" : to_string(audioDurationInMilliSeconds),
					audioCodecName == "" ? "null" : trans.quote(audioCodecName),
					audioBitRate == -1 ? "null" : to_string(audioBitRate),
					audioSampleRate == -1 ? "null" : to_string(audioSampleRate),
					audioChannels == -1 ? "null" : to_string(audioChannels),
					language == "" ? "null" : trans.quote(language)
				);
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
		else if (contentType == ContentType::Image)
        {
			string sqlStatement = fmt::format( 
				"insert into MMS_ImageItemProfile (physicalPathKey, width, height, format, "
				"quality) values ("
				"{}, {}, {}, {}, {})",
				physicalPathKey, imageWidth, imageHeight, trans.quote(imageFormat), imageQuality
			);
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
		else
		{
			string errorMessage = __FILEREF__ + "ContentType is wrong"
				+ ", contentType: " + MMSEngineDBFacade::toString(contentType)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);                    
		}            

		addIngestionJobOutput(conn, &trans, ingestionJobKey, mediaItemKey, physicalPathKey,
			sourceIngestionJobKey);
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
	catch(exception& e)
	{
		SPDLOG_ERROR("exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
    
    return physicalPathKey;
}

void MMSEngineDBFacade::addCrossReference (
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType,
	int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
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
		addCrossReference (conn, &trans, ingestionJobKey,
			sourceMediaItemKey, crossReferenceType, targetMediaItemKey,
			crossReferenceParametersRoot);

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
    catch(DeadlockFound& e)
	{
		SPDLOG_ERROR("SQL (deadlock) exception"
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

void MMSEngineDBFacade::addCrossReference (
    shared_ptr<PostgresConnection> conn, transaction_base* trans,
	int64_t ingestionJobKey,
	int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	Json::Value crossReferenceParametersRoot)
{
    
    try
    {
		string crossReferenceParameters;
		{
            crossReferenceParameters = JSONUtils::toString(crossReferenceParametersRoot);
		}

        {
			string sqlStatement;
			if (crossReferenceParameters != "")
				sqlStatement = fmt::format(
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values ({}, {}, {}, {})",
					sourceMediaItemKey, trans->quote(toString(crossReferenceType)), targetMediaItemKey,
					trans->quote(crossReferenceParameters)
				);
			else
				sqlStatement = fmt::format( 
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values ({}, {}, {}, NULL)",
					sourceMediaItemKey, trans->quote(toString(crossReferenceType)), targetMediaItemKey
				);
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

		// if (exceptionMessage.find("Deadlock found when trying to get lock") !=
		// 	string::npos)
		// 	throw DeadlockFound(exceptionMessage);
		// else
			throw e;
	}
    catch(DeadlockFound& e)
	{
		SPDLOG_ERROR("SQL (deadlock) exception"
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

void MMSEngineDBFacade::removePhysicalPath (
	int64_t physicalPathKey)
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
                "WITH rows AS (delete from MMS_PhysicalPath "
				"where physicalPathKey = {} "
				"returning 1) select count(*) from rows",
				physicalPathKey);
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
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", physicalPathKey: " + to_string(physicalPathKey)
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

void MMSEngineDBFacade::removeMediaItem (
        int64_t mediaItemKey)
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
                "WITH rows AS (delete from MMS_MediaItem "
				"where mediaItemKey = {} "
				"returning 1) select count(*) from rows",
				mediaItemKey);
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
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", sqlStatement: " + sqlStatement
                ;
                _logger->warn(errorMessage);

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

Json::Value MMSEngineDBFacade::getTagsList (
	int64_t workspaceKey, int start, int rows,
	int liveRecordingChunk, bool contentTypePresent, ContentType contentType,
	string tagNameFilter, bool fromMaster
)
{
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

    Json::Value tagsListRoot;
    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getTagsList"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", liveRecordingChunk: " + to_string(liveRecordingChunk)
            + ", contentTypePresent: " + to_string(contentTypePresent)
            + ", contentType: " + (contentTypePresent ? toString(contentType) : "")
            + ", tagNameFilter: " + tagNameFilter
        );
        
        {
            Json::Value requestParametersRoot;

            field = "start";
            requestParametersRoot[field] = start;

            field = "rows";
            requestParametersRoot[field] = rows;
            
            field = "liveRecordingChunk";
            requestParametersRoot[field] = liveRecordingChunk;

            if (contentTypePresent)
            {
                field = "contentType";
                requestParametersRoot[field] = toString(contentType);
            }

            if (tagNameFilter != "")
            {
                field = "tagNameFilter";
                requestParametersRoot[field] = tagNameFilter;
            }
            
            field = "requestParameters";
            tagsListRoot[field] = requestParametersRoot;
        }

        string sqlWhere;
		sqlWhere = fmt::format("where workspaceKey = {} ", workspaceKey);
        if (contentTypePresent)
            sqlWhere += fmt::format("and contentType = {} ", trans.quote(toString(contentType)));
		if (liveRecordingChunk == 0)
			sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is NULL ");
		else if (liveRecordingChunk == 1)
			sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is not NULL ");

        Json::Value responseRoot;
        {
			string sqlStatement = fmt::format(
				"select count(distinct tagName) from ("
				"select unnest(tags) as tagName from MMS_MediaItem {}) t ",
				sqlWhere);
			if (tagNameFilter != "")
				sqlStatement += fmt::format("where tagName like {} ", trans.quote("%" + tagNameFilter + "%"));
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

        Json::Value tagsRoot(Json::arrayValue);
        {
			string sqlStatement = fmt::format( 
				"select distinct tagName from ("
				"select unnest(tags) as tagName from MMS_MediaItem {}) t ",
				sqlWhere);
			if (tagNameFilter != "")
				sqlStatement += fmt::format("where tagName like {} ", trans.quote("%" + tagNameFilter + "%"));
			sqlStatement += fmt::format("order by tagName limit {} offset {}", rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.exec(sqlStatement);
			for (auto row: res)
                tagsRoot.append(static_cast<string>(row["tagName"].as<string>()));
			SPDLOG_INFO("SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, conn->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);
        }

        field = "tags";
        responseRoot[field] = tagsRoot;

        field = "response";
        tagsListRoot[field] = responseRoot;

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
    
    return tagsListRoot;
}

void MMSEngineDBFacade::updateMediaItem(
		int64_t mediaItemKey,
        string processorMMSForRetention
        )
{
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = _masterPostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata 
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo 
	nontransaction trans{*(conn->_sqlConnection)};

	_logger->info(__FILEREF__ + "updateMediaItem"
			+ ", mediaItemKey: " + to_string(mediaItemKey)
			+ ", processorMMSForRetention: " + processorMMSForRetention
			);
    try
    {
        {
            string sqlStatement = fmt::format( 
                "WITH rows AS (update MMS_MediaItem set processorMMSForRetention = {} "
				"where mediaItemKey = {} "
				"returning 1) select count(*) from rows",
				processorMMSForRetention == "" ? "null" : trans.quote(processorMMSForRetention),
				mediaItemKey);
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
					+ ", mediaItemKey: " + to_string(mediaItemKey)
					+ ", processorMMSForRetention: " + processorMMSForRetention
					+ ", rowsUpdated: " + to_string(rowsUpdated)
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

