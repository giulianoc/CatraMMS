
#include "JSONUtils.h"
#include <algorithm>
#include "MMSEngineDBFacade.h"
#include "MMSCURL.h"
#include "catralibraries/Convert.h"


int64_t MMSEngineDBFacade::addEncoder(
    string label,
	bool external,
	bool enabled,
    string protocol,
	string publicServerName,
	string internalServerName,
	int port
	)
{
    string      lastSQLCommand;
    int64_t		encoderKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "insert into MMS_Encoder(label, external, enabled, protocol, "
				"publicServerName, internalServerName, port "
				// "maxTranscodingCapability, maxLiveProxiesCapabilities, maxLiveRecordingCapabilities"
				") values ("
                "?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setInt(queryParameterIndex++, external ? 1 : 0);
            preparedStatement->setInt(queryParameterIndex++, enabled ? 1 : 0);
            preparedStatement->setString(queryParameterIndex++, protocol);
            preparedStatement->setString(queryParameterIndex++, publicServerName);
            preparedStatement->setString(queryParameterIndex++, internalServerName);
            preparedStatement->setInt(queryParameterIndex++, port);
            // preparedStatement->setInt(queryParameterIndex++, maxTranscodingCapability);
            // preparedStatement->setInt(queryParameterIndex++, maxLiveProxiesCapabilities);
            // preparedStatement->setInt(queryParameterIndex++, maxLiveRecordingCapabilities);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label: " + label
				+ ", external: " + to_string(external)
				+ ", enabled: " + to_string(enabled)
				+ ", protocol: " + protocol
				+ ", publicServerName: " + publicServerName
				+ ", internalServerName: " + internalServerName
				+ ", port: " + to_string(port)
				// + ", maxTranscodingCapability: " + to_string(maxTranscodingCapability)
				// + ", maxLiveProxiesCapabilities: " + to_string(maxLiveProxiesCapabilities)
				// + ", maxLiveRecordingCapabilities: " + to_string(maxLiveRecordingCapabilities)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            encoderKey = getLastInsertId(conn);
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return encoderKey;
}

