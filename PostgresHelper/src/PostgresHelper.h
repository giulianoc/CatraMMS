
#ifndef PostgresHelper_h
#define PostgresHelper_h

#include "spdlog/spdlog.h"

#include <cstddef>
#include <cstdint>
#include <memory>
// #define DBCONNECTIONPOOL_LOG
#define DBCONNECTIONPOOL_STATS_LOG
#include <JSONUtils.h>
#include <catralibraries/PostgresConnection.h>
#include <string>

class PostgresHelper
{
  public:
	struct SqlColumnSchema
	{
		SqlColumnSchema(string tableName, string columnName, bool nullable, string dataType, string arrayDataType)
		{
			this->tableName = tableName;
			this->columnName = columnName;
			this->nullable = nullable;
			this->dataType = dataType;
			this->arrayDataType = arrayDataType;
		}

		string tableName;
		string columnName;
		bool nullable;
		string dataType;
		string arrayDataType;
	};

  public:
	class Base
	{
	  protected:
		bool _isNull;

	  public:
		Base() { _isNull = true; };
		bool isNull() { return _isNull; };
	};

	template <typename T> class SqlType : public Base
	{
		T value;

	  public:
		SqlType(T v)
		{
			value = v;
			_isNull = false;
		};
		T as() { return value; };
	};

	class SqlValue
	{
		shared_ptr<Base> value;

	  public:
		SqlValue() {};

		void setValue(shared_ptr<Base> value) { this->value = value; };

		bool isNull() { return value->isNull(); };

		template <class T> T as(T valueIfNull) { return isNull() ? valueIfNull : ((SqlType<T> *)(value.get()))->as(); };
	};

	class SqlResultSet
	{
	  public:
		enum SqlValueType
		{
			unknown,
			int16,
			int32,
			int64,
			double_,
			text,
			boolean,
			json_,
			vectorInt32,
			vectorInt64,
			vectorDouble,
			vectorText,
			vectorBoolean
		};

	  private:
		// column Name / type
		vector<pair<string, SqlValueType>> _sqlColumnTypeByIndex;
		map<string, SqlValueType> _sqlColumnTypeByName;

		// per ogni riga (vector) abbiamo un vettore che contiene i valori delle colonne by Index
		vector<vector<SqlValue>> _sqlValuesByIndex;

		int32_t _count;
		chrono::milliseconds _countSqlDuration;
		chrono::milliseconds _sqlDuration;

		// temporary vector to fill _sqlValuesByIndex
		vector<SqlValue> _sqlCurrentRowValuesByIndex;

	  public:
		virtual void clearData()
		{
			_sqlColumnTypeByIndex.clear();
			_sqlColumnTypeByName.clear();
			_sqlValuesByIndex.clear();
		};
		virtual void addColumnValueToCurrentRow(string fieldName, SqlValue sqlValue) { _sqlCurrentRowValuesByIndex.push_back(sqlValue); };
		virtual void addCurrentRow()
		{
			_sqlValuesByIndex.push_back(_sqlCurrentRowValuesByIndex);
			_sqlCurrentRowValuesByIndex.clear();
		};
		virtual size_t size() { return _sqlValuesByIndex.size(); };
		virtual bool empty() { return _sqlValuesByIndex.size() == 0; };
		virtual json asJson();
		void addColumnType(string fieldName, SqlValueType sqlValueType)
		{
			auto it = _sqlColumnTypeByName.find(fieldName);
			if (it == _sqlColumnTypeByName.end())
				_sqlColumnTypeByName.insert(make_pair(fieldName, sqlValueType));
			else
				// se il nome della colonna è già presente, aggiungiamo anche l'indice della colonna
				_sqlColumnTypeByName.insert(make_pair(fmt::format("{} - {}", fieldName, _sqlColumnTypeByIndex.size()), sqlValueType));
			_sqlColumnTypeByName.insert(make_pair(fieldName, sqlValueType));

			_sqlColumnTypeByIndex.push_back(make_pair(fieldName, sqlValueType));
		};
		SqlValueType type(string fieldName);
		json asJson(string fieldName, SqlValue sqlValue);
		string getColumnNameByIndex(int columnIndex) { return _sqlColumnTypeByIndex[columnIndex].first; };
		int getColumnIndexByName(string columnName)
		{
			for (int index = 0, size = _sqlColumnTypeByIndex.size(); index < size; index++)
				if (_sqlColumnTypeByIndex[index].first == columnName)
					return index;
			return -1;
		};
		vector<vector<SqlValue>>::iterator begin() { return _sqlValuesByIndex.begin(); };
		vector<vector<SqlValue>>::iterator end() { return _sqlValuesByIndex.end(); };
		vector<vector<SqlValue>>::const_iterator begin() const { return _sqlValuesByIndex.begin(); };
		vector<vector<SqlValue>>::const_iterator end() const { return _sqlValuesByIndex.end(); };
		vector<SqlValue> &operator[](int index) { return _sqlValuesByIndex[index]; }

