
#include "PostgresHelper.h"
#include "catralibraries/StringUtils.h"
#include "spdlog/spdlog.h"
#include <cstdint>
#include <string>
#include <utility>

PostgresHelper::PostgresHelper() {}
PostgresHelper::~PostgresHelper() = default;

// requestedTableAndColumnNames: { "<tbl name>.<table name alias>.<col name>", ..., "<tbl name 2>.<table name alias>.*" }
// <table name alias> puo essere una stringa vuota, in tal caso avremo "<tbl name>..<col name>"
string PostgresHelper::buildQueryColumns(vector<pair<bool, string>> &requestedColumns)
{
	string queryColumns;

	if (requestedColumns.size() == 0)
	{
		string errorMessage = "no requestedColumns found";
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	for (pair<bool, string> requestedColumn : requestedColumns)
	{
		auto [custom, column] = requestedColumn;

		if (custom)
		{
			if (!queryColumns.empty())
				queryColumns += ", ";
			queryColumns += column;
		}
		else
		{
			string requestedTableName;
			string requestedTableNameAlias;
			string requestedColumnName;
			{
				string requestedTableNameAndAlias;

				stringstream s1(column);
				getline(s1, requestedTableNameAndAlias, '.');
				getline(s1, requestedColumnName, '.');

				stringstream s2(requestedTableNameAndAlias);
				getline(s2, requestedTableName, ':');
				getline(s2, requestedTableNameAlias, ':');

				requestedTableName = StringUtils::lowerCase(requestedTableName);
				requestedColumnName = StringUtils::lowerCase(requestedColumnName);

				// SPDLOG_INFO(
				// 	"ColumnInfo"
				// 	", column: {}"
				// 	", requestedTableNameAndAlias: {}"
				// 	", requestedTableName: {}"
				// 	", requestedTableNameAlias: {}"
				// 	", requestedColumnName: {}",
				// 	column, requestedTableNameAndAlias, requestedTableName, requestedTableNameAlias, requestedColumnName
				// );
			}

			auto itTable = _sqlTablesColumnsSchema.find(requestedTableName);
			if (itTable == _sqlTablesColumnsSchema.end())
			{
				string errorMessage = fmt::format(
					"requested table name not found"
					", requestedTableName: {}",
					requestedTableName
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			if (requestedColumnName == "*")
			{
				for (pair<string, shared_ptr<SqlColumnSchema>> sqlColumnSchema : itTable->second)
				{
					string columnName = sqlColumnSchema.first;

					if (!queryColumns.empty())
						queryColumns += ", ";

					queryColumns += getQueryColumn(sqlColumnSchema.second, requestedTableNameAlias);
				}
			}
			else
			{
				size_t endOfColumn = requestedColumnName.find("[");
				auto itColumn =
					(endOfColumn == string::npos ? itTable->second.find(requestedColumnName)
												 : itTable->second.find(requestedColumnName.substr(0, endOfColumn)));
				if (itColumn == itTable->second.end())
				{
					string errorMessage = fmt::format(
						"requested column name not found"
						", requestedTableName: {}"
						", requestedColumnName: {}",
						requestedTableName, requestedColumnName
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				if (!queryColumns.empty())
					queryColumns += ", ";

				queryColumns += getQueryColumn(itColumn->second, requestedTableNameAlias, requestedColumnName);
			}
		}
	}

	return queryColumns;
}

shared_ptr<PostgresHelper::SqlResultSet> PostgresHelper::buildResult(result result)
{
	shared_ptr<PostgresHelper::SqlResultSet> sqlResultSet = make_shared<PostgresHelper::SqlResultSet>();

	sqlResultSet->clearData();
	int rowIndex = 0;
	for (auto row : result)
	{
		int columnIndex = 0;
		for (auto field : row)
		{
			string fieldName = field.name();
			PostgresHelper::SqlValue sqlValue;
			PostgresHelper::SqlResultSet::SqlValueType sqlValueType = PostgresHelper::SqlResultSet::unknown;
			{
				if (field.is_null())
					// sqlValue.setNull();
					sqlValue.setValue(make_shared<Base>());
				else
				{
					switch (field.type())
					{
					case 16: // bool
						sqlValue.setValue(make_shared<SqlType<bool>>(field.as<bool>()));
						sqlValueType = PostgresHelper::SqlResultSet::boolean;
						break;
						/*
					case 18: // char: field.as<char>() sembra non esistere
						sqlValue.setValue(make_shared<SqlType<char>>(field.as<char>()));
						sqlValueType = PostgresHelper::SqlResultSet::char_;
						break;
						*/
					case 20: // int8
						sqlValue.setValue(make_shared<SqlType<int64_t>>(field.as<int64_t>()));
						sqlValueType = PostgresHelper::SqlResultSet::int64;
						break;
					case 21: // int2
						sqlValue.setValue(make_shared<SqlType<int16_t>>(field.as<int16_t>()));
						sqlValueType = PostgresHelper::SqlResultSet::int16;
						break;
					case 23: // int4
						sqlValue.setValue(make_shared<SqlType<int32_t>>(field.as<int32_t>()));
						sqlValueType = PostgresHelper::SqlResultSet::int32;
						break;
					case 25: // text
						sqlValue.setValue(make_shared<SqlType<string>>(field.as<string>()));
						sqlValueType = PostgresHelper::SqlResultSet::text;
						break;
					case 1000: // array of bool
					{
						vector<bool> v;

						auto array = field.as_array();
						pair<array_parser::juncture, string> elem;
						do
						{
							elem = array.get_next();
							if (elem.first == array_parser::juncture::string_value)
								v.push_back(elem.second == "t");
						} while (elem.first != array_parser::juncture::done);

						sqlValue.setValue(make_shared<SqlType<vector<bool>>>(v));
						sqlValueType = PostgresHelper::SqlResultSet::vectorBoolean;
					}
					break;
					case 1007: // array of int32
					{
						vector<int32_t> v;

						auto array = field.as_array();
						pair<array_parser::juncture, string> elem;
						do
						{
							elem = array.get_next();
							if (elem.first == array_parser::juncture::string_value)
								v.push_back(stol(elem.second));
						} while (elem.first != array_parser::juncture::done);

						sqlValue.setValue(make_shared<SqlType<vector<int32_t>>>(v));
						sqlValueType = PostgresHelper::SqlResultSet::vectorInt32;
					}
					break;
					case 1700: // numeric
						sqlValue.setValue(make_shared<SqlType<double>>(field.as<double>()));
						sqlValueType = PostgresHelper::SqlResultSet::double_;
						break;
					case 3802: // jsonb
						sqlValue.setValue(make_shared<SqlType<json>>(JSONUtils::toJson(field.as<string>())));
						sqlValueType = PostgresHelper::SqlResultSet::json_;
						break;
					default:
					{
						// per avere il mapping tra oid e type: select oid, typname from pg_catalog.pg_type
						string errorMessage = fmt::format(
							"oid / sql data type not managed"
							", oid: {}"
							", fieldName: {}",
							field.type(), field.name()
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					}
				}
			}

			if (rowIndex == 0)
				sqlResultSet->addColumnType(fieldName, sqlValueType);

			sqlResultSet->addColumnValueToCurrentRow(fieldName, sqlValue);
			columnIndex++;
		}

		sqlResultSet->addCurrentRow();
		rowIndex++;
	}

	return sqlResultSet;
}

string PostgresHelper::getQueryColumn(
	shared_ptr<SqlColumnSchema> sqlColumnSchema, string requestedTableNameAlias,
	string requestedColumnName // serve solamente se identifica un elemento di un array
)
{
	string queryColumn;

	string columnName = getColumnName(sqlColumnSchema, requestedTableNameAlias);

	if (sqlColumnSchema->dataType == "\"char\"")
	{
		// devo fare il cast a int perchè in buildResult field.as<char>() sembra non esistere
		if (requestedTableNameAlias.empty())
			queryColumn = fmt::format("CAST({} as integer) as {}", sqlColumnSchema->columnName, columnName);
		else
			queryColumn = fmt::format("CAST({}.{} as integer) as {}", requestedTableNameAlias, sqlColumnSchema->columnName, columnName);
	}
	else if (sqlColumnSchema->dataType == "integer" || sqlColumnSchema->dataType == "smallint" || sqlColumnSchema->dataType == "bigint" ||
			 sqlColumnSchema->dataType == "numeric" || sqlColumnSchema->dataType == "boolean" || sqlColumnSchema->dataType == "json" ||
			 sqlColumnSchema->dataType == "jsonb" || sqlColumnSchema->dataType == "text")
	{
		if (requestedTableNameAlias.empty())
			queryColumn = sqlColumnSchema->columnName;
		// queryColumn = fmt::format("{} as {}", sqlColumnSchema->columnName, columnName); commentato perchè verrebbe "name as name"
		else
			queryColumn = fmt::format("{}.{} as {}", requestedTableNameAlias, sqlColumnSchema->columnName, columnName);
	}
	else if (StringUtils::startWith(sqlColumnSchema->dataType, "timestamp"))
	{
		// EPOCH ritorna un float (seconds.milliseconds)
		if (requestedTableNameAlias.empty())
			queryColumn = fmt::format(
				"EXTRACT(EPOCH FROM {0} AT TIME ZONE 'UTC') * 1000 as {1}, "
				"to_char({0}, 'YYYY-MM-DD\"T\"HH24:MI:SS.MSZ') as \"{1}:iso\"",
				sqlColumnSchema->columnName, columnName
			);
		else
			queryColumn = fmt::format(
				"EXTRACT(EPOCH FROM {0}.{1} AT TIME ZONE 'UTC') * 1000 as {2}, to_char({0}.{1}, 'YYYY-MM-DD\"T\"HH24:MI:SS.MSZ') as \"{2}:iso\"",
				requestedTableNameAlias, sqlColumnSchema->columnName, columnName
			);
	}
	else if (sqlColumnSchema->dataType == "ARRAY")
	{
		size_t endOfColumn = requestedColumnName.find("[");
		if (endOfColumn == string::npos)
		{
			if (requestedTableNameAlias.empty())
				queryColumn = sqlColumnSchema->columnName;
			// queryColumn = fmt::format("{} as {}", sqlColumnSchema->columnName, columnName); commentato perchè verrebbe "name as name"
			else
				queryColumn = fmt::format("{}.{} as {}", requestedTableNameAlias, sqlColumnSchema->columnName, columnName);
		}
		else
		{
			if (requestedTableNameAlias.empty())
				queryColumn = fmt::format("{} as {}", requestedColumnName, columnName);
			else
				queryColumn = fmt::format("{}.{} as {}", requestedTableNameAlias, requestedColumnName, columnName);
		}
	}
	else
	{
		string errorMessage = fmt::format(
			"sql data type not managed"
			", dataType: {}",
			sqlColumnSchema->dataType
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	return queryColumn;
}

string PostgresHelper::getColumnName(shared_ptr<SqlColumnSchema> sqlColumnSchema, string requestedTableNameAlias)
{
	string queryColumnName;

	if (StringUtils::startWith(sqlColumnSchema->dataType, "timestamp") || sqlColumnSchema->dataType == "\"char\"" ||
		sqlColumnSchema->dataType == "integer" || sqlColumnSchema->dataType == "smallint" || sqlColumnSchema->dataType == "bigint" ||
		sqlColumnSchema->dataType == "numeric" || sqlColumnSchema->dataType == "boolean" || sqlColumnSchema->dataType == "text" ||
		sqlColumnSchema->dataType == "jsonb" || sqlColumnSchema->dataType == "ARRAY")
	{
		if (requestedTableNameAlias.empty())
			queryColumnName = sqlColumnSchema->columnName;
		else
			queryColumnName = fmt::format("{0}_{1}", requestedTableNameAlias, sqlColumnSchema->columnName);
	}
	else
	{
		string errorMessage = fmt::format(
			"sql data type not managed"
			", dataType: {}",
			sqlColumnSchema->dataType
		);
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}

	return queryColumnName;
}

bool PostgresHelper::isDataTypeManaged(string dataType, string arrayDataType)
{
	if (dataType == "\"char\"" || dataType == "integer" || dataType == "smallint" || dataType == "bigint" || dataType == "numeric" ||
		StringUtils::startWith(dataType, "timestamp") || dataType == "boolean" || dataType == "text" || dataType == "jsonb")
		return true;
	else if (dataType == "ARRAY")
	{
		if (arrayDataType == "_int4" || arrayDataType == "_text" || arrayDataType == "_bool")
			return true;
		else
			return false;
	}
	else
		return false;
}

json PostgresHelper::SqlResultSet::asJson(string fieldName, SqlValue sqlValue)
{
	json root = json::array();

	if (sqlValue.isNull())
		root = nullptr;
	else
	{
		switch (type(fieldName))
		{
		case PostgresHelper::SqlResultSet::int16:
			root = sqlValue.as<int16_t>(-1);
			break;
		case PostgresHelper::SqlResultSet::int32:
			root = sqlValue.as<int32_t>(-1);
			break;
		case PostgresHelper::SqlResultSet::int64:
			root = sqlValue.as<int64_t>(-1);
			break;
		case PostgresHelper::SqlResultSet::double_:
			root = sqlValue.as<double>(-1.0);
			break;
		case PostgresHelper::SqlResultSet::text:
			root = sqlValue.as<string>("");
			break;
		case PostgresHelper::SqlResultSet::boolean:
			root = sqlValue.as<bool>(false);
			break;
		case PostgresHelper::SqlResultSet::json_:
			root = sqlValue.as<json>(nullptr);
			break;
		case PostgresHelper::SqlResultSet::vectorInt32:
			for (int32_t value : sqlValue.as<vector<int32_t>>(vector<int32_t>()))
				root.push_back(value);
			break;
		case PostgresHelper::SqlResultSet::vectorInt64:
			for (int64_t value : sqlValue.as<vector<int64_t>>(vector<int64_t>()))
				root.push_back(value);
			break;
		case PostgresHelper::SqlResultSet::vectorDouble:
			for (double value : sqlValue.as<vector<double>>(vector<double>()))
				root.push_back(value);
			break;
		case PostgresHelper::SqlResultSet::vectorText:
			for (string value : sqlValue.as<vector<string>>(vector<string>()))
				root.push_back(value);
			break;
		case PostgresHelper::SqlResultSet::vectorBoolean:
			for (bool value : sqlValue.as<vector<bool>>(vector<bool>()))
				root.push_back(value);
			break;
		case PostgresHelper::SqlResultSet::unknown:
		default:
			root = "unknown";
			break;
		}
	}
	return root;
}

json PostgresHelper::SqlResultSet::asJson()
{
	json jsonRoot = json::array();

	for (auto row : _sqlValuesByIndex)
	{
		json rowRoot;

		// for (auto sqlValue : row)
		for (int columnIndex = 0, columnNumber = row.size(); columnIndex < columnNumber; columnIndex++)
		{
			string fieldName = _sqlColumnTypeByIndex[columnIndex].first;
			SqlValue sqlValue = row[columnIndex];

			string jsonKey = fieldName; // fmt::format("{} ({})", fieldName, (int)type(fieldName));
			if (sqlValue.isNull())
				rowRoot[jsonKey] = nullptr;
			else
				rowRoot[jsonKey] = SqlResultSet::asJson(fieldName, sqlValue);
		}
		jsonRoot.push_back(rowRoot);
	}
	return jsonRoot;
}

PostgresHelper::SqlResultSet::SqlValueType PostgresHelper::SqlResultSet::type(string fieldName)
{
	auto it = _sqlColumnTypeByName.find(fieldName);
	if (it == _sqlColumnTypeByName.end())
		return unknown;
	else
		return it->second;
}

void PostgresHelper::loadSqlColumnsSchema(shared_ptr<PostgresConnection> conn, transaction_base *trans)
{
	// uso il "modello" della doc. di libpqxx dove il costruttore della transazione è fuori del try/catch
	// Se questo non dovesse essere vero, unborrow non sarà chiamata
	// In alternativa, dovrei avere un try/catch per il borrow/transazione che sarebbe eccessivo
	try
	{
		_sqlTablesColumnsSchema.clear();

		{
			string sqlStatement = "select table_name, column_name, is_nullable, data_type, udt_name "
								  "from information_schema.columns where table_schema = 'public' "
								  "order by table_name, column_name ";
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			result result = trans->exec(sqlStatement);
			SPDLOG_DEBUG(
				"SQL statement"
				", sqlStatement: @{}@"
				", getConnectionId: @{}@"
				", elapsed: @{}@",
				sqlStatement, conn->getConnectionId(), chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - startSql).count()
			);

			for (auto row : result)
			{
				if (row["table_name"].is_null() || row["column_name"].is_null() || row["is_nullable"].is_null() || row["data_type"].is_null() ||
					row["udt_name"].is_null())
				{
					SPDLOG_ERROR(
						"schema null column!!!"
						", table_name: {}"
						", column_name: {}"
						", is_nullable: {}"
						", data_type: {}"
						", udt_name: {}",
						row["table_name"].is_null(), row["column_name"].is_null(), row["is_nullable"].is_null(), row["data_type"].is_null(),
						row["udt_name"].is_null()
					);
					continue;
				}

				string tableName = row["table_name"].as<string>();
				string columnName = row["column_name"].as<string>();
				string isNullable = row["is_nullable"].as<string>();
				string dataType = row["data_type"].as<string>();
				string arrayDataType = row["udt_name"].as<string>();

				if (!isDataTypeManaged(dataType, arrayDataType))
				{
					SPDLOG_ERROR(
						"dataType is not managed by our class"
						", table_name: {}"
						", column_name: {}"
						", data_type: {}"
						", arrayDataType: {}",
						tableName, columnName, dataType, arrayDataType
					);
				}

				SPDLOG_DEBUG(
					"table-column found"
					", table_name: {}"
					", column_name: {}"
					", is_nullable: {}"
					", data_type: {}"
					", arrayDataType: {}",
					tableName, columnName, isNullable, dataType, arrayDataType
				);

				auto it = _sqlTablesColumnsSchema.find(tableName);
				if (it == _sqlTablesColumnsSchema.end())
				{
					map<string, shared_ptr<SqlColumnSchema>> sqlColumnsSchema;
					sqlColumnsSchema.insert(make_pair(
						columnName, make_shared<PostgresHelper::SqlColumnSchema>(tableName, columnName, isNullable == "YES", dataType, arrayDataType)
					));
					_sqlTablesColumnsSchema.insert(make_pair(tableName, sqlColumnsSchema));
				}
				else
					it->second.insert(make_pair(
						columnName, make_shared<PostgresHelper::SqlColumnSchema>(tableName, columnName, isNullable == "YES", dataType, arrayDataType)
					));
			}
		}
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

		throw e;
	}
	catch (runtime_error const &e)
	{
		SPDLOG_ERROR(
			"runtime_error"
			", exceptionMessage: {}"
			", conn: {}",
			e.what(), (conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
	catch (exception const &e)
	{
		SPDLOG_ERROR(
			"exception"
			", conn: {}",
			(conn != nullptr ? conn->getConnectionId() : -1)
		);

		throw e;
	}
}
