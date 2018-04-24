/* Copyright 2013 Active911 Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http: *www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "catralibraries/DBConnectionPool.h"
#include <string>
#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>


class MySQLConnection : public DBConnection {

public:
    shared_ptr<sql::Connection> _sqlConnection;

    MySQLConnection(): DBConnection() 
    {
    }

    MySQLConnection(string selectTestingConnection, int connectionId):
		DBConnection(selectTestingConnection, connectionId) 
    {
	}

    ~MySQLConnection() 
    {
        if(_sqlConnection) 
        {
			DB_DEBUG_LOGGER(__FILEREF__ + "sql connection destruct"
				", _connectionId: " + to_string(_connectionId)
			);

            _sqlConnection->close();
            _sqlConnection.reset(); 	// Release and destruct
        }
    };

	virtual bool connectionValid()
	{
		bool connectionValid = true;

		if (_sqlConnection == nullptr)
		{
			DB_ERROR_LOGGER(__FILEREF__ + "sql connection is null"
				+ ", _connectionId: " + to_string(_connectionId)
			);
			connectionValid = false;
		}
		else
		{
			if (_selectTestingConnection != "" && _sqlConnection != nullptr)
			{
				try
				{
					shared_ptr<sql::PreparedStatement> preparedStatement (
						_sqlConnection->prepareStatement(_selectTestingConnection));
					shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
					if (resultSet->next())
					{
						int count     = resultSet->getInt(1);
					}
					else
					{
						connectionValid = false;
					}
				}
				catch(sql::SQLException se)
				{
					DB_ERROR_LOGGER(__FILEREF__ + "sql connection exception"
						+ ", _connectionId: " + to_string(_connectionId)
						+ ", se.what(): " + se.what()
					);

					connectionValid = false;
				}
				catch(exception e)
				{
					DB_ERROR_LOGGER(__FILEREF__ + "sql connection exception"
						+ ", _connectionId: " + to_string(_connectionId)
						+ ", e.what(): " + e.what()
					);

					connectionValid = false;
				}
			}
		}

		return connectionValid;
	}
};


class MySQLConnectionFactory : public DBConnectionFactory {

private:
    string _dbServer;
    string _dbUsername;
    string _dbPassword;
    string _dbName;
	string _selectTestingConnection;

public:
    MySQLConnectionFactory(string dbServer, string dbUsername, string dbPassword, string dbName,
			string selectTestingConnection) 
    {
        _dbServer = dbServer;
        _dbUsername = dbUsername;
        _dbPassword = dbPassword;
        _dbName = dbName;
		_selectTestingConnection = selectTestingConnection;
    };

    // Any exceptions thrown here should be caught elsewhere
    shared_ptr<DBConnection> create(int connectionId) {

        sql::Driver *driver;
        driver = get_driver_instance();

        // server like "tcp://127.0.0.1:3306"
        shared_ptr<sql::Connection> connectionFromDriver (driver->connect(_dbServer, _dbUsername, _dbPassword));
		bool reconnect_state = true;
		connectionFromDriver->setClientOption("OPT_RECONNECT", &reconnect_state);    
        connectionFromDriver->setSchema(_dbName);

        shared_ptr<MySQLConnection>     mySqlConnection = make_shared<MySQLConnection>(
				_selectTestingConnection, connectionId);
        mySqlConnection->_sqlConnection = connectionFromDriver;

		bool connectionValid = mySqlConnection->connectionValid();
		if (!connectionValid)
		{
			string errorMessage = string("just created sql connection is not valid")
				+ ", _connectionId: " + to_string(mySqlConnection->getConnectionId())
				+ ", _dbServer: " + _dbServer
				+ ", _dbUsername: " + _dbUsername
				+ ", _dbName: " + _dbName
			;
			DB_ERROR_LOGGER(__FILEREF__ + errorMessage);

			return nullptr;
		}
		else
		{
			DB_DEBUG_LOGGER(__FILEREF__ + "just created sql connection"
					+ ", _connectionId: " + to_string(mySqlConnection->getConnectionId())
					+ ", _dbServer: " + _dbServer
					+ ", _dbUsername: " + _dbUsername
					+ ", _dbName: " + _dbName
					);
		}

        return static_pointer_cast<DBConnection>(mySqlConnection);
    };

};