		void setCount(int32_t count) { _count = count; }
		int32_t getCount() { return _count; }
		void setCountSqlDuration(chrono::milliseconds countSqlDuration) { _countSqlDuration = countSqlDuration; }
		chrono::milliseconds getCountSqlDuration() { return _countSqlDuration; }
		void setSqlDuration(chrono::milliseconds sqlDuration) { _sqlDuration = sqlDuration; }
		chrono::milliseconds getSqlDuration() { return _sqlDuration; }
	};

	/*
	class SqlResultSetByIndex : public SqlResultSet
	{
	  private:
		vector<SqlValue> _sqlCurrentRowValuesByIndex;
		// per ogni riga (vector) abbiamo un vettore che contiene i valori delle colonne by Index
		vector<vector<SqlValue>> _sqlValuesByIndex;

	  public:
		virtual void clearData()
		{
			_sqlValuesByIndex.clear();
			SqlResultSet::clearData();
		};
		virtual void addColumnValueToCurrentRow(string fieldName, SqlValue sqlValue) { _sqlCurrentRowValuesByIndex.push_back(sqlValue); };
		virtual void addCurrentRow()
		{
			_sqlValuesByIndex.push_back(_sqlCurrentRowValuesByIndex);
			_sqlCurrentRowValuesByIndex.clear();
		};
		virtual size_t size() { return _sqlValuesByIndex.size(); };
		virtual json asJson();
		vector<vector<SqlValue>>::iterator begin() { return _sqlValuesByIndex.begin(); };
		vector<vector<SqlValue>>::iterator end() { return _sqlValuesByIndex.end(); };
		vector<vector<SqlValue>>::const_iterator begin() const { return _sqlValuesByIndex.begin(); };
		vector<vector<SqlValue>>::const_iterator end() const { return _sqlValuesByIndex.end(); };
		vector<SqlValue> &operator[](int index) { return _sqlValuesByIndex[index]; }
	};

	class SqlResultSetByName : public SqlResultSet
	{
	  private:
		map<string, SqlValue> _sqlCurrentRowValuesByName;
		// per ogni riga (vector) abbiamo una mappa che contiene i valori delle colonne by Name
		vector<map<string, SqlValue>> _sqlValuesByName;

	  public:
		virtual void clearData()
		{
			_sqlValuesByName.clear();
			SqlResultSet::clearData();
		};
		virtual void addColumnValueToCurrentRow(string fieldName, SqlValue sqlValue)
		{
			auto it = _sqlCurrentRowValuesByName.find(fieldName);
			if (it == _sqlCurrentRowValuesByName.end())
				_sqlCurrentRowValuesByName.insert(make_pair(fieldName, sqlValue));
			else
				// se il nome della colonna è già presente, aggiungiamo anche l'indice della colonna
				_sqlCurrentRowValuesByName.insert(make_pair(fmt::format("{} - {}", fieldName, _sqlCurrentRowValuesByName.size()), sqlValue));
		};
		virtual void addCurrentRow()
		{
			_sqlValuesByName.push_back(_sqlCurrentRowValuesByName);
			_sqlCurrentRowValuesByName.clear();
		};
		virtual size_t size() { return _sqlValuesByName.size(); };
		virtual json asJson();
		vector<map<string, SqlValue>>::iterator begin() { return _sqlValuesByName.begin(); };
		vector<map<string, SqlValue>>::iterator end() { return _sqlValuesByName.end(); };
		vector<map<string, SqlValue>>::const_iterator begin() const { return _sqlValuesByName.begin(); };
		vector<map<string, SqlValue>>::const_iterator end() const { return _sqlValuesByName.end(); };
		map<string, SqlValue> &operator[](int index) { return _sqlValuesByName[index]; }
	};
*/

  public:
	PostgresHelper();
	~PostgresHelper();
	void loadSqlColumnsSchema(PostgresConnTrans &trans);
	map<string, shared_ptr<SqlColumnSchema>> getSqlTableSchema(string tableName)
	{
		auto it = _sqlTablesColumnsSchema.find(tableName);
		if (it == _sqlTablesColumnsSchema.end())
			throw runtime_error(std::format("table {} not found", tableName));
		return it->second;
	}

	string getSqlColumnType(string tableName, string columnName)
	{
		map<string, shared_ptr<SqlColumnSchema>> sqlTableSchema = getSqlTableSchema(tableName);
		auto it = sqlTableSchema.find(columnName);
		if (it == sqlTableSchema.end())
			throw runtime_error(std::format("column {}.{} not found", tableName, columnName));

		shared_ptr<SqlColumnSchema> sqlColumnSchema = it->second;

		return sqlColumnSchema->dataType;
	}

	string buildQueryColumns(vector<string> &requestedColumns, bool convertDateFieldsToUtc = false);
	shared_ptr<PostgresHelper::SqlResultSet> buildResult(result result);

  private:
	map<string, map<string, shared_ptr<SqlColumnSchema>>> _sqlTablesColumnsSchema;

	string getQueryColumn(
		shared_ptr<SqlColumnSchema> sqlColumnSchema, string requestedTableNameAlias, string requestedColumnName = "",
		bool convertDateFieldsToUtc = false
	);
	string getColumnName(shared_ptr<SqlColumnSchema> sqlColumnSchema, string requestedTableNameAlias, string requestedColumnName);
	bool isDataTypeManaged(string dataType, string arrayDataType);
};

#endif