void MMSEngineDBFacade::modifyEncoder(
    int64_t encoderKey,
    bool labelToBeModified, string label,
    bool externalToBeModified, bool external,
    bool enabledToBeModified, bool enabled,
    bool protocolToBeModified, string protocol,
	bool publicServerNameToBeModified, string publicServerName,
	bool internalServerNameToBeModified, string internalServerName,
	bool portToBeModified, int port
	)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
			string setSQL = "set ";
			bool oneParameterPresent = false;

			if (labelToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("label = ?");
				oneParameterPresent = true;
			}

			if (externalToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("external = ?");
				oneParameterPresent = true;
			}

			if (enabledToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("enabled = ?");
				oneParameterPresent = true;
			}

			if (protocolToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("protocol = ?");
				oneParameterPresent = true;
			}

			if (publicServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("publicServerName = ?");
				oneParameterPresent = true;
			}

			if (internalServerNameToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("internalServerName = ?");
				oneParameterPresent = true;
			}

			if (portToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("port = ?");
				oneParameterPresent = true;
			}

			/*
			if (maxTranscodingCapabilityToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxTranscodingCapability = ?");
				oneParameterPresent = true;
			}

			if (maxLiveProxiesCapabilitiesToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxLiveProxiesCapabilities = ?");
				oneParameterPresent = true;
			}

			if (maxLiveRecordingCapabilitiesToBeModified)
			{
				if (oneParameterPresent)
					setSQL += (", ");
				setSQL += ("maxLiveRecordingCapabilities = ?");
				oneParameterPresent = true;
			}
			*/

			if (!oneParameterPresent)
            {
                string errorMessage = __FILEREF__ + "Wrong input, no parameters to be updated"
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", oneParameterPresent: " + to_string(oneParameterPresent)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            lastSQLCommand = 
                string("update MMS_Encoder ") + setSQL + " "
				"where encoderKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (labelToBeModified)
				preparedStatement->setString(queryParameterIndex++, label);
			if (externalToBeModified)
				preparedStatement->setInt(queryParameterIndex++, external ? 1 : 0);
			if (enabledToBeModified)
				preparedStatement->setInt(queryParameterIndex++, enabled ? 1 : 0);
			if (protocolToBeModified)
				preparedStatement->setString(queryParameterIndex++, protocol);
			if (publicServerNameToBeModified)
				preparedStatement->setString(queryParameterIndex++, publicServerName);
			if (internalServerNameToBeModified)
				preparedStatement->setString(queryParameterIndex++, internalServerName);
			if (portToBeModified)
				preparedStatement->setInt(queryParameterIndex++, port);
			// if (maxTranscodingCapabilityToBeModified)
			// 	preparedStatement->setInt(queryParameterIndex++, maxTranscodingCapability);
			// if (maxLiveProxiesCapabilitiesToBeModified)
			// 	preparedStatement->setInt(queryParameterIndex++, maxLiveProxiesCapabilities);
			// if (maxLiveRecordingCapabilitiesToBeModified)
			// 	preparedStatement->setInt(queryParameterIndex++, maxLiveRecordingCapabilities);
            preparedStatement->setInt64(queryParameterIndex++, encoderKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", label (" + to_string(labelToBeModified) + "): " + label
				+ ", external (" + to_string(externalToBeModified) + "): " + to_string(external)
				+ ", enabled (" + to_string(enabledToBeModified) + "): " + to_string(enabled)
				+ ", protocol (" + to_string(protocolToBeModified) + "): " + protocol
				+ ", publicServerName (" + to_string(publicServerNameToBeModified) + "): " + publicServerName
				+ ", internalServerName (" + to_string(internalServerNameToBeModified) + "): " + internalServerName
				+ ", port (" + to_string(portToBeModified) + "): " + to_string(port)
				// + ", maxTranscodingCapability (" + to_string(maxTranscodingCapabilityToBeModified)
				// 	+ "): " + to_string(maxTranscodingCapability)
				// + ", maxLiveProxiesCapabilities (" + to_string(maxLiveProxiesCapabilitiesToBeModified) + "): "
				// 	+ to_string(maxLiveProxiesCapabilities)
				// + ", maxLiveRecordingCapabilities (" + to_string(maxLiveRecordingCapabilitiesToBeModified) + "): "
				// 	+ to_string(maxLiveRecordingCapabilities)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                /*
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", confKey: " + to_string(confKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
                */
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }      
}

void MMSEngineDBFacade::removeEncoder(
    int64_t encoderKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
                "delete from MMS_Encoder where encoderKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encoderKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

tuple<string, string, string> MMSEngineDBFacade::getEncoderDetails (
	int64_t encoderKey
)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getEncoderDetails"
            + ", encoderKey: " + to_string(encoderKey)
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		string label;
		string publicServerName;
		string internalServerName;

		{
			lastSQLCommand =
				"select label, publicServerName, internalServerName "
				"from MMS_Encoder "
				"where encoderKey = ?"
				;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage = string("Encoder was not found")
					+ ", encoderKey: " + to_string(encoderKey)
				;
				_logger->error(errorMessage);

				throw EncoderNotFound(errorMessage);
			}

			label = static_cast<string>(resultSet->getString("label"));
			publicServerName = static_cast<string>(resultSet->getString("publicServerName"));
			internalServerName = static_cast<string>(resultSet->getString("internalServerName"));
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return make_tuple(label, publicServerName, internalServerName);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(EncoderNotFound e)
    {        
        _logger->error(__FILEREF__ + "Encoder Not Found"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		addAssociationWorkspaceEncoder(workspaceKey, encoderKey, conn);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey,
	shared_ptr<MySQLConnection> conn)
{
    string      lastSQLCommand;

	_logger->info(__FILEREF__ + "Received addAssociationWorkspaceEncoder"
		+ ", workspaceKey: " + to_string(workspaceKey)
		+ ", encoderKey: " + to_string(encoderKey)
	);

    try
    {
        {
            lastSQLCommand = 
                "insert into MMS_EncoderWorkspaceMapping (workspaceKey, encoderKey) "
				"values (?, ?) ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, encoderKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        throw e;
    }        
}

void MMSEngineDBFacade::addAssociationWorkspaceEncoder(
    int64_t workspaceKey,
	string sharedEncodersPoolLabel, Json::Value sharedEncodersLabel)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		vector<int64_t> encoderKeys;
		for (int encoderIndex = 0; encoderIndex < sharedEncodersLabel.size();
			encoderIndex++)
        {
			string encoderLabel = sharedEncodersLabel[encoderIndex].asString();

			lastSQLCommand = "select encoderKey from MMS_Encoder "
				"where label = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setString(queryParameterIndex++, encoderLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderLabel: " + encoderLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
				encoderKeys.push_back(resultSet->getInt64("encoderKey"));
			else
			{
				string errorMessage = string("No encoder label found")
					+ ", encoderLabel: " + encoderLabel
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
        }

		for (int64_t encoderKey: encoderKeys)
			addAssociationWorkspaceEncoder(workspaceKey, encoderKey, conn);

		addEncodersPool(workspaceKey, sharedEncodersPoolLabel, encoderKeys);

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

void MMSEngineDBFacade::removeAssociationWorkspaceEncoder(
    int64_t workspaceKey, int64_t encoderKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            lastSQLCommand = 
                "delete from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = ? and encoderKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            preparedStatement->setInt64(queryParameterIndex++, encoderKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", encoderKey: " + to_string(encoderKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

Json::Value MMSEngineDBFacade::getEncoderWorkspacesAssociation(
    int64_t encoderKey)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		Json::Value encoderWorkspacesAssociatedRoot(Json::arrayValue);
        {
			lastSQLCommand = "select w.workspaceKey, w.name "
				"from MMS_Workspace w, MMS_EncoderWorkspaceMapping ewm "
				"where w.workspaceKey = ewm.workspaceKey and ewm.encoderKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
			{
				Json::Value encoderWorkspaceAssociatedRoot;
    
				string field = "workspaceKey";
				encoderWorkspaceAssociatedRoot[field] = resultSet->getInt64("workspaceKey");

				field = "workspaceName";
				encoderWorkspaceAssociatedRoot[field] = static_cast<string>(resultSet->getString("name"));

				encoderWorkspacesAssociatedRoot.append(encoderWorkspaceAssociatedRoot);
			}
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return encoderWorkspacesAssociatedRoot;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}


Json::Value MMSEngineDBFacade::getEncoderList (
	bool admin,
	int start, int rows,
	bool allEncoders, int64_t workspaceKey, bool runningInfo, int64_t encoderKey,
	string label, string serverName, int port,
	string labelOrder	// "" or "asc" or "desc"
)
{
    string      lastSQLCommand;
    Json::Value encoderListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEncoderList"
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", allEncoders: " + to_string(allEncoders)
            + ", runningInfo: " + to_string(runningInfo)
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encoderKey: " + to_string(encoderKey)
            + ", label: " + label
            + ", serverName: " + serverName
            + ", port: " + to_string(port)
            + ", labelOrder: " + labelOrder
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;

			/*
			{
				field = "allEncoders";
				requestParametersRoot[field] = allEncoders;
			}

			{
				field = "workspaceKey";
				requestParametersRoot[field] = workspaceKey;
			}
			*/

            if (encoderKey != -1)
			{
				field = "encoderKey";
				requestParametersRoot[field] = encoderKey;
			}
            
			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}
            
            if (label != "")
			{
				field = "label";
				requestParametersRoot[field] = label;
			}
            
            if (serverName != "")
			{
				field = "serverName";
				requestParametersRoot[field] = serverName;
			}

            if (port != -1)
			{
				field = "port";
				requestParametersRoot[field] = port;
			}

			{
				field = "runningInfo";
				requestParametersRoot[field] = runningInfo;
			}

            if (labelOrder != "")
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

            field = "requestParameters";
            encoderListRoot[field] = requestParametersRoot;
        }
        
        string sqlWhere;
        if (encoderKey != -1)
		{
			if (sqlWhere != "")
				sqlWhere += ("and e.encoderKey = ? ");
			else
				sqlWhere += ("e.encoderKey = ? ");
		}
        if (label != "")
		{
			if (sqlWhere != "")
				sqlWhere += ("and LOWER(e.label) like LOWER(?) ");
			else
				sqlWhere += ("LOWER(e.label) like LOWER(?) ");
		}
        if (serverName != "")
		{
			if (sqlWhere != "")
				sqlWhere += ("and (e.publicServerName like ? or e.internalServerName like ?) ");
			else
				sqlWhere += ("(e.publicServerName like ? or e.internalServerName like ?) ");
		}
        if (port != -1)
		{
			if (sqlWhere != "")
				sqlWhere += ("and e.port = ? ");
			else
				sqlWhere += ("e.port = ? ");
		}

		if (allEncoders)
		{
			// using just MMS_Encoder
			if (sqlWhere != "")
				sqlWhere = string ("where ") + sqlWhere;
		}
		else
		{
			// join with MMS_EncoderWorkspaceMapping
			if (sqlWhere != "")
				sqlWhere = "where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = ? and " + sqlWhere;
			else
				sqlWhere = "where e.encoderKey = ewm.encoderKey "
					"and ewm.workspaceKey = ? ";
		}

        Json::Value responseRoot;
        {
			if (allEncoders)
			{
				lastSQLCommand = string("select count(*) from MMS_Encoder e ")
					+ sqlWhere;
			}
			else
			{
				lastSQLCommand = string("select count(*) "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm ")
					+ sqlWhere;
			}

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (!allEncoders)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encoderKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, encoderKey);
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            if (serverName != "")
			{
                preparedStatement->setString(queryParameterIndex++, string("%") + serverName + "%");
                preparedStatement->setString(queryParameterIndex++, string("%") + serverName + "%");
			}
            if (port != -1)
                preparedStatement->setInt(queryParameterIndex++, port);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ (!allEncoders ? (", workspaceKey: " + to_string(workspaceKey)) : "")
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", label: " + "%" + label + "%"
				+ ", serverName: " + "%" + serverName + "%"
				+ ", port: " + to_string(port)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value encodersRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

			if (allEncoders)
				lastSQLCommand = 
					string("select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
						"e.publicServerName, e.internalServerName, e.port "
						// ", e.maxTranscodingCapability, "
						// "e.maxLiveProxiesCapabilities, e.maxLiveRecordingCapabilities "
						"from MMS_Encoder e ") 
                + sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";
			else
				lastSQLCommand = 
					string("select e.encoderKey, e.label, e.external, e.enabled, e.protocol, "
						"e.publicServerName, e.internalServerName, e.port "
						// ", e.maxTranscodingCapability, "
						// "e.maxLiveProxiesCapabilities, e.maxLiveRecordingCapabilities "
						"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm ") 
                + sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			if (!allEncoders)
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encoderKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, encoderKey);
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            if (serverName != "")
			{
                preparedStatement->setString(queryParameterIndex++, string("%") + serverName + "%");
                preparedStatement->setString(queryParameterIndex++, string("%") + serverName + "%");
			}
            if (port != -1)
                preparedStatement->setInt(queryParameterIndex++, port);
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ (!allEncoders ? (", workspaceKey: " + to_string(workspaceKey)) : "")
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", label: " + "%" + label + "%"
				+ ", serverName: " + "%" + serverName + "%"
				+ ", port: " + to_string(port)
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
                Json::Value encoderRoot = getEncoderRoot(admin, runningInfo, resultSet);

                encodersRoot.append(encoderRoot);
            }
        }

        field = "encoders";
        responseRoot[field] = encodersRoot;

        field = "response";
        encoderListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return encoderListRoot;
}

Json::Value MMSEngineDBFacade::getEncoderRoot (
	bool admin,
	bool runningInfo,
	shared_ptr<sql::ResultSet> resultSet
)
{
    Json::Value encoderRoot;
    
    try
    {
		int64_t encoderKey = resultSet->getInt64("encoderKey");

        string field = "encoderKey";
		encoderRoot[field] = encoderKey;

		field = "label";
		encoderRoot[field] = static_cast<string>(resultSet->getString("label"));

		field = "external";
		bool external = resultSet->getInt("external") == 1 ? true : false;
		encoderRoot[field] = external;

		field = "enabled";
		encoderRoot[field] = resultSet->getInt("enabled") == 1 ? true : false;

		field = "protocol";
		string protocol = resultSet->getString("protocol");
		encoderRoot[field] = protocol;

		field = "publicServerName";
		string publicServerName = resultSet->getString("publicServerName");
		encoderRoot[field] = publicServerName;

		field = "internalServerName";
		string internalServerName = resultSet->getString("internalServerName");
		encoderRoot[field] = internalServerName;

		field = "port";
		int port = resultSet->getInt("port");
		encoderRoot[field] = port;

		// 2022-1-30: running and cpu usage takes a bit of time
		//		scenario: some MMS WEB pages loading encoder info, takes a bit of time
		//		to be loaded because of this check and, in these pages, we do not care about
		//		running info, so we made it optional
		if (runningInfo)
		{
			bool running;
			int cpuUsage = 0;
			pair<bool, int> encoderRunningDetails = getEncoderInfo(external, protocol,
				publicServerName, internalServerName, port);
			tie(running, cpuUsage) = encoderRunningDetails;

			field = "running";
			encoderRoot[field] = running;

			field = "cpuUsage";
			encoderRoot[field] = cpuUsage;
		}

		if (admin)
		{
			field = "workspacesAssociated";
			encoderRoot[field] = getEncoderWorkspacesAssociation(encoderKey);
		}
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
        );

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
        );

        throw e;
    } 
    
    return encoderRoot;
}

