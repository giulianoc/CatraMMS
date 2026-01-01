
#include "JSONUtils.h"
#include "MMSEngineDBFacade.h"
#include "StringUtils.h"
#include "spdlog/spdlog.h"
#include <cstdint>

using namespace std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
using namespace pqxx;

void MMSEngineDBFacade::getExpiredMediaItemKeysCheckingDependencies(
	string processorMMS, vector<tuple<shared_ptr<Workspace>, int64_t, int64_t>> &mediaItemKeyOrPhysicalPathKeyToBeRemoved, int maxEntriesNumber
)
{
	PostgresConnTrans trans(_masterPostgresConnectionPool, false);
	try
	{
		// 2021-09-23: I removed TRANSACTION and FOR UPDATE because I saw we may have deadlock when a MediaItem is added

		SPDLOG_INFO(
			"getExpiredMediaItemKeysCheckingDependencies (MediaItemKeys expired)"
			", processorMMS: {}"
			", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: {}"
			", maxEntriesNumber: {}",
			processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved.size(), maxEntriesNumber
		);

		// 1. MediaItemKeys expired
		int start = 0;
		bool noMoreRowsReturned = false;
		while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber && !noMoreRowsReturned)
		{
			string sqlStatement = std::format(
				"select workspaceKey, mediaItemKey, ingestionJobKey, retentionInMinutes, title, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem where "
				"willBeRemovedAt_virtual < NOW() at time zone 'utc' "
				"and processorMMSForRetention is null "
				"limit {} offset {}", // for update"; see comment marked as 2021-09-23
				maxEntriesNumber, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			chrono::milliseconds internalSqlDuration(0);
			result res = trans.transaction->exec(sqlStatement);
			noMoreRowsReturned = true;
			start += maxEntriesNumber;
			for (auto row : res)
			{
				noMoreRowsReturned = false;

				auto ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
				auto workspaceKey = row["workspaceKey"].as<int64_t>();
				auto mediaItemKey = row["mediaItemKey"].as<int64_t>();
				auto retentionInMinutes = row["retentionInMinutes"].as<int64_t>();
				auto ingestionDate = row["ingestionDate"].as<string>();
				auto title = row["title"].as<string>();

				// check if there is still an ingestion depending on the ingestionJobKey
				chrono::system_clock::time_point startMethod = chrono::system_clock::now();
				bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(trans, ingestionJobKey) > 0)
					ingestionDependingOnMediaItemKey = true;
				auto sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startMethod);
				internalSqlDuration += sqlDuration;

				if (!ingestionDependingOnMediaItemKey)
				{
					{
						string sqlStatement = std::format(
							"update MMS_MediaItem set processorMMSForRetention = {} "
							"where mediaItemKey = {} and processorMMSForRetention is null ",
							trans.transaction->quote(processorMMS), mediaItemKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						result res = trans.transaction->exec(sqlStatement);
						int rowsUpdated = res.affected_rows();
						auto sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
						internalSqlDuration += sqlDuration;
						long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
						SQLQUERYLOG(
							"default", elapsed,
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, trans.connection->getConnectionId(), sqlDuration.count()
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
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
							*/
						}
					}

					shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

					tuple<shared_ptr<Workspace>, int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey = make_tuple(workspace, mediaItemKey, -1);

					mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
				}
				else
				{
					SPDLOG_INFO(
						"Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
						"ingestionJobKey: {}"
						", workspaceKey: {}"
						", mediaItemKey: {}"
						", title: {}"
						", ingestionDate: {}"
						", retentionInMinutes: {}",
						ingestionJobKey, workspaceKey, mediaItemKey, title, ingestionDate, retentionInMinutes
					);
				}
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(),
				chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count()
			);
		}

		SPDLOG_INFO(
			"getExpiredMediaItemKeysCheckingDependencies (PhysicalPathKeys expired)"
			", processorMMS: {}"
			", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: {}"
			", maxEntriesNumber: {}",
			processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved.size(), maxEntriesNumber
		);

		// 1. PhysicalPathKeys expired
		start = 0;
		noMoreRowsReturned = false;
		while (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() < maxEntriesNumber && !noMoreRowsReturned)
		{
			string sqlStatement = std::format(
				"select mi.workspaceKey, mi.mediaItemKey, p.physicalPathKey, mi.ingestionJobKey, "
				"p.retentionInMinutes, mi.title, "
				"to_char(mi.ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p where "
				"mi.mediaItemKey = p.mediaItemKey "
				"and p.retentionInMinutes is not null "
				// PhysicalPathKey expired
				"and mi.ingestionDate + INTERVAL '1 minute' * p.retentionInMinutes < NOW() at time zone 'utc' "
				// MediaItemKey not expired
				"and mi.willBeRemovedAt_virtual > NOW() at time zone 'utc' "
				"and processorMMSForRetention is null "
				"limit {} offset {}", // for update"; see comment marked as 2021-09-23
				maxEntriesNumber, start
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			noMoreRowsReturned = true;
			start += maxEntriesNumber;
			for (auto row : res)
			{
				noMoreRowsReturned = false;

				auto ingestionJobKey = row["ingestionJobKey"].as<int64_t>();
				auto workspaceKey = row["workspaceKey"].as<int64_t>();
				auto mediaItemKey = row["mediaItemKey"].as<int64_t>();
				auto physicalPathKey = row["physicalPathKey"].as<int64_t>();
				auto physicalPathKeyRetentionInMinutes = row["retentionInMinutes"].as<int64_t>();
				auto ingestionDate = row["ingestionDate"].as<string>();
				auto title = row["title"].as<string>();

				// check if there is still an ingestion depending on the ingestionJobKey
				bool ingestionDependingOnMediaItemKey = false;
				if (getNotFinishedIngestionDependenciesNumberByIngestionJobKey(trans, ingestionJobKey) > 0)
					ingestionDependingOnMediaItemKey = true;

				if (!ingestionDependingOnMediaItemKey)
				{
					{
						string sqlStatement = std::format(
							"update MMS_MediaItem set processorMMSForRetention = {} "
							"where mediaItemKey = {} and processorMMSForRetention is null ",
							trans.transaction->quote(processorMMS), mediaItemKey
						);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						result res = trans.transaction->exec(sqlStatement);
						int rowsUpdated = res.affected_rows();
						long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
						SQLQUERYLOG(
							"default", elapsed,
							"SQL statement"
							", sqlStatement: @{}@"
							", getConnectionId: @{}@"
							", elapsed (millisecs): @{}@",
							sqlStatement, trans.connection->getConnectionId(), elapsed
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
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
							*/
						}
					}

					shared_ptr<Workspace> workspace = getWorkspace(workspaceKey);

					tuple<shared_ptr<Workspace>, int64_t, int64_t> workspaceMediaItemKeyAndPhysicalPathKey =
						make_tuple(workspace, mediaItemKey, physicalPathKey);

					mediaItemKeyOrPhysicalPathKeyToBeRemoved.push_back(workspaceMediaItemKeyAndPhysicalPathKey);
				}
				else
				{
					SPDLOG_INFO(
						"Content expired but not removed because there are still ingestion jobs depending on him. Content details: "
						"ingestionJobKey: {}"
						", workspaceKey: {}"
						", mediaItemKey: {}"
						", title: {}"
						", ingestionDate: {}"
						", physicalPathKeyRetentionInMinutes: {}",
						ingestionJobKey, workspaceKey, mediaItemKey, title, ingestionDate, physicalPathKeyRetentionInMinutes
					);
				}
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

		SPDLOG_INFO(
			"getExpiredMediaItemKeysCheckingDependencies"
			", processorMMS: {}"
			", mediaItemKeyOrPhysicalPathKeyToBeRemoved.size: {}"
			", maxEntriesNumber: {}",
			processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved.size(), maxEntriesNumber
		);
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

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(int64_t ingestionJobKey, bool fromMaster)
{
	int dependenciesNumber;
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		dependenciesNumber = getNotFinishedIngestionDependenciesNumberByIngestionJobKey(trans, ingestionJobKey);
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

	return dependenciesNumber;
}

int MMSEngineDBFacade::getNotFinishedIngestionDependenciesNumberByIngestionJobKey(PostgresConnTrans &trans, int64_t ingestionJobKey)
{
	int dependenciesNumber;

	try
	{
		{
			// like: non lo uso per motivi di performance
			string sqlStatement = std::format(
				"select count(*) from MMS_IngestionJobDependency ijd, MMS_IngestionJob ij where "
				"ijd.ingestionJobKey = ij.ingestionJobKey "
				"and ijd.dependOnIngestionJobKey = {} "
				"and ij.status in ('Start_TaskQueued', 'SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', "
				"'SourceUploadingInProgress', 'EncodingQueued') ", // not like 'End_%' "
				ingestionJobKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			dependenciesNumber = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		throw;
	}

	return dependenciesNumber;
}

json MMSEngineDBFacade::updateMediaItem(
	int64_t workspaceKey, int64_t mediaItemKey, bool titleModified, string newTitle, bool userDataModified, string newUserData,
	bool retentionInMinutesModified, int64_t newRetentionInMinutes, bool tagsModified, json tagsRoot, bool uniqueNameModified, string newUniqueName,
	json crossReferencesRoot, bool admin
)
{
	json mediaItemRoot;
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
		if (titleModified || userDataModified || retentionInMinutesModified || tagsModified)
		{
			string setSQL;

			if (titleModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("title = {}", trans.transaction->quote(newTitle));
			}

			if (userDataModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				if (newUserData == "")
					setSQL += ("userData = null");
				else
					setSQL += std::format("userData = {}", trans.transaction->quote(newUserData));
			}

			if (retentionInMinutesModified)
			{
				if (setSQL != "")
					setSQL += ", ";
				setSQL += std::format("retentionInMinutes = {}", newRetentionInMinutes);
			}

			if (tagsModified)
			{
				if (!setSQL.empty())
					setSQL += ", ";
				setSQL += std::format("tags = {}", getPostgresArray(tagsRoot, true, trans));
			}

			setSQL = "set " + setSQL + " ";

			string sqlStatement = std::format(
				"update MMS_MediaItem {} "
				"where mediaItemKey = {} "
				// 2021-02: in case the user is not the owner and it is a shared workspace
				//		the workspacekey will not match
				// 2021-03: I think the above comment is wrong, the user, in that case,
				//		will use an APIKey of the correct workspace, even if this is shared.
				//		So let's add again the below sql condition
				"and workspaceKey = {} ",
				setSQL, mediaItemKey, workspaceKey
			);
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
				SPDLOG_WARN(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
		}

		if (uniqueNameModified)
		{
			bool allowUniqueNameOverride = false;

			manageExternalUniqueName(
				trans, workspaceKey, mediaItemKey,

				allowUniqueNameOverride, newUniqueName
			);
		}

		if (crossReferencesRoot != nullptr)
			manageCrossReferences(trans, -1, workspaceKey, mediaItemKey, crossReferencesRoot);
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
	set<string> responseFields;

	json mediaItemsListRoot = getMediaItemsList(
		workspaceKey, mediaItemKey, uniqueName, physicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent, contentType,
		// startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
		utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true
	);

	return mediaItemsListRoot;
}

json MMSEngineDBFacade::updatePhysicalPath(
	int64_t workspaceKey, int64_t mediaItemKey, int64_t physicalPathKey, int64_t newRetentionInMinutes, bool admin
)
{
	json mediaItemRoot;
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
		{
			string sqlStatement = std::format(
				"update MMS_PhysicalPath set retentionInMinutes = {} "
				"where physicalPathKey = {} and mediaItemKey = {} ",
				newRetentionInMinutes == -1 ? "null" : to_string(newRetentionInMinutes), physicalPathKey, mediaItemKey
			);
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
			/*
			if (rowsUpdated != 1)
			{
				string errorMessage = __FILEREF__ + "no update was done"
						+ ", physicalPathKey: " + to_string(physicalPathKey)
						+ ", newRetentionInMinutes: " + to_string(newRetentionInMinutes)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", sqlStatement: " + sqlStatement
				;
				SPDLOG_WARN(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
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
	set<string> responseFields;

	json mediaItemsListRoot = getMediaItemsList(
		workspaceKey, mediaItemKey, uniqueName, localPhysicalPathKey, otherMediaItemsKey, start, rows, contentTypePresent, contentType,
		// startAndEndIngestionDatePresent,
		startIngestionDate, endIngestionDate, title, liveRecordingChunk, recordingCode, utcCutPeriodStartTimeInMilliSeconds,
		utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, jsonCondition, tagsIn, tagsNotIn, orderBy, jsonOrderBy, responseFields, admin,
		// 2022-12-18: MIK is just updated, let's take from master
		true
	);

	return mediaItemsListRoot;
}

json MMSEngineDBFacade::getMediaItemsList(
	int64_t workspaceKey, int64_t mediaItemKey, string uniqueName, int64_t physicalPathKey, vector<int64_t> &otherMediaItemsKey, int start, int rows,
	bool contentTypePresent, ContentType contentType,
	// bool startAndEndIngestionDatePresent,
	string startIngestionDate, string endIngestionDate, string title, int liveRecordingChunk, int64_t recordingCode,
	int64_t utcCutPeriodStartTimeInMilliSeconds, int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, string jsonCondition, vector<string> &tagsIn,
	vector<string> &tagsNotIn,
	string orderBy,		// i.e.: "", mi.ingestionDate desc, mi.title asc
	string jsonOrderBy, // i.e.: "", JSON_EXTRACT(userData, '$.mmsData.utcChunkStartTime') asc
	set<string> &responseFields, bool admin, bool fromMaster
)
{
	json mediaItemsListRoot;

	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		string field;

		SPDLOG_INFO(
			"getMediaItemsList"
			", workspaceKey: {}"
			", mediaItemKey: {}"
			", uniqueName: {}"
			", physicalPathKey: {}"
			", start: {}"
			", rows: {}"
			", contentTypePresent: {}"
			", contentType: {}"
			", startIngestionDate: {}"
			", endIngestionDate: {}"
			", title: {}"
			", tagsIn.size(): {}"
			", tagsNotIn.size(): {}"
			", otherMediaItemsKey.size(): {}"
			", liveRecordingChunk: {}"
			", recordingCode: {}"
			", jsonCondition: {}"
			", orderBy: {}"
			", jsonOrderBy: {}",
			workspaceKey, mediaItemKey, uniqueName, physicalPathKey, start, rows, contentTypePresent,
			(contentTypePresent ? toString(contentType) : ""), startIngestionDate, endIngestionDate, title, tagsIn.size(), tagsNotIn.size(),
			otherMediaItemsKey.size(), liveRecordingChunk, recordingCode, jsonCondition, orderBy, jsonOrderBy
		);

		{
			json requestParametersRoot;

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
				json tagsRoot = json::array();

				for (int tagIndex = 0; tagIndex < tagsIn.size(); tagIndex++)
					tagsRoot.push_back(tagsIn[tagIndex]);

				field = "tagsIn";
				requestParametersRoot[field] = tagsRoot;
			}

			if (tagsNotIn.size() > 0)
			{
				json tagsRoot = json::array();

				for (int tagIndex = 0; tagIndex < tagsNotIn.size(); tagIndex++)
					tagsRoot.push_back(tagsNotIn[tagIndex]);

				field = "tagsNotIn";
				requestParametersRoot[field] = tagsRoot;
			}

			if (otherMediaItemsKey.size() > 0)
			{
				json otherMediaItemsKeyRoot = json::array();

				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					otherMediaItemsKeyRoot.push_back(otherMediaItemsKey[mediaItemIndex]);

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
				try
				{
					newMediaItemKey = physicalPath_columnAsInt64("mediaitemkey", physicalPathKey, nullptr, fromMaster);
				}
				catch (DBRecordNotFound &e)
				{
					SPDLOG_WARN(
						"physicalPathKey does not exist"
						", physicalPathKey: {}",
						physicalPathKey
					);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0; // let's force a MIK that does not exist
				}
			}
			else if (uniqueName != "")
			{
				try
				{
					newMediaItemKey = externalUniqueName_columnAsInt64(workspaceKey, "mediaitemkey", uniqueName, -1, nullptr, fromMaster);
				}
				catch (DBRecordNotFound &e)
				{
					SPDLOG_WARN(
						"getExternalUniqueName_MediaItemKey: requested workspaceKey/uniqueName does not exist"
						", workspaceKey: {}"
						", uniqueName: {}",
						workspaceKey, uniqueName
					);

					// throw runtime_error(errorMessage);
					newMediaItemKey = 0; // let's force a MIK that does not exist
				}
			}
		}

		string sqlWhere;
		sqlWhere = std::format("where mi.workspaceKey = {} and mi.markedAsRemoved = false ", workspaceKey);
		if (newMediaItemKey != -1)
		{
			if (otherMediaItemsKey.size() > 0)
			{
				sqlWhere += ("and mi.mediaItemKey in (");
				sqlWhere += to_string(newMediaItemKey);
				for (int mediaItemIndex = 0; mediaItemIndex < otherMediaItemsKey.size(); mediaItemIndex++)
					sqlWhere += (", " + to_string(otherMediaItemsKey[mediaItemIndex]));
				sqlWhere += ") ";
			}
			else
				sqlWhere += std::format("and mi.mediaItemKey = {} ", newMediaItemKey);
		}
		if (contentTypePresent)
			sqlWhere += std::format("and mi.contentType = {} ", trans.transaction->quote(toString(contentType)));
		if (startIngestionDate != "")
			sqlWhere += std::format(
				"and mi.ingestionDate >= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(startIngestionDate)
			);
		if (endIngestionDate != "")
			sqlWhere += std::format(
				"and mi.ingestionDate <= to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') ", trans.transaction->quote(endIngestionDate)
			);
		if (title != "")
			sqlWhere += std::format(
				"and LOWER(mi.title) like LOWER({}) ", trans.transaction->quote("%" + title + "%")
			); // LOWER was used because the column is using utf8_bin that is case sensitive
		/*
		 * liveRecordingChunk:
		 * -1: no condition in select
		 *  0: look for NO liveRecordingChunk
		 *  1: look for liveRecordingChunk
		 */
		if (contentTypePresent && contentType == ContentType::Video && liveRecordingChunk != -1)
		{
			if (liveRecordingChunk == 0)
				sqlWhere += ("and liveRecordingChunk_virtual = false ");
			else if (liveRecordingChunk == 1)
				sqlWhere += ("and liveRecordingChunk_virtual = true ");
			// sqlWhere += ("and JSON_UNQUOTE(JSON_EXTRACT(userData, '$.mmsData.dataType')) like 'liveRecordingChunk%' ");
		}
		if (recordingCode != -1)
			sqlWhere += std::format("and mi.recordingCode_virtual = {} ", recordingCode);

		if (utcCutPeriodStartTimeInMilliSeconds != -1 && utcCutPeriodEndTimeInMilliSecondsPlusOneSecond != -1)
		{
			// SC: Start Chunk
			// PS: Playout Start, PE: Playout End
			// --------------SC--------------SC--------------SC--------------SC
			//                       PS-------------------------------PE

			sqlWhere += ("and ( ");

			// first chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodStartTimeInMilliSeconds
			sqlWhere += std::format(
				"(mi.utcStartTimeInMilliSecs_virtual <= {} and {} < mi.utcEndTimeInMilliSecs_virtual) ", utcCutPeriodStartTimeInMilliSeconds,
				utcCutPeriodStartTimeInMilliSeconds
			);

			sqlWhere += ("or ");

			// internal chunk of the cut
			// utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += std::format(
				"({} <= mi.utcStartTimeInMilliSecs_virtual and mi.utcEndTimeInMilliSecs_virtual <= {}) ", utcCutPeriodStartTimeInMilliSeconds,
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			);

			sqlWhere += ("or ");

			// last chunk of the cut
			// utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			sqlWhere += std::format(
				"(mi.utcStartTimeInMilliSecs_virtual < {} and {} <= mi.utcEndTimeInMilliSecs_virtual) ",
				utcCutPeriodEndTimeInMilliSecondsPlusOneSecond, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond
			);

			sqlWhere += (") ");
		}

		if (tagsIn.size() > 0)
		{
			// &&: Gli array si sovrappongono, cioè hanno qualche elemento in comune?
			sqlWhere += std::format("and mi.tags && {} = true ", getPostgresArray(tagsIn, true, trans));
		}
		if (tagsNotIn.size() > 0)
		{
			// &&: Gli array si sovrappongono, cioè hanno qualche elemento in comune?
			sqlWhere += std::format("and mi.tags && {} = false ", getPostgresArray(tagsNotIn, true, trans));
		}

		if (jsonCondition != "")
			sqlWhere += ("and " + jsonCondition + " ");

		int64_t numFound;
		{
			string sqlStatement = std::format("select count(*) from MMS_MediaItem mi {}", sqlWhere);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			numFound = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		string orderByCondition;
		if (orderBy == "" && jsonOrderBy == "")
			orderByCondition = " ";
		else if (orderBy == "" && jsonOrderBy != "")
			orderByCondition = "order by " + jsonOrderBy + " ";
		else if (orderBy != "" && jsonOrderBy == "")
			orderByCondition = "order by " + orderBy + " ";
		else // if (orderBy != "" && jsonOrderBy != "")
			orderByCondition = "order by " + jsonOrderBy + ", " + orderBy + " ";

		string sqlStatement = std::format(
			"select mi.mediaItemKey, mi.title, mi.deliveryFileName, mi.ingester, mi.userData, "
			"to_char(mi.ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as formattedIngestionDate, "
			"to_char(mi.startPublishing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as formattedStartPublishing, "
			"to_char(mi.endPublishing, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as formattedEndPublishing, "
			"to_char(mi.willBeRemovedAt_virtual, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as formattedWillBeRemovedAt, "
			"mi.contentType, mi.retentionInMinutes, mi.tags from MMS_MediaItem mi {} {} "
			"limit {} offset {}",
			sqlWhere, orderByCondition, rows, start
		);
		chrono::milliseconds internalSqlDuration(0);
		chrono::system_clock::time_point startSql = chrono::system_clock::now();
		result res = trans.transaction->exec(sqlStatement);

		json responseRoot;
		{
			field = "numFound";
			responseRoot[field] = numFound;
		}

		json mediaItemsRoot = json::array();
		{
			chrono::system_clock::time_point startSqlResultSet = chrono::system_clock::now();
			for (auto row : res)
			{
				json mediaItemRoot;

				int64_t localMediaItemKey = row["mediaItemKey"].as<int64_t>();

				field = "mediaItemKey";
				mediaItemRoot[field] = localMediaItemKey;

				if (responseFields.empty() || responseFields.find("title") != responseFields.end())
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

				if (responseFields.empty() || responseFields.find("deliveryFileName") != responseFields.end())
				{
					field = "deliveryFileName";
					if (row["deliveryFileName"].is_null())
						mediaItemRoot[field] = nullptr;
					else
						mediaItemRoot[field] = row["deliveryFileName"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("ingester") != responseFields.end())
				{
					field = "ingester";
					if (row["ingester"].is_null())
						mediaItemRoot[field] = nullptr;
					else
						mediaItemRoot[field] = row["ingester"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("userData") != responseFields.end())
				{
					field = "userData";
					if (row["userData"].is_null())
						mediaItemRoot[field] = nullptr;
					else
						mediaItemRoot[field] = row["userData"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("ingestionDate") != responseFields.end())
				{
					field = "ingestionDate";
					mediaItemRoot[field] = row["formattedIngestionDate"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("startPublishing") != responseFields.end())
				{
					field = "startPublishing";
					mediaItemRoot[field] = row["formattedStartPublishing"].as<string>();
				}
				if (responseFields.empty() || responseFields.find("endPublishing") != responseFields.end())
				{
					field = "endPublishing";
					mediaItemRoot[field] = row["formattedEndPublishing"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("willBeRemovedAt") != responseFields.end())
				{
					field = "willBeRemovedAt";
					mediaItemRoot[field] = row["formattedWillBeRemovedAt"].as<string>();
				}

				ContentType contentType = MMSEngineDBFacade::toContentType(row["contentType"].as<string>());
				if (responseFields.empty() || responseFields.find("contentType") != responseFields.end())
				{
					field = "contentType";
					mediaItemRoot[field] = row["contentType"].as<string>();
				}

				if (responseFields.empty() || responseFields.find("retentionInMinutes") != responseFields.end())
				{
					field = "retentionInMinutes";
					mediaItemRoot[field] = row["retentionInMinutes"].as<int64_t>();
				}

				if (responseFields.empty() || responseFields.find("tags") != responseFields.end())
				{
					json mediaItemTagsRoot = json::array();

					/*
					auto const array{row["tags"].as_sql_array<string>()};
					for (int index = 0; index < array.size(); index++)
						mediaItemTagsRoot.push_back(array[index]);
					*/
					{
						// pqxx::array<string> tagsArray = row["tags"].as<array>();
						auto tagsArray = row["tags"].as_array();
						pair<pqxx::array_parser::juncture, string> elem;
						do
						{
							elem = tagsArray.get_next();
							if (elem.first == pqxx::array_parser::juncture::string_value)
								mediaItemTagsRoot.push_back(elem.second);
						} while (elem.first != pqxx::array_parser::juncture::done);
					}

					field = "tags";
					mediaItemRoot[field] = mediaItemTagsRoot;
				}

				if (responseFields.empty() || responseFields.find("uniqueName") != responseFields.end())
				{
					chrono::milliseconds localSqlDuration(0);
					field = "uniqueName";
					try
					{
						mediaItemRoot[field] =
							externalUniqueName_columnAsString(workspaceKey, "uniquename", "", localMediaItemKey, &localSqlDuration, fromMaster);
					}
					catch (DBRecordNotFound &e)
					{
						mediaItemRoot[field] = "";
					}
					internalSqlDuration += localSqlDuration;
				}

				// CrossReferences
				if (responseFields.empty() || responseFields.find("crossReferences") != responseFields.end())
				{
					// if (contentType == ContentType::Video)
					{
						json mediaItemReferencesRoot = json::array();

						{
							string sqlStatement = std::format(
								"select sourceMediaItemKey, type, parameters "
								"from MMS_CrossReference "
								"where targetMediaItemKey = {}",
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";
								localMediaItemKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							result res = trans.transaction->exec(sqlStatement);
							for (auto row : res)
							{
								json crossReferenceRoot;

								field = "sourceMediaItemKey";
								crossReferenceRoot[field] = row["sourceMediaItemKey"].as<int64_t>();

								field = "type";
								crossReferenceRoot[field] = row["type"].as<string>();

								if (!row["parameters"].is_null())
								{
									string crossReferenceParameters = row["parameters"].as<string>();
									if (crossReferenceParameters != "")
									{
										json crossReferenceParametersRoot = JSONUtils::toJson<json>(crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.push_back(crossReferenceRoot);
							}
							chrono::milliseconds sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
							internalSqlDuration += sqlDuration;
							long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
							SQLQUERYLOG(
								"default", elapsed,
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, trans.connection->getConnectionId(), sqlDuration.count()
							);
						}

						{
							string sqlStatement = std::format(
								"select type, targetMediaItemKey, parameters "
								"from MMS_CrossReference "
								"where sourceMediaItemKey = {}",
								// "where type = 'imageOfVideo' and targetMediaItemKey = ?";
								localMediaItemKey
							);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							result res = trans.transaction->exec(sqlStatement);
							for (auto row : res)
							{
								json crossReferenceRoot;

								field = "type";
								crossReferenceRoot[field] = row["type"].as<string>();

								field = "targetMediaItemKey";
								crossReferenceRoot[field] = row["targetMediaItemKey"].as<int64_t>();

								if (!row["parameters"].is_null())
								{
									string crossReferenceParameters = row["parameters"].as<string>();
									if (crossReferenceParameters != "")
									{
										json crossReferenceParametersRoot = JSONUtils::toJson<json>(crossReferenceParameters);

										field = "parameters";
										crossReferenceRoot[field] = crossReferenceParametersRoot;
									}
								}

								mediaItemReferencesRoot.push_back(crossReferenceRoot);
							}
							chrono::milliseconds sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
							internalSqlDuration += sqlDuration;
							long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
							SQLQUERYLOG(
								"default", elapsed,
								"SQL statement"
								", sqlStatement: @{}@"
								", getConnectionId: @{}@"
								", elapsed (millisecs): @{}@",
								sqlStatement, trans.connection->getConnectionId(), sqlDuration.count()
							);
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

				if (responseFields.empty() || responseFields.find("physicalPaths") != responseFields.end())
				{
					json mediaItemProfilesRoot = json::array();

					string sqlStatement = std::format(
						"select physicalPathKey, durationInMilliSeconds, bitRate, externalReadOnlyStorage, "
						"deliveryInfo ->> 'externalDeliveryTechnology' as externalDeliveryTechnology, "
						"deliveryInfo ->> 'externalDeliveryURL' as externalDeliveryURL, "
						"metaData, fileName, relativePath, partitionNumber, encodingProfileKey, sizeInBytes, retentionInMinutes, "
						"to_char(creationDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as creationDate "
						"from MMS_PhysicalPath where mediaItemKey = {}",
						localMediaItemKey
					);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					result res = trans.transaction->exec(sqlStatement);
					for (auto row : res)
					{
						json profileRoot;

						int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();

						field = "physicalPathKey";
						profileRoot[field] = physicalPathKey;

						field = "durationInMilliSeconds";
						if (row["durationInMilliSeconds"].is_null())
							profileRoot[field] = nullptr;
						else
							profileRoot[field] = row["durationInMilliSeconds"].as<int64_t>();

						field = "bitRate";
						if (row["bitRate"].is_null())
							profileRoot[field] = nullptr;
						else
							profileRoot[field] = row["bitRate"].as<int64_t>();

						field = "fileFormat";
						string fileName = row["fileName"].as<string>();
						size_t extensionIndex = fileName.find_last_of(".");
						string fileExtension;
						if (extensionIndex == string::npos)
							profileRoot[field] = nullptr;
						else
						{
							fileExtension = fileName.substr(extensionIndex + 1);
							if (fileExtension == "m3u8")
								profileRoot[field] = "hls";
							else
								profileRoot[field] = fileExtension;
						}

						field = "metaData";
						if (row["metaData"].is_null())
							profileRoot[field] = nullptr;
						else
							profileRoot[field] = row["metaData"].as<string>();

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
							profileRoot[field] = nullptr;
						else
						{
							externalDeliveryTechnology = row["externalDeliveryTechnology"].as<string>();
							profileRoot[field] = externalDeliveryTechnology;
						}

						field = "externalDeliveryURL";
						if (row["externalDeliveryURL"].is_null())
							profileRoot[field] = nullptr;
						else
							profileRoot[field] = row["externalDeliveryURL"].as<string>();

						field = "encodingProfileKey";
						if (row["encodingProfileKey"].is_null())
						{
							profileRoot[field] = nullptr;

							field = "deliveryTechnology";
							if (externalDeliveryTechnology == "hls")
							{
								profileRoot[field] = MMSEngineDBFacade::toString(MMSEngineDBFacade::DeliveryTechnology::HTTPStreaming);
							}
							else
							{
								MMSEngineDBFacade::DeliveryTechnology deliveryTechnology =
									MMSEngineDBFacade::fileFormatToDeliveryTechnology(fileExtension);
								profileRoot[field] = MMSEngineDBFacade::toString(deliveryTechnology);
							}

							field = "encodingProfileLabel";
							profileRoot[field] = nullptr;
						}
						else
						{
							int64_t encodingProfileKey = row["encodingProfileKey"].as<int64_t>();

							profileRoot[field] = encodingProfileKey;

							string label;
							MMSEngineDBFacade::ContentType contentType;
							MMSEngineDBFacade::DeliveryTechnology deliveryTechnology;

							chrono::system_clock::time_point startMethod = chrono::system_clock::now();
							tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
								getEncodingProfileDetailsByKey(workspaceKey, encodingProfileKey);
							internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startMethod);

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
							profileRoot[field] = nullptr;
						else
							profileRoot[field] = row["retentionInMinutes"].as<int64_t>();

						if (contentType == ContentType::Video)
						{
							vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							chrono::system_clock::time_point startMethod = chrono::system_clock::now();
							getVideoDetails(localMediaItemKey, physicalPathKey, fromMaster, videoTracks, audioTracks);
							internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startMethod);
							SPDLOG_INFO(
								"getVideoDetails"
								", mediaItemKey: {}"
								", physicalPathKey: {}"
								", videoTracks.size: {}"
								", audioTracks.size: {}",
								localMediaItemKey, physicalPathKey, videoTracks.size(), audioTracks.size()
							);

							{
								json videoTracksRoot = json::array();

								for (tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack : videoTracks)
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

									tie(videoTrackKey, trackIndex, durationInMilliSeconds, width, height, avgFrameRate, codecName, bitRate, profile) =
										videoTrack;

									json videoTrackRoot;

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
									videoTrackRoot[field] = (int64_t)bitRate;

									field = "profile";
									videoTrackRoot[field] = profile;

									videoTracksRoot.push_back(videoTrackRoot);
								}

								field = "videoTracks";
								profileRoot[field] = videoTracksRoot;
							}

							{
								json audioTracksRoot = json::array();

								for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack : audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language) =
										audioTrack;

									json audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t)bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t)sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t)channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.push_back(audioTrackRoot);
								}

								field = "audioTracks";
								profileRoot[field] = audioTracksRoot;
							}
						}
						else if (contentType == ContentType::Audio)
						{
							vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

							chrono::system_clock::time_point startMethod = chrono::system_clock::now();
							getAudioDetails(localMediaItemKey, physicalPathKey, fromMaster, audioTracks);
							internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startMethod);

							{
								json audioTracksRoot = json::array();

								for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack : audioTracks)
								{
									int64_t audioTrackKey;
									int trackIndex;
									int64_t durationInMilliSeconds;
									long bitRate;
									string codecName;
									long sampleRate;
									int channels;
									string language;

									tie(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language) =
										audioTrack;

									json audioTrackRoot;

									field = "audioTrackKey";
									audioTrackRoot[field] = audioTrackKey;

									field = "trackIndex";
									audioTrackRoot[field] = trackIndex;

									field = "durationInMilliSeconds";
									audioTrackRoot[field] = durationInMilliSeconds;

									field = "bitRate";
									audioTrackRoot[field] = (int64_t)bitRate;

									field = "codecName";
									audioTrackRoot[field] = codecName;

									field = "sampleRate";
									audioTrackRoot[field] = (int64_t)sampleRate;

									field = "channels";
									audioTrackRoot[field] = (int64_t)channels;

									field = "language";
									audioTrackRoot[field] = language;

									audioTracksRoot.push_back(audioTrackRoot);
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

							chrono::system_clock::time_point startMethod = chrono::system_clock::now();
							tuple<int, int, string, int> imageDetails = getImageDetails(localMediaItemKey, physicalPathKey, fromMaster);
							internalSqlDuration += chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startMethod);

							tie(width, height, format, quality) = imageDetails;

							json imageDetailsRoot;

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
							string errorMessage = std::format(
								"ContentType unmanaged"
								", mediaItemKey: {}"
								", sqlStatement: {}",
								localMediaItemKey, sqlStatement
							);
							SPDLOG_ERROR(errorMessage);

							throw runtime_error(errorMessage);
						}

						mediaItemProfilesRoot.push_back(profileRoot);
					}
					chrono::milliseconds sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
					internalSqlDuration += sqlDuration;
					long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
					SQLQUERYLOG(
						"default", elapsed,
						"SQL statement"
						", sqlStatement: @{}@"
						", getConnectionId: @{}@"
						", elapsed (millisecs): @{}@",
						sqlStatement, trans.connection->getConnectionId(), sqlDuration.count()
					);

					field = "physicalPaths";
					mediaItemRoot[field] = mediaItemProfilesRoot;
				}

				mediaItemsRoot.push_back(mediaItemRoot);
			}
			long elapsed = chrono::duration_cast<chrono::milliseconds>((chrono::system_clock::now() - startSql) - internalSqlDuration).count();
			SQLQUERYLOG(
				"getMediaItemsList", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@getMediaItemsList@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
		}

		field = "mediaItems";
		responseRoot[field] = mediaItemsRoot;

		field = "response";
		mediaItemsListRoot[field] = responseRoot;
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

	return mediaItemsListRoot;
}

json MMSEngineDBFacade::mediaItem_columnAsJson(string columnName, int64_t mediaItemKey, chrono::milliseconds *sqlDuration, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_mediaitem:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = mediaItemQuery(requestedColumns, mediaItemKey, fromMaster);

		if (sqlDuration != nullptr)
			*sqlDuration = sqlResultSet->getSqlDuration();

		return (*sqlResultSet)[0][0].as<json>(json());
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_ERROR(
			"NotFound"
			", physicalPathKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			physicalPathKey, fromMaster, e.what()
		);
		*/

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", mediaItemKey: {}"
			", fromMaster: {}",
			mediaItemKey, fromMaster
		);

		throw;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::mediaItemQuery(
	vector<string> &requestedColumns, int64_t mediaItemKey, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
)
{
	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = std::format(
				"Too many rows requested"
				", rows: {}"
				", maxRows: {}",
				rows, _maxRows
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else if ((startIndex != -1 || rows != -1) && orderBy == "")
		{
			// The query optimizer takes LIMIT into account when generating query plans, so you are very likely to get different plans (yielding
			// different row orders) depending on what you give for LIMIT and OFFSET. Thus, using different LIMIT/OFFSET values to select different
			// subsets of a query result will give inconsistent results unless you enforce a predictable result ordering with ORDER BY. This is not a
			// bug; it is an inherent consequence of the fact that SQL does not promise to deliver the results of a query in any particular order
			// unless ORDER BY is used to constrain the order. The rows skipped by an OFFSET clause still have to be computed inside the server;
			// therefore a large OFFSET might be inefficient.
			string errorMessage = std::format(
				"Using startIndex/row without orderBy will give inconsistent results"
				", startIndex: {}"
				", rows: {}"
				", orderBy: {}",
				startIndex, rows, orderBy
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (mediaItemKey != -1)
				where += std::format("{} mediaItemKey = {} ", where.size() > 0 ? "and" : "", mediaItemKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_MediaItem "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (empty(res) && mediaItemKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"mediaItemKey not found"
					", mediaItemKey: {}",
					mediaItemKey
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		DBRecordNotFound const *de = dynamic_cast<DBRecordNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (de != nullptr) // il chaimante decidera se loggarlo come error
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
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

int64_t MMSEngineDBFacade::physicalPath_columnAsInt64(string columnName, int64_t physicalPathKey, chrono::milliseconds *sqlDuration, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_physicalpath:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = physicalPathQuery(requestedColumns, physicalPathKey, fromMaster);

		if (sqlDuration != nullptr)
			*sqlDuration = sqlResultSet->getSqlDuration();

		return (*sqlResultSet)[0][0].as<int64_t>(static_cast<int64_t>(-1));
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_ERROR(
			"NotFound"
			", physicalPathKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			physicalPathKey, fromMaster, e.what()
		);
		*/

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", physicalPathKey: {}"
			", fromMaster: {}",
			physicalPathKey, fromMaster
		);

		throw;
	}
}

json MMSEngineDBFacade::physicalPath_columnAsJson(string columnName, int64_t physicalPathKey, chrono::milliseconds *sqlDuration, bool fromMaster)
{
	try
	{
		string requestedColumn = std::format("mms_physicalpath:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = physicalPathQuery(requestedColumns, physicalPathKey, fromMaster);

		if (sqlDuration != nullptr)
			*sqlDuration = sqlResultSet->getSqlDuration();

		return (*sqlResultSet)[0][0].as<json>(json());
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_ERROR(
			"NotFound"
			", physicalPathKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			physicalPathKey, fromMaster, e.what()
		);
		*/

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", physicalPathKey: {}"
			", fromMaster: {}",
			physicalPathKey, fromMaster
		);

		throw;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::physicalPathQuery(
	vector<string> &requestedColumns, int64_t physicalPathKey, bool fromMaster, int startIndex, int rows, string orderBy, bool notFoundAsException
)
{
	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = std::format(
				"Too many rows requested"
				", rows: {}"
				", maxRows: {}",
				rows, _maxRows
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else if ((startIndex != -1 || rows != -1) && orderBy == "")
		{
			// The query optimizer takes LIMIT into account when generating query plans, so you are very likely to get different plans (yielding
			// different row orders) depending on what you give for LIMIT and OFFSET. Thus, using different LIMIT/OFFSET values to select different
			// subsets of a query result will give inconsistent results unless you enforce a predictable result ordering with ORDER BY. This is not a
			// bug; it is an inherent consequence of the fact that SQL does not promise to deliver the results of a query in any particular order
			// unless ORDER BY is used to constrain the order. The rows skipped by an OFFSET clause still have to be computed inside the server;
			// therefore a large OFFSET might be inefficient.
			string errorMessage = std::format(
				"Using startIndex/row without orderBy will give inconsistent results"
				", startIndex: {}"
				", rows: {}"
				", orderBy: {}",
				startIndex, rows, orderBy
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (physicalPathKey != -1)
				where += std::format("{} physicalPathKey = {} ", where.size() > 0 ? "and" : "", physicalPathKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_PhysicalPath "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (empty(res) && physicalPathKey != -1 && notFoundAsException)
			{
				string errorMessage = std::format(
					"physicalPath not found"
					", physicalPathKey: {}",
					physicalPathKey
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		DBRecordNotFound const *de = dynamic_cast<DBRecordNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (de != nullptr) // il chaimante decidera se loggarlo come error
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
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

string MMSEngineDBFacade::externalUniqueName_columnAsString(
	int64_t workspaceKey, string columnName, string uniqueName, int64_t mediaItemKey, chrono::milliseconds *sqlDuration, bool fromMaster
)
{
	try
	{
		string requestedColumn = std::format("mms_externaluniquename:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet =
			externalUniqueNameQuery(requestedColumns, workspaceKey, uniqueName, mediaItemKey, fromMaster);

		if (sqlDuration != nullptr)
			*sqlDuration = sqlResultSet->getSqlDuration();

		if ((*sqlResultSet).size() == 0)
		{
			string errorMessage = std::format(
				"workspaceKey/mediaItemKey not found"
				", workspaceKey: {}"
				", mediaItemKey: {}",
				workspaceKey, mediaItemKey
			);
			throw DBRecordNotFound(errorMessage);
		}

		return (*sqlResultSet)[0][0].as<string>("");
	}
	catch (DBRecordNotFound &e)
	{
		/*
		SPDLOG_WARN(
			"NotFound"
			", workspaceKey: {}"
			", mediaItemKey: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, mediaItemKey, fromMaster, e.what()
		);
		*/

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", mediaItemKey: {}"
			", fromMaster: {}",
			workspaceKey, mediaItemKey, fromMaster
		);

		throw;
	}
}

int64_t MMSEngineDBFacade::externalUniqueName_columnAsInt64(
	int64_t workspaceKey, string columnName, string uniqueName, int64_t mediaItemKey, chrono::milliseconds *sqlDuration, bool fromMaster
)
{
	try
	{
		string requestedColumn = std::format("mms_externaluniquename:.{}", columnName);
		vector<string> requestedColumns = vector<string>(1, requestedColumn);
		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet =
			externalUniqueNameQuery(requestedColumns, workspaceKey, uniqueName, mediaItemKey, fromMaster);

		if (sqlDuration != nullptr)
			*sqlDuration = sqlResultSet->getSqlDuration();

		if ((*sqlResultSet).size() == 0)
		{
			string errorMessage = std::format(
				"workspaceKey/mediaItemKey not found"
				", workspaceKey: {}"
				", mediaItemKey: {}",
				workspaceKey, mediaItemKey
			);
			throw DBRecordNotFound(errorMessage);
		}

		return (*sqlResultSet)[0][0].as<int64_t>(-1);
	}
	catch (DBRecordNotFound &e)
	{
		SPDLOG_ERROR(
			"NotFound"
			", workspaceKey: {}"
			", uniqueName: {}"
			", fromMaster: {}"
			", exceptionMessage: {}",
			workspaceKey, uniqueName, fromMaster, e.what()
		);

		throw;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"exception"
			", workspaceKey: {}"
			", uniqueName: {}"
			", fromMaster: {}",
			workspaceKey, uniqueName, fromMaster
		);

		throw;
	}
}

shared_ptr<PostgresHelper::SqlResultSet> MMSEngineDBFacade::externalUniqueNameQuery(
	vector<string> &requestedColumns, int64_t workspaceKey, string uniqueName, int64_t mediaItemKey, bool fromMaster, int startIndex, int rows,
	string orderBy, bool notFoundAsException
)
{
	/*
	shared_ptr<PostgresConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<PostgresConnection>> connectionPool = fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool;

	conn = connectionPool->borrow();
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	nontransaction trans{*(conn->_sqlConnection)};
	*/
	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (rows > _maxRows)
		{
			string errorMessage = std::format(
				"Too many rows requested"
				", rows: {}"
				", maxRows: {}",
				rows, _maxRows
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		else if ((startIndex != -1 || rows != -1) && orderBy == "")
		{
			// The query optimizer takes LIMIT into account when generating query plans, so you are very likely to get different plans (yielding
			// different row orders) depending on what you give for LIMIT and OFFSET. Thus, using different LIMIT/OFFSET values to select different
			// subsets of a query result will give inconsistent results unless you enforce a predictable result ordering with ORDER BY. This is not a
			// bug; it is an inherent consequence of the fact that SQL does not promise to deliver the results of a query in any particular order
			// unless ORDER BY is used to constrain the order. The rows skipped by an OFFSET clause still have to be computed inside the server;
			// therefore a large OFFSET might be inefficient.
			string errorMessage = std::format(
				"Using startIndex/row without orderBy will give inconsistent results"
				", startIndex: {}"
				", rows: {}"
				", orderBy: {}",
				startIndex, rows, orderBy
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet;
		{
			string where;
			if (workspaceKey != -1)
				where += std::format("{} workspaceKey = {} ", where.size() > 0 ? "and" : "", workspaceKey);
			if (uniqueName != "")
				where += std::format("{} uniqueName = {} ", where.size() > 0 ? "and" : "", trans.transaction->quote(uniqueName));
			if (mediaItemKey != -1)
				where += std::format("{} mediaItemKey = {} ", where.size() > 0 ? "and" : "", mediaItemKey);

			string limit;
			string offset;
			string orderByCondition;
			if (rows != -1)
				limit = std::format("limit {} ", rows);
			if (startIndex != -1)
				offset = std::format("offset {} ", startIndex);
			if (orderBy != "")
				orderByCondition = std::format("order by {} ", orderBy);

			string sqlStatement = std::format(
				"select {} "
				"from MMS_ExternalUniqueName "
				"{} {} "
				"{} {} {}",
				_postgresHelper.buildQueryColumns(requestedColumns), where.size() > 0 ? "where " : "", where, limit, offset, orderByCondition
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);

			sqlResultSet = _postgresHelper.buildResult(res);

			chrono::system_clock::time_point endSql = chrono::system_clock::now();
			sqlResultSet->setSqlDuration(chrono::duration_cast<chrono::milliseconds>(endSql - startSql));
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(endSql - startSql).count()
			);

			if (empty(res) && workspaceKey != -1 && uniqueName != "" && notFoundAsException)
			{
				string errorMessage = std::format(
					"workspaceKey/uniqueName not found"
					", physicalPathKey: {}"
					", uniqueName: {}",
					workspaceKey, uniqueName
				);
				// abbiamo il log nel catch
				// SPDLOG_WARN(errorMessage);

				throw DBRecordNotFound(errorMessage);
			}
		}

		return sqlResultSet;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		DBRecordNotFound const *de = dynamic_cast<DBRecordNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (de != nullptr)
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				de->what(), trans.connection->getConnectionId()
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

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
	int64_t referenceMediaItemKey,
	// encodingProfileKey == -1 means it is requested the source file (the one having 'ts' file format and bigger size in case there are more than
	// one)
	int64_t encodingProfileKey, bool warningIfMissing, bool fromMaster
)
{
	int64_t physicalPathKey = -1;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (encodingProfileKey != -1)
		{
			string sqlStatement = std::format(
				"select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} "
				"and encodingProfileKey = {}",
				referenceMediaItemKey, encodingProfileKey
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
				physicalPathKey = res[0]["physicalPathKey"].as<int64_t>();

			if (physicalPathKey == -1)
			{
				string errorMessage = std::format(
					"MediaItemKey/encodingProfileKey are not found"
					", mediaItemKey: {}"
					", encodingProfileKey: {}"
					", sqlStatement: {}",
					referenceMediaItemKey, encodingProfileKey, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}
		else
		{
			tuple<int64_t, int, string, string, uint64_t, bool, int64_t> sourcePhysicalPathDetails =
				getSourcePhysicalPath(referenceMediaItemKey, fromMaster);
			tie(physicalPathKey, ignore, ignore, ignore, ignore, ignore, ignore) = sourcePhysicalPathDetails;
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

	return physicalPathKey;
}

int64_t MMSEngineDBFacade::getPhysicalPathDetails(
	int64_t workspaceKey, int64_t mediaItemKey, ContentType contentType, string encodingProfileLabel, bool warningIfMissing, bool fromMaster
)
{
	int64_t physicalPathKey = -1;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		int64_t encodingProfileKey = -1;
		{
			string sqlStatement = std::format(
				"select encodingProfileKey from MMS_EncodingProfile "
				"where (workspaceKey = {} or workspaceKey is null) and "
				"contentType = {} and label = {}",
				workspaceKey, trans.transaction->quote(toString(contentType)), trans.transaction->quote(encodingProfileLabel)
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
				encodingProfileKey = res[0]["encodingProfileKey"].as<int64_t>();
			else
			{
				string errorMessage = std::format(
					"encodingProfileKey is not found"
					", workspaceKey: {}"
					", contentType: {}"
					", encodingProfileLabel: {}"
					", sqlStatement: {}",
					workspaceKey, toString(contentType), encodingProfileLabel, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} "
				"and encodingProfileKey = {}",
				mediaItemKey, encodingProfileKey
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
				physicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
			else
			{
				string errorMessage = std::format(
					"MediaItemKey/encodingProfileKey are not found"
					", mediaItemKey: {}"
					", encodingProfileKey: {}"
					", sqlStatement: {}",
					mediaItemKey, encodingProfileKey, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

	return physicalPathKey;
}

string MMSEngineDBFacade::getPhysicalPathDetails(int64_t physicalPathKey, bool warningIfMissing, bool fromMaster)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		string metaData;
		{
			string sqlStatement = std::format(
				"select metaData "
				"from MMS_PhysicalPath p "
				"where physicalPathKey = {}",
				physicalPathKey
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
				if (!res[0]["metaData"].is_null())
					metaData = res[0]["metaData"].as<string>();
			}
			else
			{
				string errorMessage = std::format(
					"physicalPathKey is not found"
					", physicalPathKey: {}"
					", sqlStatement: {}",
					physicalPathKey, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		return metaData;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

tuple<int64_t, int, string, string, uint64_t, bool, int64_t> MMSEngineDBFacade::getSourcePhysicalPath(int64_t mediaItemKey, bool fromMaster)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
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

		uint64_t sizeInBytesWithEncodingProfile;
		uint64_t sizeInBytesWithoutEncodingProfile;

		int64_t durationInMilliSecondsWithEncodingProfile = 0;
		int64_t durationInMilliSecondsWithoutEncodingProfile = 0;

		uint64_t maxSizeInBytesWithEncodingProfile = -1;
		uint64_t maxSizeInBytesWithoutEncodingProfile = -1;

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
			string sqlStatement = std::format(
				"select physicalPathKey, sizeInBytes, fileName, relativePath, partitionNumber, "
				"externalReadOnlyStorage, durationInMilliSeconds, encodingProfileKey "
				"from MMS_PhysicalPath where mediaItemKey = {}",
				mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			string selectedFileFormatWithEncodingProfile;
			string selectedFileFormatWithoutEncodingProfile;
			for (auto row : res)
			{
				uint64_t localSizeInBytes = row["sizeInBytes"].as<uint64_t>();

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
					sizeInBytesWithoutEncodingProfile = row["sizeInBytes"].as<uint64_t>();
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
					sizeInBytesWithEncodingProfile = row["sizeInBytes"].as<uint64_t>();
					if (!row["durationInMilliSeconds"].is_null())
						durationInMilliSecondsWithEncodingProfile = row["durationInMilliSeconds"].as<int64_t>();

					fileNameWithEncodingProfile = localFileName;
					maxSizeInBytesWithEncodingProfile = localSizeInBytes;
					selectedFileFormatWithEncodingProfile = localFileFormat;
				}
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

			if (maxSizeInBytesWithoutEncodingProfile == -1 && maxSizeInBytesWithEncodingProfile == -1)
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				// 2024-08-17: warn, sara' il chiamante che deciderà se loggare o no l'errore
				// if (warningIfMissing)
				SPDLOG_WARN(errorMessage);
				// else
				// 	SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		// senza encoding profile ha priorità rispetto a 'con encoding profile'
		if (maxSizeInBytesWithoutEncodingProfile != -1)
			return make_tuple(
				physicalPathKeyWithoutEncodingProfile, mmsPartitionNumberWithoutEncodingProfile, relativePathWithoutEncodingProfile,
				fileNameWithoutEncodingProfile, sizeInBytesWithoutEncodingProfile, externalReadOnlyStorageWithoutEncodingProfile,
				durationInMilliSecondsWithoutEncodingProfile
			);
		else
			return make_tuple(
				physicalPathKeyWithEncodingProfile, mmsPartitionNumberWithEncodingProfile, relativePathWithEncodingProfile,
				fileNameWithEncodingProfile, sizeInBytesWithEncodingProfile, externalReadOnlyStorageWithEncodingProfile,
				durationInMilliSecondsWithEncodingProfile
			);
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
			SPDLOG_WARN(
				"query failed"
				", exceptionMessage: {}"
				", conn: {}",
				me->what(), trans.connection->getConnectionId()
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

tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t>
MMSEngineDBFacade::getMediaItemKeyDetails(int64_t workspaceKey, int64_t mediaItemKey, bool warningIfMissing, bool fromMaster)
{
	tuple<MMSEngineDBFacade::ContentType, string, string, string, int64_t, int64_t> contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select contentType, title, userData, ingestionJobKey, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate, "
				"EXTRACT(EPOCH FROM (willBeRemovedAt_virtual - NOW() at time zone 'utc')) as willBeRemovedInSeconds "
				"from MMS_MediaItem where workspaceKey = {} and mediaItemKey = {}",
				workspaceKey, mediaItemKey
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
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		return contentTypeTitleUserDataIngestionDateRemovedInAndIngestionJobKey;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t>
MMSEngineDBFacade::getMediaItemKeyDetailsByPhysicalPathKey(int64_t workspaceKey, int64_t physicalPathKey, bool warningIfMissing, bool fromMaster)
{
	tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select mi.mediaItemKey, mi.contentType, mi.title, mi.userData, "
				"mi.ingestionJobKey, p.fileName, p.relativePath, p.durationInMilliSeconds, "
				"to_char(ingestionDate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') as ingestionDate "
				"from MMS_MediaItem mi, MMS_PhysicalPath p "
				"where mi.workspaceKey = {} and mi.mediaItemKey = p.mediaItemKey "
				"and p.physicalPathKey = {}",
				workspaceKey, physicalPathKey
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
				int64_t mediaItemKey = res[0]["mediaItemKey"].as<int64_t>();
				MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());

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

				mediaItemDetails = make_tuple(
					mediaItemKey, contentType, title, userData, ingestionDate, ingestionJobKey, fileName, relativePath, durationInMilliSeconds
				);
			}
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", physicalPathKey: {}"
					", sqlStatement: {}",
					physicalPathKey, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		return mediaItemDetails;
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

void MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey(
	int64_t workspaceKey, int64_t referenceIngestionJobKey, int maxLastMediaItemsToBeReturned,
	vector<tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType>> &mediaItemsDetails, bool warningIfMissing, bool fromMaster
)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		IngestionType ingestionType;
		{
			string sqlStatement = std::format(
				"select ingestionType from MMS_IngestionJob "
				"where ingestionJobKey = {} ",
				referenceIngestionJobKey
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
				ingestionType = MMSEngineDBFacade::toIngestionType(res[0]["ingestionType"].as<string>());
			else
			{
				string errorMessage = std::format(
					"IngestionJob is not found"
					", referenceIngestionJobKey: {}"
					", sqlStatement: {}",
					referenceIngestionJobKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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
					orderBy = "order by mi.utcStartTimeInMilliSecs_virtual asc ";
				else
					orderBy = "order by JSON_EXTRACT(mi.userData, '$.mmsData.utcChunkStartTime') asc ";
			}
			else
			{
				// vedi commento aggiunto con label 2024-01-05 in MMSEngineDBFacade_IngestionJobs_Postgres.cpp
				// orderBy = "order by ijo.mediaItemKey desc ";
				orderBy = "order by ijo.position asc ";
			}

			string sqlStatement = std::format(
				"select ijo.mediaItemKey, ijo.physicalPathKey "
				"from MMS_IngestionJobOutput ijo, MMS_MediaItem mi "
				"where mi.workspaceKey = {} and ijo.mediaItemKey = mi.mediaItemKey "
				"and ijo.ingestionJobKey = {} {} ",
				workspaceKey, referenceIngestionJobKey, orderBy
			);
			if (maxLastMediaItemsToBeReturned != -1)
				sqlStatement += ("limit " + to_string(maxLastMediaItemsToBeReturned));
			/*
			lastSQLCommand =
				"select mediaItemKey, physicalPathKey "
				"from MMS_IngestionJobOutput "
				"where ingestionJobKey = ? order by mediaItemKey";
			*/
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
			{
				int64_t mediaItemKey = row["mediaItemKey"].as<int64_t>();
				int64_t physicalPathKey = row["physicalPathKey"].as<int64_t>();

				ContentType contentType;
				{
					string sqlStatement = std::format("select contentType from MMS_MediaItem where mediaItemKey = {}", mediaItemKey);
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
						contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
					else
					{
						string errorMessage = std::format(
							"MediaItemKey is not found"
							", referenceIngestionJobKey: {}"
							", mediaItemKey: {}"
							", sqlStatement: {}",
							referenceIngestionJobKey, mediaItemKey, sqlStatement
						);
						if (warningIfMissing)
						{
							SPDLOG_WARN(errorMessage);

							continue;
						}
						else
						{
							SPDLOG_ERROR(errorMessage);

							throw MediaItemKeyNotFound(errorMessage);
						}
					}
				}

				tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyPhysicalPathKeyAndContentType =
					make_tuple(mediaItemKey, physicalPathKey, contentType);
				// 2024-01-05: sostituito mediaItemsDetails.begin() con mediaItemsDetails.end() per rispottare
				//	l'ordine del campo position di ingestionJobOutput
				mediaItemsDetails.insert(mediaItemsDetails.end(), mediaItemKeyPhysicalPathKeyAndContentType);
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
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

pair<int64_t, MMSEngineDBFacade::ContentType>
MMSEngineDBFacade::getMediaItemKeyDetailsByUniqueName(int64_t workspaceKey, string referenceUniqueName, bool warningIfMissing, bool fromMaster)
{
	pair<int64_t, MMSEngineDBFacade::ContentType> mediaItemKeyAndContentType;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		{
			string sqlStatement = std::format(
				"select mi.mediaItemKey, mi.contentType "
				"from MMS_MediaItem mi, MMS_ExternalUniqueName eun "
				"where mi.mediaItemKey = eun.mediaItemKey "
				"and eun.workspaceKey = {} and eun.uniqueName = {}",
				workspaceKey, trans.transaction->quote(referenceUniqueName)
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
				mediaItemKeyAndContentType.first = res[0]["mediaItemKey"].as<int64_t>();
				mediaItemKeyAndContentType.second = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
			}
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", referenceUniqueName: {}"
					", sqlStatement: {}",
					referenceUniqueName, sqlStatement
				);
				if (warningIfMissing)
					SPDLOG_WARN(errorMessage);
				else
					SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}
	}
	catch (exception const &e)
	{
		sql_error const *se = dynamic_cast<sql_error const *>(&e);
		MediaItemKeyNotFound const *me = dynamic_cast<MediaItemKeyNotFound const *>(&e);
		if (se != nullptr)
			SPDLOG_ERROR(
				"query failed"
				", query: {}"
				", exceptionMessage: {}"
				", conn: {}",
				se->query(), se->what(), trans.connection->getConnectionId()
			);
		else if (me != nullptr)
		{
			if (warningIfMissing)
				SPDLOG_WARN(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
			else
				SPDLOG_ERROR(
					"query failed"
					", exceptionMessage: {}"
					", conn: {}",
					me->what(), trans.connection->getConnectionId()
				);
		}
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

	return mediaItemKeyAndContentType;
}

int64_t MMSEngineDBFacade::getMediaDurationInMilliseconds(
	// mediaItemKey or physicalPathKey has to be initialized, the other has to be -1
	int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster
)
{
	int64_t durationInMilliSeconds;

	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		if (physicalPathKey == -1)
		{
			string sqlStatement = std::format(
				"select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where mediaItemKey = {} and encodingProfileKey is null",
				mediaItemKey
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
				IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(res[0]["status"].as<string>());
				if (res[0]["durationInMilliSeconds"].is_null())
				{
					string errorMessage = std::format(
						"duration is not found"
						", mediaItemKey: {}"
						", sqlStatement: {}",
						mediaItemKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();
			}
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else
		{
			string sqlStatement = std::format(
				"select durationInMilliSeconds "
				"from MMS_PhysicalPath "
				"where physicalPathKey = {}",
				physicalPathKey
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
				if (res[0]["durationInMilliSeconds"].is_null())
				{
					string errorMessage = std::format(
						"duration is not found"
						", physicalPathKey: {}"
						", sqlStatement: {}",
						physicalPathKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				durationInMilliSeconds = res[0]["durationInMilliSeconds"].as<int64_t>();
			}
			else
			{
				string errorMessage = std::format(
					"physicalPathKey is not found"
					", physicalPathKey: {}"
					", sqlStatement: {}",
					physicalPathKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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

	return durationInMilliSeconds;
}

void MMSEngineDBFacade::getVideoDetails(
	int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster,
	vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> &videoTracks,
	vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> &audioTracks
)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		int64_t localPhysicalPathKey;

		if (physicalPathKey == -1)
		{
			string sqlStatement =
				std::format("select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null", mediaItemKey);
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
				localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"select videoTrackKey, trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
				"codecName, profile, bitRate "
				"from MMS_VideoTrack where physicalPathKey = {}",
				localPhysicalPathKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
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

				videoTracks.push_back(
					make_tuple(videoTrackKey, trackIndex, durationInMilliSeconds, width, height, avgFrameRate, codecName, bitRate, profile)
				);
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

		{
			string sqlStatement = std::format(
				"select audioTrackKey, trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language "
				"from MMS_AudioTrack where physicalPathKey = {}",
				localPhysicalPathKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
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

				audioTracks.push_back(
					make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language)
				);
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

void MMSEngineDBFacade::getAudioDetails(
	int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster, vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> &audioTracks
)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		int64_t localPhysicalPathKey;

		if (physicalPathKey == -1)
		{
			string sqlStatement =
				std::format("select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null", mediaItemKey);
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
				localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		else
		{
			localPhysicalPathKey = physicalPathKey;
		}

		audioTracks.clear();

		{
			string sqlStatement = std::format(
				"select audioTrackKey, trackIndex, durationInMilliSeconds, "
				"codecName, bitRate, sampleRate, channels, language "
				"from MMS_AudioTrack where physicalPathKey = {}",
				localPhysicalPathKey
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

				audioTracks.push_back(
					make_tuple(audioTrackKey, trackIndex, durationInMilliSeconds, bitRate, codecName, sampleRate, channels, language)
				);
			}
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", localPhysicalPathKey: {}"
					", sqlStatement: {}",
					mediaItemKey, localPhysicalPathKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
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

tuple<int, int, string, int> MMSEngineDBFacade::getImageDetails(int64_t mediaItemKey, int64_t physicalPathKey, bool fromMaster)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	try
	{
		int64_t localPhysicalPathKey;

		if (physicalPathKey == -1)
		{
			string sqlStatement =
				std::format("select physicalPathKey from MMS_PhysicalPath where mediaItemKey = {} and encodingProfileKey is null", mediaItemKey);
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
				localPhysicalPathKey = res[0]["physicalPathKey"].as<int64_t>();
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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
			string sqlStatement = std::format(
				"select width, height, format, quality "
				"from MMS_ImageItemProfile where physicalPathKey = {}",
				localPhysicalPathKey
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
				width = res[0]["width"].as<int>();
				height = res[0]["height"].as<int>();
				format = res[0]["format"].as<string>();
				quality = res[0]["quality"].as<int>();
			}
			else
			{
				string errorMessage = std::format(
					"MediaItemKey is not found"
					", mediaItemKey: {}"
					", sqlStatement: {}",
					mediaItemKey, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

				throw MediaItemKeyNotFound(errorMessage);
			}
		}

		return make_tuple(width, height, format, quality);
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

pair<int64_t, int64_t> MMSEngineDBFacade::saveSourceContentMetadata(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey, bool ingestionRowToBeUpdatedAsSuccess, MMSEngineDBFacade::ContentType contentType,
	int64_t encodingProfileKey, json parametersRoot, bool externalReadOnlyStorage, string relativePath, string mediaSourceFileName,
	int mmsPartitionIndexUsed, unsigned long sizeInBytes,

	// video-audio
	tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

	// image
	int imageWidth, int imageHeight, string imageFormat, int imageQuality
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
	pair<int64_t, int64_t> mediaItemKeyAndPhysicalPathKey;
	string title = "";
	try
	{
		SPDLOG_INFO("Insert into MMS_MediaItem");
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
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				// 2020-03-15: when it is set by the GUI it arrive here as a string
				if ((parametersRoot[field]).type() == json::value_t::string)
					userData = JSONUtils::asString(parametersRoot, field, "");
				else
					userData = JSONUtils::toString(parametersRoot[field]);
			}

			field = "deliveryFileName";
			deliveryFileName = JSONUtils::asString(parametersRoot, field, "");

			field = "retention";
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				string retention = JSONUtils::asString(parametersRoot, field, "1d");
				retentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
			}

			string startPublishing = "NOW";
			string endPublishing = "FOREVER";
			{
				field = "publishing";
				if (JSONUtils::isPresent(parametersRoot, field))
				{
					json publishingRoot = parametersRoot[field];

					field = "startPublishing";
					startPublishing = JSONUtils::asString(publishingRoot, field, "NOW");

					field = "endPublishing";
					endPublishing = JSONUtils::asString(publishingRoot, field, "FOREVER");
				}

				if (startPublishing == "NOW")
				{
					tm tmDateTime;
					// char strUtcDateTime[64];
					string strUtcDateTime;

					chrono::system_clock::time_point now = chrono::system_clock::now();
					time_t utcTime = chrono::system_clock::to_time_t(now);

					gmtime_r(&utcTime, &tmDateTime);

					/*
					sprintf(
						strUtcDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
						tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
					);
					*/
					strUtcDateTime = std::format(
						"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
						tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
					);

					startPublishing = strUtcDateTime;
				}

				if (endPublishing == "FOREVER")
				{
					tm tmDateTime;
					// char strUtcDateTime[64];
					string strUtcDateTime;

					chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 10);

					time_t utcTime = chrono::system_clock::to_time_t(forever);

					gmtime_r(&utcTime, &tmDateTime);

					/*
					sprintf(
						strUtcDateTime, "%04d-%02d-%02dT%02d:%02d:%02dZ", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
						tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
					);
					*/
					strUtcDateTime = std::format(
						"{:0>4}-{:0>2}-{:0>2}T{:0>2}:{:0>2}:{:0>2}Z", tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday,
						tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec
					);

					endPublishing = strUtcDateTime;
				}
			}

			string tags;
			{
				json tagsRoot;
				string field = "tags";
				if (JSONUtils::isPresent(parametersRoot, field))
					tagsRoot = parametersRoot[field];
				tags = getPostgresArray(tagsRoot, true, trans);
			}

			string sqlStatement = std::format(
				"insert into MMS_MediaItem (mediaItemKey, workspaceKey, title, ingester, userData, "
				"deliveryFileName, ingestionJobKey, ingestionDate, contentType, "
				"startPublishing, endPublishing, "
				"retentionInMinutes, tags, markedAsRemoved, processorMMSForRetention) values ("
				"DEFAULT,      {},           {},     {},      {}, "
				"{},               {},              NOW() at time zone 'utc', {}, "
				"to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), to_timestamp({}, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"'), "
				"{},                 {},   false,           NULL) returning mediaItemKey",
				workspace->_workspaceKey, trans.transaction->quote(title), ingester == "" ? "null" : trans.transaction->quote(ingester),
				userData == "" ? "null" : trans.transaction->quote(userData),
				deliveryFileName == "" ? "null" : trans.transaction->quote(deliveryFileName), ingestionJobKey,
				trans.transaction->quote(toString(contentType)), trans.transaction->quote(startPublishing), trans.transaction->quote(endPublishing),
				retentionInMinutes, tags
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			mediaItemKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		{
			string uniqueName;
			if (JSONUtils::isPresent(parametersRoot, "uniqueName"))
				uniqueName = JSONUtils::asString(parametersRoot, "uniqueName", "");

			if (uniqueName != "")
			{
				bool allowUniqueNameOverride = false;
				allowUniqueNameOverride = JSONUtils::asBool(parametersRoot, "allowUniqueNameOverride", false);

				manageExternalUniqueName(trans, workspace->_workspaceKey, mediaItemKey, allowUniqueNameOverride, uniqueName);
			}
		}

		// cross references
		{
			string field = "crossReferences";
			if (JSONUtils::isPresent(parametersRoot, field))
			{
				json crossReferencesRoot = parametersRoot[field];

				manageCrossReferences(trans, ingestionJobKey, workspace->_workspaceKey, mediaItemKey, crossReferencesRoot);
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
			if (JSONUtils::isPresent(parametersRoot, field))
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
				if (JSONUtils::isPresent(parametersRoot, field))
				{
					json userDataRoot = parametersRoot[field];

					field = "mmsData";
					if (JSONUtils::isPresent(userDataRoot, field))
					{
						json mmsDataRoot = userDataRoot[field];

						field = "ingestionJobKey";
						if (JSONUtils::isPresent(mmsDataRoot, "liveRecordingChunk"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveRecordingChunk"], field, -1);
						else if (JSONUtils::isPresent(mmsDataRoot, "generatedFrame"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["generatedFrame"], field, -1);
						else if (JSONUtils::isPresent(mmsDataRoot, "externalTranscoder"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["externalTranscoder"], field, -1);
						else if (JSONUtils::isPresent(mmsDataRoot, "liveCut"))
							sourceIngestionJobKey = JSONUtils::asInt64(mmsDataRoot["liveCut"], field, -1);
					}
				}
			}

			physicalPathKey = saveVariantContentMetadata(
				trans,

				workspace->_workspaceKey, ingestionJobKey, sourceIngestionJobKey, mediaItemKey, externalReadOnlyStorage, externalDeliveryTechnology,
				externalDeliveryURL, mediaSourceFileName, relativePath, mmsPartitionIndexUsed, sizeInBytes, encodingProfileKey,
				physicalItemRetentionInMinutes,

				// video-audio
				mediaInfoDetails, videoTracks, audioTracks,

				// image
				imageWidth, imageHeight, imageFormat, imageQuality
			);
		}

		{
			int currentDirLevel1;
			int currentDirLevel2;
			int currentDirLevel3;

			{
				string sqlStatement = std::format(
					"select currentDirLevel1, currentDirLevel2, currentDirLevel3 "
					"from MMS_WorkspaceMoreInfo where workspaceKey = {}",
					workspace->_workspaceKey
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
					currentDirLevel1 = res[0]["currentDirLevel1"].as<int>();
					currentDirLevel2 = res[0]["currentDirLevel2"].as<int>();
					currentDirLevel3 = res[0]["currentDirLevel3"].as<int>();
				}
				else
				{
					string errorMessage = std::format(
						"Workspace is not present/configured"
						", workspace->_workspaceKey: {}"
						", sqlStatement: {}",
						workspace->_workspaceKey, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}
			}

			if (currentDirLevel3 >= 999)
			{
				currentDirLevel3 = 0;

				if (currentDirLevel2 >= 999)
				{
					currentDirLevel2 = 0;

					if (currentDirLevel1 >= 999)
					{
						currentDirLevel1 = 0;
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
				string sqlStatement = std::format(
					"update MMS_WorkspaceMoreInfo set currentDirLevel1 = {}, currentDirLevel2 = {}, "
					"currentDirLevel3 = {}, currentIngestionsNumber = currentIngestionsNumber + 1 "
					"where workspaceKey = {} ",
					currentDirLevel1, currentDirLevel2, currentDirLevel3, workspace->_workspaceKey
				);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				result res = trans.transaction->exec(sqlStatement);
				int rowsUpdated = res.affected_rows();
				long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
				SQLQUERYLOG(
					"default", elapsed,
					"SQL statement"
					", sqlStatement: @{}@"
					", getConnectionId: @{}@"
					", elapsed (millisecs): @{}@",
					sqlStatement, trans.connection->getConnectionId(), elapsed
				);
				if (rowsUpdated != 1)
				{
					string errorMessage = std::format(
						"no update was done"
						", currentDirLevel1: {}"
						", currentDirLevel2: {}"
						", currentDirLevel3: {}"
						", workspace->_workspaceKey: {}"
						", rowsUpdated: {}"
						", sqlStatement: {}",
						currentDirLevel1, currentDirLevel2, currentDirLevel3, workspace->_workspaceKey, rowsUpdated, sqlStatement
					);
					SPDLOG_ERROR(errorMessage);

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
				SPDLOG_INFO(
					"Update IngestionJob"
					", ingestionJobKey: {}"
					", IngestionStatus: {}"
					", errorMessage: {}"
					", processorMMS: {}",
					ingestionJobKey, toString(newIngestionStatus), errorMessage, processorMMS
				);
				updateIngestionJob(trans, ingestionJobKey, newIngestionStatus, errorMessage);
			}
		}

		mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
		mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
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
		case 's': // seconds
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) / 60;

			break;
		case 'm': // minutes
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1));

			break;
		case 'h': // hours
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 60;

			break;
		case 'd': // days
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * 1440;

			break;
		case 'M': // month
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 30);

			break;
		case 'y': // year
			retentionInMinutes = stoll(localRetention.substr(0, localRetention.length() - 1)) * (1440 * 365);

			break;
		}
	}

	return retentionInMinutes;
}

void MMSEngineDBFacade::manageExternalUniqueName(
	PostgresConnTrans &trans, int64_t workspaceKey, int64_t mediaItemKey,

	bool allowUniqueNameOverride, string uniqueName
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
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
			*/

			// delete it if present
			{
				string sqlStatement = std::format(
					"delete from MMS_ExternalUniqueName "
					"where workspaceKey = {} and mediaItemKey = {} ",
					workspaceKey, mediaItemKey
				);
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

			return;
		}

		// look if it is an insert (we do NOT have one) or an update (we already have one)
		string currentUniqueName;
		{
			string sqlStatement = std::format(
				"select uniqueName from MMS_ExternalUniqueName "
				"where workspaceKey = {} and mediaItemKey = {}",
				workspaceKey, mediaItemKey
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
				currentUniqueName = res[0]["uniqueName"].as<string>();
		}

		if (currentUniqueName == "")
		{
			// insert

			if (allowUniqueNameOverride)
			{
				string sqlStatement = std::format(
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = {} and uniqueName = {}",
					workspaceKey, trans.transaction->quote(uniqueName)
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
					int64_t mediaItemKeyOfCurrentUniqueName = res[0]["mediaItemKey"].as<int64_t>();

					{
						string sqlStatement = std::format(
							"update MMS_ExternalUniqueName "
							"set uniqueName = uniqueName || '-' || '{}' || '-' || '{}' "
							"where workspaceKey = {} and uniqueName = {} ",
							mediaItemKey, chrono::system_clock::now().time_since_epoch().count(), workspaceKey, trans.transaction->quote(uniqueName)
						);
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

					{
						string sqlStatement = std::format(
							"update MMS_MediaItem "
							"set markedAsRemoved = true "
							"where workspaceKey = {} and mediaItemKey = {} ",
							workspaceKey, mediaItemKeyOfCurrentUniqueName
						);
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

			{
				string sqlStatement = std::format(
					"insert into MMS_ExternalUniqueName (workspaceKey, mediaItemKey, uniqueName) "
					"values ({}, {}, {})",
					workspaceKey, mediaItemKey, trans.transaction->quote(uniqueName)
				);
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
		else
		{
			// update

			if (allowUniqueNameOverride)
			{
				string sqlStatement = std::format(
					"select mediaItemKey from MMS_ExternalUniqueName "
					"where workspaceKey = {} and uniqueName = {}",
					workspaceKey, trans.transaction->quote(uniqueName)
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
					int64_t mediaItemKeyOfCurrentUniqueName = res[0]["mediaItemKey"].as<int64_t>();

					if (mediaItemKeyOfCurrentUniqueName != mediaItemKey)
					{
						{
							string sqlStatement = std::format(
								"update MMS_ExternalUniqueName "
								"set uniqueName = uniqueName || '-' || '{}' || '-' || '{}' "
								"where workspaceKey = {} and uniqueName = {} ",
								mediaItemKey, chrono::system_clock::now().time_since_epoch().count(), workspaceKey,
								trans.transaction->quote(uniqueName)
							);
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

						{
							string sqlStatement = std::format(
								"update MMS_MediaItem "
								"set markedAsRemoved = true "
								"where workspaceKey = {} and mediaItemKey = {} ",
								workspaceKey, mediaItemKeyOfCurrentUniqueName
							);
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
			}

			{
				string sqlStatement = std::format(
					"update MMS_ExternalUniqueName "
					"set uniqueName = {} "
					"where workspaceKey = {} and mediaItemKey = {} ",
					trans.transaction->quote(uniqueName), workspaceKey, mediaItemKey
				);
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

		throw;
	}
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
	int64_t workspaceKey, int64_t ingestionJobKey, int64_t sourceIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
	string externalDeliveryTechnology, string externalDeliveryURL, string encodedFileName, string relativePath, int mmsPartitionIndexUsed,
	unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

	// video-audio
	tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

	// image
	int imageWidth, int imageHeight, string imageFormat, int imageQuality
)
{
	int64_t physicalPathKey;
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
		physicalPathKey = saveVariantContentMetadata(
			trans,

			workspaceKey, ingestionJobKey, sourceIngestionJobKey, mediaItemKey, externalReadOnlyStorage, externalDeliveryTechnology,
			externalDeliveryURL, encodedFileName, relativePath, mmsPartitionIndexUsed, sizeInBytes, encodingProfileKey,
			physicalItemRetentionPeriodInMinutes,

			// video-audio
			mediaInfoDetails, videoTracks, audioTracks,

			// image
			imageWidth, imageHeight, imageFormat, imageQuality
		);
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

	return physicalPathKey;
}

int64_t MMSEngineDBFacade::saveVariantContentMetadata(
	PostgresConnTrans &trans,

	int64_t workspaceKey, int64_t ingestionJobKey, int64_t sourceIngestionJobKey, int64_t mediaItemKey, bool externalReadOnlyStorage,
	string externalDeliveryTechnology, string externalDeliveryURL, string encodedFileName, string relativePath, int mmsPartitionIndexUsed,
	unsigned long long sizeInBytes, int64_t encodingProfileKey, int64_t physicalItemRetentionPeriodInMinutes,

	// video-audio
	tuple<int64_t, long, json> &mediaInfoDetails, vector<tuple<int, int64_t, string, string, int, int, string, long>> &videoTracks,
	vector<tuple<int, int64_t, string, long, int, long, string>> &audioTracks,

	// image
	int imageWidth, int imageHeight, string imageFormat, int imageQuality
)
{
	int64_t physicalPathKey;

	try
	{
		MMSEngineDBFacade::ContentType contentType;
		{
			string sqlStatement = std::format("select contentType from MMS_MediaItem where mediaItemKey = {}", mediaItemKey);
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
				contentType = MMSEngineDBFacade::toContentType(res[0]["contentType"].as<string>());
			else
			{
				string errorMessage = std::format(
					"no ContentType returned"
					", mediaItemKey: {}",
					mediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		string deliveryInfo;
		{
			if (externalDeliveryTechnology != "" || externalDeliveryURL != "")
			{
				json deliveryInfoRoot;

				string field = "externalDeliveryTechnology";
				deliveryInfoRoot[field] = externalDeliveryTechnology;

				field = "externalDeliveryURL";
				deliveryInfoRoot[field] = externalDeliveryURL;

				deliveryInfo = JSONUtils::toString(deliveryInfoRoot);
			}
		}

		int64_t durationInMilliSeconds;
		long bitRate;
		json metaDataRoot;
		string metaData;

		tie(durationInMilliSeconds, bitRate, metaDataRoot) = mediaInfoDetails;
		if (metaDataRoot != nullptr)
			metaData = JSONUtils::toString(metaDataRoot);

		{
			int drm = 0;

			SPDLOG_INFO(
				"insert into MMS_PhysicalPath"
				", mediaItemKey: {}"
				", relativePath: {}"
				", encodedFileName: {}"
				", encodingProfileKey: {}"
				", deliveryInfo: {}"
				", physicalItemRetentionPeriodInMinutes: {}",
				mediaItemKey, relativePath, encodedFileName, encodingProfileKey, deliveryInfo, physicalItemRetentionPeriodInMinutes
			);
			string sqlStatement = std::format(
				"insert into MMS_PhysicalPath(physicalPathKey, mediaItemKey, drm, externalReadOnlyStorage, "
				"fileName, relativePath, partitionNumber, sizeInBytes, encodingProfileKey, "
				"durationInMilliSeconds, bitRate, deliveryInfo, metaData, creationDate, retentionInMinutes) values ("
				"DEFAULT, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, NOW() at time zone 'utc', {}) returning physicalPathKey",
				mediaItemKey, drm, externalReadOnlyStorage, trans.transaction->quote(encodedFileName), trans.transaction->quote(relativePath),
				mmsPartitionIndexUsed, sizeInBytes, encodingProfileKey == -1 ? "null" : to_string(encodingProfileKey),
				durationInMilliSeconds == -1 ? "null" : to_string(durationInMilliSeconds), bitRate == -1 ? "null" : to_string(bitRate),
				deliveryInfo == "" ? "null" : trans.transaction->quote(deliveryInfo), metaData == "" ? "null" : trans.transaction->quote(metaData),
				physicalItemRetentionPeriodInMinutes == -1 ? "null" : to_string(physicalItemRetentionPeriodInMinutes)
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			physicalPathKey = trans.transaction->exec1(sqlStatement)[0].as<int64_t>();
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

		if (contentType == ContentType::Video || contentType == ContentType::Audio)
		{
			for (tuple<int, int64_t, string, string, int, int, string, long> videoTrack : videoTracks)
			{
				int videoTrackIndex;
				int64_t videoDurationInMilliSeconds;
				string videoCodecName;
				string videoProfile;
				int videoWidth;
				int videoHeight;
				string videoAvgFrameRate;
				long videoBitRate;

				tie(videoTrackIndex, videoDurationInMilliSeconds, videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate,
					videoBitRate) = videoTrack;

				string sqlStatement = std::format(
					"insert into MMS_VideoTrack (videoTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, width, height, avgFrameRate, "
					"codecName, bitRate, profile) values ("
					"DEFAULT, {}, {}, {}, {}, {}, {}, {}, {}, {})",
					physicalPathKey, videoTrackIndex == -1 ? "null" : to_string(videoTrackIndex),
					videoDurationInMilliSeconds == -1 ? "null" : to_string(videoDurationInMilliSeconds),
					videoWidth == -1 ? "null" : to_string(videoWidth), videoHeight == -1 ? "null" : to_string(videoHeight),
					videoAvgFrameRate == "" ? "null" : trans.transaction->quote(videoAvgFrameRate),
					videoCodecName == "" ? "null" : trans.transaction->quote(videoCodecName), videoBitRate == -1 ? "null" : to_string(videoBitRate),
					videoProfile == "" ? "null" : trans.transaction->quote(videoProfile)
				);
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

			for (tuple<int, int64_t, string, long, int, long, string> audioTrack : audioTracks)
			{
				int audioTrackIndex;
				int64_t audioDurationInMilliSeconds;
				string audioCodecName;
				long audioSampleRate;
				int audioChannels;
				long audioBitRate;
				string language;

				tie(audioTrackIndex, audioDurationInMilliSeconds, audioCodecName, audioSampleRate, audioChannels, audioBitRate, language) =
					audioTrack;

				string sqlStatement = std::format(
					"insert into MMS_AudioTrack (audioTrackKey, physicalPathKey, "
					"trackIndex, durationInMilliSeconds, codecName, bitRate, sampleRate, channels, language) values ("
					"DEFAULT, {}, {}, {}, {}, {}, {}, {}, {})",
					physicalPathKey, audioTrackIndex == -1 ? "null" : to_string(audioTrackIndex),
					audioDurationInMilliSeconds == -1 ? "null" : to_string(audioDurationInMilliSeconds),
					audioCodecName == "" ? "null" : trans.transaction->quote(audioCodecName), audioBitRate == -1 ? "null" : to_string(audioBitRate),
					audioSampleRate == -1 ? "null" : to_string(audioSampleRate), audioChannels == -1 ? "null" : to_string(audioChannels),
					language == "" ? "null" : trans.transaction->quote(language)
				);
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
		else if (contentType == ContentType::Image)
		{
			string sqlStatement = std::format(
				"insert into MMS_ImageItemProfile (physicalPathKey, width, height, format, "
				"quality) values ("
				"{}, {}, {}, {}, {})",
				physicalPathKey, imageWidth, imageHeight, trans.transaction->quote(imageFormat), imageQuality
			);
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
			string errorMessage = std::format(
				"ContentType is wrong"
				", contentType: {}",
				toString(contentType)
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		addIngestionJobOutput(trans, ingestionJobKey, mediaItemKey, physicalPathKey, sourceIngestionJobKey);
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

		throw;
	}

	return physicalPathKey;
}

void MMSEngineDBFacade::manageCrossReferences(
	PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t workspaceKey, int64_t mediaItemKey, json crossReferencesRoot
)
{
	try
	{
		// make sure the mediaitemkey belong to the workspace
		{
			string sqlStatement = std::format(
				"select mediaItemKey from MMS_MediaItem "
				"where workspaceKey = {} and mediaItemKey = {}",
				workspaceKey, mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			chrono::milliseconds sqlDuration = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql);
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), sqlDuration.count()
			);
			if (empty(res))
			{
				string errorMessage = std::format(
					"cross references cannot be updated because mediaItemKey does not belong to the workspace"
					", workspaceKey: {}"
					", mediaItemKey: {}",
					workspaceKey, mediaItemKey
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			string sqlStatement = std::format(
				"delete from MMS_CrossReference "
				"where sourceMediaItemKey = {} or targetMediaItemKey = {} ",
				mediaItemKey, mediaItemKey
			);
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

		for (int crossReferenceIndex = 0; crossReferenceIndex < crossReferencesRoot.size(); crossReferenceIndex++)
		{
			json crossReferenceRoot = crossReferencesRoot[crossReferenceIndex];

			string field = "type";
			CrossReferenceType crossReferenceType = toCrossReferenceType(JSONUtils::asString(crossReferenceRoot, field, ""));

			int64_t sourceMediaItemKey;
			int64_t targetMediaItemKey;

			if (crossReferenceType == CrossReferenceType::VideoOfImage)
			{
				crossReferenceType = CrossReferenceType::ImageOfVideo;

				targetMediaItemKey = mediaItemKey;

				field = "mediaItemKey";
				sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
			}
			else if (crossReferenceType == CrossReferenceType::VideoOfPoster)
			{
				crossReferenceType = CrossReferenceType::PosterOfVideo;

				targetMediaItemKey = mediaItemKey;

				field = "mediaItemKey";
				sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
			}
			else if (crossReferenceType == CrossReferenceType::VideoOfFace)
			{
				crossReferenceType = CrossReferenceType::FaceOfVideo;

				targetMediaItemKey = mediaItemKey;

				field = "mediaItemKey";
				sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
			}
			else if (crossReferenceType == CrossReferenceType::ImageForSlideShow)
			{
				crossReferenceType = CrossReferenceType::SlideShowOfImage;

				targetMediaItemKey = mediaItemKey;

				field = "mediaItemKey";
				sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
			}
			else if (crossReferenceType == CrossReferenceType::AudioForSlideShow)
			{
				crossReferenceType = CrossReferenceType::SlideShowOfAudio;

				targetMediaItemKey = mediaItemKey;

				field = "mediaItemKey";
				sourceMediaItemKey = JSONUtils::asInt64(crossReferenceRoot, field, 0);
			}
			else if (crossReferenceType == CrossReferenceType::AudioOfImage)
			{
				crossReferenceType = CrossReferenceType::ImageOfAudio;

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

			json crossReferenceParametersRoot;
			field = "parameters";
			if (JSONUtils::isPresent(crossReferenceRoot, field))
				crossReferenceParametersRoot = crossReferenceRoot[field];

			addCrossReference(trans, ingestionJobKey, sourceMediaItemKey, crossReferenceType, targetMediaItemKey, crossReferenceParametersRoot);
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

		throw;
	}
}

void MMSEngineDBFacade::addCrossReference(
	int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	json crossReferenceParametersRoot
)
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
		addCrossReference(trans, ingestionJobKey, sourceMediaItemKey, crossReferenceType, targetMediaItemKey, crossReferenceParametersRoot);
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

void MMSEngineDBFacade::addCrossReference(
	PostgresConnTrans &trans, int64_t ingestionJobKey, int64_t sourceMediaItemKey, CrossReferenceType crossReferenceType, int64_t targetMediaItemKey,
	json crossReferenceParametersRoot
)
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
				sqlStatement = std::format(
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values ({}, {}, {}, {})",
					sourceMediaItemKey, trans.transaction->quote(toString(crossReferenceType)), targetMediaItemKey,
					trans.transaction->quote(crossReferenceParameters)
				);
			else
				sqlStatement = std::format(
					"insert into MMS_CrossReference (sourceMediaItemKey, type, targetMediaItemKey, parameters) "
					"values ({}, {}, {}, NULL)",
					sourceMediaItemKey, trans.transaction->quote(toString(crossReferenceType)), targetMediaItemKey
				);
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

		throw;
	}
}

void MMSEngineDBFacade::removePhysicalPath(int64_t physicalPathKey)
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
		{
			string sqlStatement = std::format(
				"delete from MMS_PhysicalPath "
				"where physicalPathKey = {} ",
				physicalPathKey
			);
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
			/*
			if (rowsUpdated != 1)
			{
				// probable because encodingPercentage was already the same in the table
				string errorMessage = __FILEREF__ + "no delete was done" + ", physicalPathKey: " + to_string(physicalPathKey) +
									  ", rowsUpdated: " + to_string(rowsUpdated) + ", sqlStatement: " + sqlStatement;
				SPDLOG_WARN(errorMessage);

				// throw runtime_error(errorMessage);
			}
			*/
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

void MMSEngineDBFacade::removeMediaItem(int64_t mediaItemKey)
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
		{
			string sqlStatement = std::format(
				"delete from MMS_MediaItem "
				"where mediaItemKey = {} ",
				mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			int rowsUpdated = res.affected_rows();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = std::format(
					"no delete was done"
					", mediaItemKey: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					mediaItemKey, rowsUpdated, sqlStatement
				);
				SPDLOG_WARN(errorMessage);

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
}

json MMSEngineDBFacade::getTagsList(
	int64_t workspaceKey, int start, int rows, int liveRecordingChunk, optional<ContentType> contentType, string tagNameFilter,
	bool fromMaster
)
{
	/*
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
	*/

	PostgresConnTrans trans(fromMaster ? _masterPostgresConnectionPool : _slavePostgresConnectionPool, false);
	json tagsListRoot;
	try
	{
		string field;

		SPDLOG_INFO(
			"getTagsList"
			", workspaceKey: {}"
			", start: {}"
			", rows: {}"
			", liveRecordingChunk: {}"
			", contentType: {}"
			", tagNameFilter: {}",
			workspaceKey, start, rows, liveRecordingChunk, (contentType ? toString(*contentType) : ""), tagNameFilter
		);

		{
			json requestParametersRoot;

			field = "start";
			requestParametersRoot[field] = start;

			field = "rows";
			requestParametersRoot[field] = rows;

			field = "liveRecordingChunk";
			requestParametersRoot[field] = liveRecordingChunk;

			if (contentType)
			{
				field = "contentType";
				requestParametersRoot[field] = toString(*contentType);
			}

			if (tagNameFilter != "")
			{
				field = "tagNameFilter";
				requestParametersRoot[field] = tagNameFilter;
			}

			field = "requestParameters";
			tagsListRoot[field] = requestParametersRoot;
		}

		string tagNameFilterLowerCase;
		if (tagNameFilter != "")
		{
			tagNameFilterLowerCase.resize(tagNameFilter.size());
			transform(tagNameFilter.begin(), tagNameFilter.end(), tagNameFilterLowerCase.begin(), [](unsigned char c) { return tolower(c); });
		}

		string sqlWhere;
		sqlWhere = std::format("where workspaceKey = {} ", workspaceKey);
		if (contentType)
			sqlWhere += std::format("and contentType = {} ", trans.transaction->quote(toString(*contentType)));
		if (liveRecordingChunk == 0)
			sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is NULL ");
		else if (liveRecordingChunk == 1)
			sqlWhere += ("and userData -> 'mmsData' ->> 'liveRecordingChunk' is not NULL ");

		json responseRoot;
		{
			string sqlStatement = std::format(
				"select count(distinct tagName) from ("
				"select unnest(tags) as tagName from MMS_MediaItem {}) t ",
				sqlWhere
			);
			if (tagNameFilter != "")
				sqlStatement += std::format("where lower(tagName) like {} ", trans.transaction->quote("%" + tagNameFilterLowerCase + "%"));
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			field = "numFound";
			responseRoot[field] = trans.transaction->exec1(sqlStatement)[0].as<int>();
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

		json tagsRoot = json::array();
		{
			string sqlStatement = std::format(
				"select distinct tagName from ("
				"select unnest(tags) as tagName from MMS_MediaItem {}) t ",
				sqlWhere
			);
			if (tagNameFilter != "")
				sqlStatement += std::format("where lower(tagName) like {} ", trans.transaction->quote("%" + tagNameFilterLowerCase + "%"));
			sqlStatement += std::format("order by tagName limit {} offset {}", rows, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			for (auto row : res)
				tagsRoot.push_back(static_cast<string>(row["tagName"].as<string>()));
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

		field = "tags";
		responseRoot[field] = tagsRoot;

		field = "response";
		tagsListRoot[field] = responseRoot;
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

	return tagsListRoot;
}

void MMSEngineDBFacade::updateMediaItem(int64_t mediaItemKey, string processorMMSForRetention)
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
	SPDLOG_INFO(
		"updateMediaItem"
		", mediaItemKey: {}"
		", processorMMSForRetention: {}",
		mediaItemKey, processorMMSForRetention
	);
	try
	{
		{
			string sqlStatement = std::format(
				"update MMS_MediaItem set processorMMSForRetention = {} "
				"where mediaItemKey = {} ",
				processorMMSForRetention == "" ? "null" : trans.transaction->quote(processorMMSForRetention), mediaItemKey
			);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result res = trans.transaction->exec(sqlStatement);
			int rowsUpdated = res.affected_rows();
			long elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count();
			SQLQUERYLOG(
				"default", elapsed,
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed (millisecs): @{}@",
				sqlStatement, trans.connection->getConnectionId(), elapsed
			);
			if (rowsUpdated != 1)
			{
				string errorMessage = std::format(
					"no update was done"
					", mediaItemKey: {}"
					", processorMMSForRetention: {}"
					", rowsUpdated: {}"
					", sqlStatement: {}",
					mediaItemKey, processorMMSForRetention, rowsUpdated, sqlStatement
				);
				SPDLOG_ERROR(errorMessage);

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
}