bool MMSEngineDBFacade::isEncoderRunning(
	bool external, string protocol,
	string publicServerName, string internalServerName,
	int port)
{
	bool isRunning = true;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://"
			+ (external ? publicServerName : internalServerName)
			+ ":" + to_string(port)
            + _ffmpegEncoderStatusURI
		;

		Json::Value infoResponseRoot = MMSCURL::httpGetJson(
			-1,	// ingestionJobKey
			ffmpegEncoderURL,
			_ffmpegEncoderInfoTimeout,
			_ffmpegEncoderUser,
			_ffmpegEncoderPassword,
			_logger
		);
	}
	catch (ServerNotReachable e)
	{
		_logger->error(__FILEREF__ + "Encoder is not reachable, is it down?"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
    }
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Status URL failed (exception)"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
    }
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Status URL failed (exception)"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
	}

	return isRunning;
}


pair<bool, int> MMSEngineDBFacade::getEncoderInfo(
	bool external, string protocol,
	string publicServerName, string internalServerName,
	int port)
{
	bool isRunning = true;
	int cpuUsage = 0;

	string ffmpegEncoderURL;
	try
	{
		ffmpegEncoderURL = protocol + "://"
			+ (external ? publicServerName : internalServerName)
			+ ":" + to_string(port)
            + _ffmpegEncoderInfoURI
		;

		Json::Value infoResponseRoot = MMSCURL::httpGetJson(
			-1,	// ingestionJobKey
			ffmpegEncoderURL,
			_ffmpegEncoderInfoTimeout,
			_ffmpegEncoderUser,
			_ffmpegEncoderPassword,
			_logger
		);

		string field = "cpuUsage";
		cpuUsage = JSONUtils::asInt(infoResponseRoot, field, 0);
	}
	catch (ServerNotReachable e)
	{
		_logger->error(__FILEREF__ + "Encoder is not reachable, is it down?"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
    }
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Status URL failed (exception)"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
    }
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Status URL failed (exception)"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		isRunning = false;
	}

	return make_pair(isRunning, cpuUsage);
}


string MMSEngineDBFacade::getEncodersPoolDetails (int64_t encodersPoolKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getEncodersPoolDetails"
            + ", encodersPoolKey: " + to_string(encodersPoolKey)
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		string label;

		{
			lastSQLCommand =
				"select label from MMS_EncodersPool "
				"where encodersPoolKey = ?"
			;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
				string errorMessage = __FILEREF__ + "EncodersPool was not found"
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			label = static_cast<string>(resultSet->getString("label"));
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;

		return label;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
}

Json::Value MMSEngineDBFacade::getEncodersPoolList (
	int start, int rows,
	int64_t workspaceKey, int64_t encodersPoolKey, string label,
	string labelOrder	// "" or "asc" or "desc"
)
{
    string      lastSQLCommand;
    Json::Value encodersPoolListRoot;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEncodersPoolList"
            + ", start: " + to_string(start)
            + ", rows: " + to_string(rows)
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodersPoolKey: " + to_string(encodersPoolKey)
            + ", label: " + label
            + ", labelOrder: " + labelOrder
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

        {
            Json::Value requestParametersRoot;

            if (encodersPoolKey != -1)
			{
				field = "encodersPoolKey";
				requestParametersRoot[field] = encodersPoolKey;
			}
            
			{
				field = "start";
				requestParametersRoot[field] = start;
			}
            
			{
				field = "rows";
				requestParametersRoot[field] = rows;
			}
            
            if (label != "")
			{
				field = "label";
				requestParametersRoot[field] = label;
			}
            
            if (labelOrder != "")
			{
				field = "labelOrder";
				requestParametersRoot[field] = labelOrder;
			}

            field = "requestParameters";
            encodersPoolListRoot[field] = requestParametersRoot;
        }
        
		// label == NULL is the "internal" EncodersPool representing the default encoders pool
		// for a workspace, the one using all the internal encoders associated to the workspace
		string sqlWhere = "where workspaceKey = ? and label is not NULL ";
        if (encodersPoolKey != -1)
			sqlWhere += ("and encodersPoolKey = ? ");
        if (label != "")
			sqlWhere += ("and LOWER(label) like LOWER(?) ");


        Json::Value responseRoot;
        {
			lastSQLCommand = string("select count(*) from MMS_EncodersPool ")
				+ sqlWhere;

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodersPoolKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", label: " + "%" + label + "%"
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            field = "numFound";
            responseRoot[field] = resultSet->getInt64(1);
        }

        Json::Value encodersPoolsRoot(Json::arrayValue);
        {
			string orderByCondition;
			if (labelOrder == "")
				orderByCondition = " ";
			else
				orderByCondition = "order by label " + labelOrder + " ";

			lastSQLCommand = 
				string("select encodersPoolKey, label from MMS_EncodersPool ") 
				+ sqlWhere
				+ orderByCondition
				+ "limit ? offset ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodersPoolKey != -1)
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
            if (label != "")
                preparedStatement->setString(queryParameterIndex++, string("%") + label + "%");
            preparedStatement->setInt(queryParameterIndex++, rows);
            preparedStatement->setInt(queryParameterIndex++, start);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", label: " + "%" + label + "%"
				+ ", rows: " + to_string(rows)
				+ ", start: " + to_string(start)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            while (resultSet->next())
            {
				Json::Value encodersPoolRoot;

				int64_t encodersPoolKey = resultSet->getInt64("encodersPoolKey");

				string field = "encodersPoolKey";
				encodersPoolRoot[field] = encodersPoolKey;

				field = "label";
				encodersPoolRoot[field] = static_cast<string>(resultSet->getString("label"));

				Json::Value encodersRoot(Json::arrayValue);
				{
					lastSQLCommand = 
						string("select encoderKey from MMS_EncoderEncodersPoolMapping ") 
						+ "where encodersPoolKey = ?";

					shared_ptr<sql::PreparedStatement> preparedStatementEncodersPool (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementEncodersPool->setInt64(queryParameterIndex++, encodersPoolKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSetEncodersPool (preparedStatementEncodersPool->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", encodersPoolKey: " + to_string(encodersPoolKey)
						+ ", resultSetEncodersPool->rowsCount: " + to_string(resultSetEncodersPool->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					while (resultSetEncodersPool->next())
					{
						int64_t encoderKey = resultSetEncodersPool->getInt64("encoderKey");

						{
							lastSQLCommand = 
								"select encoderKey, label, external, enabled, protocol, "
								"publicServerName, internalServerName, port "
								"from MMS_Encoder "
								"where encoderKey = ? ";

							shared_ptr<sql::PreparedStatement> preparedStatementEncoder (
								conn->_sqlConnection->prepareStatement(lastSQLCommand));
							int queryParameterIndex = 1;
							preparedStatementEncoder->setInt64(queryParameterIndex++, encoderKey);
							chrono::system_clock::time_point startSql = chrono::system_clock::now();
							shared_ptr<sql::ResultSet> resultSetEncoder (preparedStatementEncoder->executeQuery());
							_logger->info(__FILEREF__ + "@SQL statistics@"
								+ ", lastSQLCommand: " + lastSQLCommand
								+ ", encoderKey: " + to_string(encoderKey)
								+ ", resultSetEncoder->rowsCount: " + to_string(resultSetEncoder->rowsCount())
								+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
									chrono::system_clock::now() - startSql).count()) + "@"
							);
							if (resultSetEncoder->next())
							{
								bool admin = false;
								bool runningInfo = false;
								Json::Value encoderRoot = getEncoderRoot (
									admin, runningInfo, resultSetEncoder);

								encodersRoot.append(encoderRoot);
							}
							else
							{
								string errorMessage = string("No encoderKey found")
									+ ", encoderKey: " + to_string(encoderKey)
								;
								_logger->error(errorMessage);

								throw runtime_error(errorMessage);
							}
						}
					}
				}

				field = "encoders";
				encodersPoolRoot[field] = encodersRoot;

                encodersPoolsRoot.append(encodersPoolRoot);
            }
        }

        field = "encodersPool";
        responseRoot[field] = encodersPoolsRoot;

        field = "response";
        encodersPoolListRoot[field] = responseRoot;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    
    return encodersPoolListRoot;
}

int64_t MMSEngineDBFacade::addEncodersPool(
	int64_t workspaceKey,
    string label,
	vector<int64_t>& encoderKeys
	)
{
    string      lastSQLCommand;
    int64_t		encodersPoolKey;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		// check: every encoderKey shall be already associated to the workspace
		for(int64_t encoderKey: encoderKeys)
        {
            lastSQLCommand = 
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = ? and encoderKey = ? ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->getInt64(1) == 0)
				{
					string errorMessage = __FILEREF__
						+ "Encoder is not already associated to the workspace"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encoderKey: " + to_string(encoderKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}
		}

        {
            lastSQLCommand = 
                "insert into MMS_EncodersPool(workspaceKey, label, "
				"lastEncoderIndexUsed) values ( "
                "?, ?, 0)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setString(queryParameterIndex++, label);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", label: " + label
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);

            encodersPoolKey = getLastInsertId(conn);
        }

		for(int64_t encoderKey: encoderKeys)
        {
            lastSQLCommand = 
                "insert into MMS_EncoderEncodersPoolMapping(encodersPoolKey, "
				"encoderKey) values ( "
				"?, ?)";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return encodersPoolKey;
}

int64_t MMSEngineDBFacade::modifyEncodersPool(
	int64_t encodersPoolKey,
	int64_t workspaceKey,
    string newLabel,
	vector<int64_t>& newEncoderKeys
	)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
		_logger->info(__FILEREF__ + "Received modifyEncodersPool"
			+ ", encodersPoolKey: " + to_string(encodersPoolKey)
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", newLabel: " + newLabel
			+ ", newEncoderKeys.size: " + to_string(newEncoderKeys.size())
		);

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
		// check: every encoderKey shall be already associated to the workspace
		for(int64_t encoderKey: newEncoderKeys)
        {
            lastSQLCommand = 
				"select count(*) from MMS_EncoderWorkspaceMapping "
				"where workspaceKey = ? and encoderKey = ? ";
            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", elapsed (secs): @"
					+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				if (resultSet->getInt64(1) == 0)
				{
					string errorMessage = __FILEREF__
						+ "Encoder is not already associated to the workspace"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encoderKey: " + to_string(encoderKey)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);                    
				}
			}
		}

        {
			lastSQLCommand = 
				string("select label from MMS_EncodersPool ") 
				+ "where encodersPoolKey = ? ";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (resultSet->next())
            {
				string savedLabel = static_cast<string>(resultSet->getString("label"));
				if (savedLabel != newLabel)
				{
					lastSQLCommand = 
						string("update MMS_EncodersPool ")
						+ "set label = ? "
						"where encodersPoolKey = ?";

					shared_ptr<sql::PreparedStatement> preparedStatement (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatement->setString(queryParameterIndex++, newLabel);
					preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);

					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					int rowsUpdated = preparedStatement->executeUpdate();
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", newLabel: " + newLabel
						+ ", encodersPoolKey: " + to_string(encodersPoolKey)
						+ ", rowsUpdated: " + to_string(rowsUpdated)
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					if (rowsUpdated != 1)
					{
						string errorMessage = __FILEREF__ + "no update was done"
							+ ", newLabel: " + newLabel
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", lastSQLCommand: " + lastSQLCommand
						;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);                    
					}
				}

				vector<int64_t> savedEncoderKeys;
				{
					lastSQLCommand = 
						string("select encoderKey from MMS_EncoderEncodersPoolMapping ") 
						+ "where encodersPoolKey = ?";

					shared_ptr<sql::PreparedStatement> preparedStatementEncodersPool (
						conn->_sqlConnection->prepareStatement(lastSQLCommand));
					int queryParameterIndex = 1;
					preparedStatementEncodersPool->setInt64(queryParameterIndex++, encodersPoolKey);
					chrono::system_clock::time_point startSql = chrono::system_clock::now();
					shared_ptr<sql::ResultSet> resultSetEncodersPool (preparedStatementEncodersPool->executeQuery());
					_logger->info(__FILEREF__ + "@SQL statistics@"
						+ ", lastSQLCommand: " + lastSQLCommand
						+ ", encodersPoolKey: " + to_string(encodersPoolKey)
						+ ", resultSetEncodersPool->rowsCount: " + to_string(resultSetEncodersPool->rowsCount())
						+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
							chrono::system_clock::now() - startSql).count()) + "@"
					);
					while (resultSetEncodersPool->next())
					{
						int64_t encoderKey = resultSetEncodersPool->getInt64("encoderKey");

						savedEncoderKeys.push_back(encoderKey);
					}
				}

				// all the new encoderKey that are not present in savedEncoderKeys have to be added
				for(int64_t newEncoderKey: newEncoderKeys)
				{
					if (find(savedEncoderKeys.begin(), savedEncoderKeys.end(),
						newEncoderKey) == savedEncoderKeys.end())
					{
						lastSQLCommand = 
							"insert into MMS_EncoderEncodersPoolMapping("
							"encodersPoolKey, encoderKey) values ( "
							"?, ?)";

						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
						preparedStatement->setInt64(queryParameterIndex++, newEncoderKey);
						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							+ ", newEncoderKey: " + to_string(newEncoderKey)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
					}
				}

				// all the saved encoderKey that are not present in encoderKeys have to be removed
				for(int64_t savedEncoderKey: savedEncoderKeys)
				{
					if (find(newEncoderKeys.begin(), newEncoderKeys.end(),
						savedEncoderKey) == newEncoderKeys.end())
					{
						lastSQLCommand = 
							"delete from MMS_EncoderEncodersPoolMapping "
							"where encodersPoolKey = ? and encoderKey = ?";
						shared_ptr<sql::PreparedStatement> preparedStatement (
							conn->_sqlConnection->prepareStatement(lastSQLCommand));
						int queryParameterIndex = 1;
						preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
						preparedStatement->setInt64(queryParameterIndex++, savedEncoderKey);

						chrono::system_clock::time_point startSql = chrono::system_clock::now();
						int rowsUpdated = preparedStatement->executeUpdate();
						_logger->info(__FILEREF__ + "@SQL statistics@"
							+ ", lastSQLCommand: " + lastSQLCommand
							+ ", encodersPoolKey: " + to_string(encodersPoolKey)
							+ ", savedEncoderKey: " + to_string(savedEncoderKey)
							+ ", rowsUpdated: " + to_string(rowsUpdated)
							+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
								chrono::system_clock::now() - startSql).count()) + "@"
						);
						if (rowsUpdated != 1)
						{
							string errorMessage = __FILEREF__ + "no delete was done"
								+ ", encodersPoolKey: " + to_string(encodersPoolKey)
								+ ", savedEncoderKey: " + to_string(savedEncoderKey)
								+ ", rowsUpdated: " + to_string(rowsUpdated)
								+ ", lastSQLCommand: " + lastSQLCommand
							;
							_logger->warn(errorMessage);

							throw runtime_error(errorMessage);                    
						}
					}
				}
            }
			else
			{
                string errorMessage = __FILEREF__ + "No encodersPool found"
                        + ", encodersPoolKey: " + to_string(encodersPoolKey)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    

			}
        }

        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }  
    
    return encodersPoolKey;
}

void MMSEngineDBFacade::removeEncodersPool(
    int64_t encodersPoolKey)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        
        {
            lastSQLCommand = 
				"delete from MMS_EncodersPool where encodersPoolKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no delete was done"
                        + ", encodersPoolKey: " + to_string(encodersPoolKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
                            
        _logger->debug(__FILEREF__ + "DB connection unborrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    }        
}

tuple<int64_t, bool, string, string, string, int>
	MMSEngineDBFacade::getRunningEncoderByEncodersPool(
	int64_t workspaceKey, string encodersPoolLabel,
	int64_t encoderKeyToBeSkipped, bool externalEncoderAllowed)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;
    bool autoCommit = true;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _masterConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getRunningEncoderByEncodersPool"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodersPoolLabel: " + encodersPoolLabel
            + ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
            + ", externalEncoderAllowed: " + to_string(externalEncoderAllowed)
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		autoCommit = false;
		{
			lastSQLCommand = 
				"START TRANSACTION";

			shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
			statement->execute(lastSQLCommand);
		}

		int lastEncoderIndexUsed;
		int64_t encodersPoolKey;
        {
			if (encodersPoolLabel == "")
				lastSQLCommand = 
					string("select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool ") 
					+ "where workspaceKey = ? "
					+ "and label is null for update";
			else
				lastSQLCommand = 
					string("select encodersPoolKey, lastEncoderIndexUsed from MMS_EncodersPool ") 
					+ "where workspaceKey = ? "
					+ "and label = ? for update";

            shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
            if (encodersPoolLabel != "")
				preparedStatement->setString(queryParameterIndex++, encodersPoolLabel);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
			{
				string errorMessage = __FILEREF__ + "encodersPool was not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encodersPoolLabel: " + encodersPoolLabel
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			lastEncoderIndexUsed = resultSet->getInt("lastEncoderIndexUsed");
			encodersPoolKey = resultSet->getInt64("encodersPoolKey");
        }

		int encodersNumber;
		{
			if (encodersPoolLabel == "")
				lastSQLCommand = string("select count(*) from MMS_EncoderWorkspaceMapping ")
					+ "where workspaceKey = ? ";
			else
				lastSQLCommand = string("select count(*) from MMS_EncoderEncodersPoolMapping ")
					+ "where encodersPoolKey = ? ";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (encodersPoolLabel == "")
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			else
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (!resultSet->next())
            {
                string errorMessage ("select count(*) failed");

                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            encodersNumber = resultSet->getInt64(1);
        }

		int newLastEncoderIndexUsed = lastEncoderIndexUsed;

		int64_t encoderKey;
		bool external = false;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port;
		bool encoderFound = false;
		int encoderIndex = 0;

		while(!encoderFound && encoderIndex < encodersNumber)
		{
			encoderIndex++;

			newLastEncoderIndexUsed = (newLastEncoderIndexUsed + 1) % encodersNumber;

			if (encodersPoolLabel == "")
				lastSQLCommand = 
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderWorkspaceMapping ewm " 
					"where e.encoderKey = ewm.encoderKey and ewm.workspaceKey = ? "
					"order by e.publicServerName "
					"limit 1 offset ?";
			else
				lastSQLCommand = 
					"select e.encoderKey, e.enabled, e.external, e.protocol, "
					"e.publicServerName, e.internalServerName, e.port "
					"from MMS_Encoder e, MMS_EncoderEncodersPoolMapping eepm " 
					"where e.encoderKey = eepm.encoderKey and eepm.encodersPoolKey = ? "
					"order by e.publicServerName "
					"limit 1 offset ?";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			if (encodersPoolLabel == "")
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			else
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
			preparedStatement->setInt(queryParameterIndex++, newLastEncoderIndexUsed);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", newLastEncoderIndexUsed: " + to_string(newLastEncoderIndexUsed)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
			{
				encoderKey = resultSet->getInt64("encoderKey");

				bool enabled = resultSet->getInt("enabled") == 1 ? true : false;
				external = resultSet->getInt("external") == 1 ? true : false;
				protocol = resultSet->getString("protocol");
				publicServerName = resultSet->getString("publicServerName");
				internalServerName = resultSet->getString("internalServerName");
				port = resultSet->getInt("port");

				if (external && !externalEncoderAllowed)
				{
					_logger->info(__FILEREF__
						+ "getEncoderByEncodersPool, skipped encoderKey because external encoder are not allowed"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
						+ ", enabled: " + to_string(enabled)
					);

					continue;
				}
				else if (!enabled)
				{
					_logger->info(__FILEREF__ + "getEncoderByEncodersPool, skipped encoderKey because encoder not enabled"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
						+ ", enabled: " + to_string(enabled)
					);

					continue;
				}
				else if (encoderKeyToBeSkipped != -1 && encoderKeyToBeSkipped == encoderKey)
				{
					_logger->info(__FILEREF__ + "getEncoderByEncodersPool, skipped encoderKey"
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
						+ ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
					);

					continue;
				}
				else
				{
					if (!isEncoderRunning(external, protocol,
						publicServerName, internalServerName, port))
					{
						_logger->info(__FILEREF__
							+ "getEncoderByEncodersPool, dicarded encoderKey because not running"
							+ ", workspaceKey: " + to_string(workspaceKey)
							+ ", encodersPoolLabel: " + encodersPoolLabel
						);

						continue;
					}
				}

				encoderFound = true;
			}
			else
			{
				string errorMessage = __FILEREF__ + "Encoder details not found"
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		if (!encoderFound)
		{
			string errorMessage = __FILEREF__ + "Encoder was not found"
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", encoderKeyToBeSkipped: " + to_string(encoderKeyToBeSkipped)
			;
			_logger->error(errorMessage);

			throw EncoderNotFound(errorMessage);
		}

        {
			lastSQLCommand = 
				"update MMS_EncodersPool set lastEncoderIndexUsed = ? "
				"where encodersPoolKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
			preparedStatement->setInt(queryParameterIndex++, newLastEncoderIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);

			chrono::system_clock::time_point startSql = chrono::system_clock::now();
            int rowsUpdated = preparedStatement->executeUpdate();
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", newLastEncoderIndexUsed: " + to_string(newLastEncoderIndexUsed)
				+ ", encodersPoolKey: " + to_string(encodersPoolKey)
				+ ", rowsUpdated: " + to_string(rowsUpdated)
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
					+ ", newLastEncoderIndexUsed: " + to_string(newLastEncoderIndexUsed)
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
					+ ", rowsUpdated: " + to_string(rowsUpdated)
					+ ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

				// in case of one encoder, no update is done
				// because newLastEncoderIndexUsed is always the same

                // throw runtime_error(errorMessage);                    
			}
		}

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _logger->debug(__FILEREF__ + "DB connection unborrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;


		return make_tuple(encoderKey, external, protocol,
			publicServerName, internalServerName, port);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			try
			{
				// conn->_sqlConnection->rollback(); OR execute ROLLBACK
				if (!autoCommit)
				{
					shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
					statement->execute("ROLLBACK");
				}

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(sql::SQLException se)
			{
				_logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
					+ ", exceptionMessage: " + se.what()
				);

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "exception doing unborrow"
					+ ", exceptionMessage: " + e.what()
				);

				/*
					_logger->debug(__FILEREF__ + "DB connection unborrow"
						+ ", getConnectionId: " + to_string(conn->getConnectionId())
					);
					connectionPool->unborrow(conn);
					conn = nullptr;
				*/
			}
        }

        throw se;
    }    
    catch(EncoderNotFound e)
    {
        _logger->error(__FILEREF__ + "Encoder Not Found"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			try
			{
				// conn->_sqlConnection->rollback(); OR execute ROLLBACK
				if (!autoCommit)
				{
					shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
					statement->execute("ROLLBACK");
				}

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(sql::SQLException se)
			{
				_logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
					+ ", exceptionMessage: " + se.what()
				);

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "exception doing unborrow"
					+ ", exceptionMessage: " + e.what()
				);

				/*
					_logger->debug(__FILEREF__ + "DB connection unborrow"
						+ ", getConnectionId: " + to_string(conn->getConnectionId())
					);
					connectionPool->unborrow(conn);
					conn = nullptr;
				*/
			}
        }

        throw e;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			try
			{
				// conn->_sqlConnection->rollback(); OR execute ROLLBACK
				if (!autoCommit)
				{
					shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
					statement->execute("ROLLBACK");
				}

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(sql::SQLException se)
			{
				_logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
					+ ", exceptionMessage: " + se.what()
				);

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "exception doing unborrow"
					+ ", exceptionMessage: " + e.what()
				);

				/*
					_logger->debug(__FILEREF__ + "DB connection unborrow"
						+ ", getConnectionId: " + to_string(conn->getConnectionId())
					);
					connectionPool->unborrow(conn);
					conn = nullptr;
				*/
			}
        }

        throw e;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
			try
			{
				// conn->_sqlConnection->rollback(); OR execute ROLLBACK
				if (!autoCommit)
				{
					shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
					statement->execute("ROLLBACK");
				}

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(sql::SQLException se)
			{
				_logger->error(__FILEREF__ + "SQL exception doing ROLLBACK"
					+ ", exceptionMessage: " + se.what()
				);

				_logger->debug(__FILEREF__ + "DB connection unborrow"
					+ ", getConnectionId: " + to_string(conn->getConnectionId())
				);
				connectionPool->unborrow(conn);
				conn = nullptr;
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "exception doing unborrow"
					+ ", exceptionMessage: " + e.what()
				);

				/*
					_logger->debug(__FILEREF__ + "DB connection unborrow"
						+ ", getConnectionId: " + to_string(conn->getConnectionId())
					);
					connectionPool->unborrow(conn);
					conn = nullptr;
				*/
			}
        }

        throw e;
    } 
}

int MMSEngineDBFacade::getEncodersNumberByEncodersPool(
	int64_t workspaceKey, string encodersPoolLabel)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;
        
        _logger->info(__FILEREF__ + "getEncodersNumberByEncodersPool"
            + ", workspaceKey: " + to_string(workspaceKey)
            + ", encodersPoolLabel: " + encodersPoolLabel
        );
        
        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		int encodersNumber;
		if (encodersPoolLabel != "")
        {
			int64_t encodersPoolKey;
			{
				lastSQLCommand = 
					string("select encodersPoolKey from MMS_EncodersPool ") 
					+ "where workspaceKey = ? "
					+ "and label = ? ";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
				preparedStatement->setString(queryParameterIndex++, encodersPoolLabel);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodersPoolLabel: " + encodersPoolLabel
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", encodersPoolLabel: " + encodersPoolLabel
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (!resultSet->next())
				{
					string errorMessage = string("lastEncoderIndexUsed was not found")
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", encodersPoolLabel: " + encodersPoolLabel
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersPoolKey = resultSet->getInt64("encodersPoolKey");
			}

			{
				lastSQLCommand = string("select count(*) from MMS_EncoderEncodersPoolMapping ")
					+ "where encodersPoolKey = ? ";

				shared_ptr<sql::PreparedStatement> preparedStatement (
					conn->_sqlConnection->prepareStatement(lastSQLCommand));
				int queryParameterIndex = 1;
				preparedStatement->setInt64(queryParameterIndex++, encodersPoolKey);
				chrono::system_clock::time_point startSql = chrono::system_clock::now();
				shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
				_logger->info(__FILEREF__ + "@SQL statistics@"
					+ ", lastSQLCommand: " + lastSQLCommand
					+ ", encodersPoolLabel: " + encodersPoolLabel
					+ ", encodersPoolKey: " + to_string(encodersPoolKey)
					+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
					+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
						chrono::system_clock::now() - startSql).count()) + "@"
				);
				if (!resultSet->next())
				{
					string errorMessage ("select count(*) failed");

					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				encodersNumber = resultSet->getInt64(1);
			}
		}
		else
		{
			lastSQLCommand = string("select count(*) from MMS_EncoderWorkspaceMapping ")
				+ "where workspaceKey = ? ";

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, workspaceKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encodersPoolLabel: " + encodersPoolLabel
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (!resultSet->next())
			{
				string errorMessage ("select count(*) failed");

				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			encodersNumber = resultSet->getInt64(1);
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;


		return encodersNumber;
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
}

pair<string, bool> MMSEngineDBFacade::getEncoderURL(int64_t encoderKey,
	string serverName)
{
    string      lastSQLCommand;
    Json::Value encodersPoolListRoot;

    shared_ptr<MySQLConnection> conn = nullptr;

	shared_ptr<DBConnectionPool<MySQLConnection>> connectionPool = _slaveConnectionPool;

    try
    {
        string field;

        _logger->info(__FILEREF__ + "getEncoderURL"
			+ ", encoderKey: " + to_string(encoderKey)
        );

        conn = connectionPool->borrow();	
        _logger->debug(__FILEREF__ + "DB connection borrow"
            + ", getConnectionId: " + to_string(conn->getConnectionId())
        );

		bool external;
		string protocol;
		string publicServerName;
		string internalServerName;
		int port;
		{
			lastSQLCommand = 
				"select external, protocol, publicServerName, internalServerName, port "
				"from MMS_Encoder " 
				"where encoderKey = ? "
				// 2022-05-22. Scenario: tried to kill a job of a just disabled encoder 
				//	The kill did not work becuase this method did not return the URL
				//	because of the below sql condition.
				//	It was commented because it is not in this method that has to be
				//	checked the enabled flag, this method is specific to return a url
				//	of an encoder and ddoes not have to check the enabled flag
				// "and enabled = 1 "
			;

			shared_ptr<sql::PreparedStatement> preparedStatement (
				conn->_sqlConnection->prepareStatement(lastSQLCommand));
			int queryParameterIndex = 1;
			preparedStatement->setInt64(queryParameterIndex++, encoderKey);
			chrono::system_clock::time_point startSql = chrono::system_clock::now();
			shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
			_logger->info(__FILEREF__ + "@SQL statistics@"
				+ ", lastSQLCommand: " + lastSQLCommand
				+ ", encoderKey: " + to_string(encoderKey)
				+ ", resultSet->rowsCount: " + to_string(resultSet->rowsCount())
				+ ", elapsed (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - startSql).count()) + "@"
			);
			if (resultSet->next())
			{
				external = resultSet->getInt("external") == 1 ? true : false;
				protocol = resultSet->getString("protocol");
				publicServerName = resultSet->getString("publicServerName");
				internalServerName = resultSet->getString("internalServerName");
				port = resultSet->getInt("port");
			}
			else
			{
				string errorMessage = string("Encoder details not found or not enabled")
					+ ", encoderKey: " + to_string(encoderKey)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

        _logger->debug(__FILEREF__ + "DB connection unborrow"
			+ ", getConnectionId: " + to_string(conn->getConnectionId())
        );
        connectionPool->unborrow(conn);
		conn = nullptr;


		string encoderURL;
		if (serverName != "")
			encoderURL = protocol + "://" + serverName + ":" + to_string(port);
		else
		{
			if (external)
				encoderURL = protocol + "://" + publicServerName + ":" + to_string(port);
			else
				encoderURL = protocol + "://" + internalServerName + ":"
					+ to_string(port);
		}

        _logger->info(__FILEREF__ + "getEncoderURL"
			+ ", encoderKey: " + to_string(encoderKey)
			+ ", serverName: " + serverName
			+ ", encoderURL: " + encoderURL
        );

		return make_pair(encoderURL, external);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw se;
    }    
    catch(runtime_error e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", e.what(): " + e.what()
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", conn: " + (conn != nullptr ? to_string(conn->getConnectionId()) : "-1")
        );

        if (conn != nullptr)
        {
            _logger->debug(__FILEREF__ + "DB connection unborrow"
                + ", getConnectionId: " + to_string(conn->getConnectionId())
            );
            connectionPool->unborrow(conn);
			conn = nullptr;
        }

        throw e;
    } 
}

