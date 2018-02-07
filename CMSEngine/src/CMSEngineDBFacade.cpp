/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngineDBFacade.cpp
 * Author: giuliano
 * 
 * Created on January 27, 2018, 9:38 AM
 */

#include "CMSEngineDBFacade.h"

// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

CMSEngineDBFacade::CMSEngineDBFacade(
        size_t poolSize, 
        string dbServer, 
        string dbUsername, 
        string dbPassword, 
        string dbName,
        shared_ptr<spdlog::logger> logger) 
{
    _logger     = logger;
    
    _defaultContentProviderName     = "default";
    _defaultTerritoryName           = "default";
    
    _maxEncodingFailures            = 3;
    
    shared_ptr<MySQLConnectionFactory>  mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName);
        
    _connectionPool = make_shared<DBConnectionPool<MySQLConnection>>(poolSize, mySQLConnectionFactory);

    createTablesIfNeeded();
}

CMSEngineDBFacade::~CMSEngineDBFacade() 
{
}

vector<shared_ptr<Customer>> CMSEngineDBFacade::getCustomers()
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        "select CustomerKey, Name, DirectoryName, MaxStorageInGB, MaxEncodingPriority from CMS_Customers where IsEnabled = 1 and CustomerType in (1, 2)"));
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    vector<shared_ptr<Customer>>    customers;
    
    while (resultSet->next())
    {
        shared_ptr<Customer>    customer = make_shared<Customer>();
        
        customers.push_back(customer);
        
        customer->_customerKey = resultSet->getInt("CustomerKey");
        customer->_name = resultSet->getString("Name");
        customer->_directoryName = resultSet->getString("DirectoryName");
        customer->_maxStorageInGB = resultSet->getInt("MaxStorageInGB");
        customer->_maxEncodingPriority = resultSet->getInt("MaxEncodingPriority");

        getTerritories(customer);
    }

    _connectionPool->unborrow(conn);
    
    return customers;
}

shared_ptr<Customer> CMSEngineDBFacade::getCustomer(int64_t customerKey)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        "select CustomerKey, Name, DirectoryName, MaxStorageInGB, MaxEncodingPriority from CMS_Customers where CustomerKey = ?"));
    int queryParameterIndex = 1;
    preparedStatement->setInt64(queryParameterIndex++, customerKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    shared_ptr<Customer>    customer = make_shared<Customer>();
    
    if (resultSet->next())
    {
        customer->_customerKey = resultSet->getInt("CustomerKey");
        customer->_name = resultSet->getString("Name");
        customer->_directoryName = resultSet->getString("DirectoryName");
        customer->_maxStorageInGB = resultSet->getInt("MaxStorageInGB");
        customer->_maxEncodingPriority = resultSet->getInt("MaxEncodingPriority");

        getTerritories(customer);
    }
    else
    {
        _connectionPool->unborrow(conn);

        string errorMessage = string("select failed")
                + ", customerKey: " + to_string(customerKey);
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _connectionPool->unborrow(conn);
    
    return customer;
}

void CMSEngineDBFacade::getTerritories(shared_ptr<Customer> customer)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        "select TerritoryKey, Name from CMS_Territories t where CustomerKey = ?"));
    preparedStatement->setInt(1, customer->_customerKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    while (resultSet->next())
    {
        customer->_territories.insert(make_pair(resultSet->getInt("TerritoryKey"), resultSet->getString("Name")));
    }

    _connectionPool->unborrow(conn);
}

int64_t CMSEngineDBFacade::addCustomer(
	string customerName,
        string customerDirectoryName,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
        CustomerType customerType,
	string deliveryURL,
        bool enabled,
	EncodingPriority maxEncodingPriority,
        EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         customerKey;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        conn->_sqlConnection->setAutoCommit(autoCommit);    // or execute the statement START TRANSACTION
        
        {
            lastSQLCommand = 
                    "insert into CMS_Customers ("
                    "CustomerKey, CreationDate, Name, DirectoryName, Street, City, State, ZIP, Phone, CountryCode, CustomerType, DeliveryURL, IsEnabled, MaxEncodingPriority, EncodingPeriod, MaxIngestionsNumber, MaxStorageInGB, CurrentStorageUsageInGB, LanguageCode) values ("
                    "NULL, NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, customerName);
            preparedStatement->setString(queryParameterIndex++, customerDirectoryName);
            if (street == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, street);
            if (city == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, city);
            if (state == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, state);
            if (zip == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, zip);
            if (phone == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, phone);
            if (countryCode == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, countryCode);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(customerType));
            if (deliveryURL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryURL);
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(maxEncodingPriority));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPeriod));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionsNumber);
            preparedStatement->setInt(queryParameterIndex++, maxStorageInGB);
            preparedStatement->setString(queryParameterIndex++, languageCode);

            preparedStatement->executeUpdate();
        }
        
        customerKey = getLastInsertId(conn);
        
        {
            lastSQLCommand = 
                    "insert into CMS_CustomerMoreInfo (CustomerKey, CurrentDirLevel1, CurrentDirLevel2, CurrentDirLevel3, StartDateTime, EndDateTime, CurrentIngestionsNumber) values ("
                    "?, 0, 0, 0, NOW(), NOW(), 0)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                "insert into CMS_ContentProviders (ContentProviderKey, CustomerKey, Name) values ("
                "NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, _defaultContentProviderName);

            preparedStatement->executeUpdate();
        }

        contentProviderKey = getLastInsertId(conn);

        int64_t territoryKey = addTerritory(
                conn,
                customerKey,
                _defaultTerritoryName);
        
        int userType = getCMSUser();
        
        int64_t userKey = addUser (
                conn,
                customerKey,
                userName,
                userPassword,
                userType,
                userEmailAddress,
                userExpirationDate);

        // insert default EncodingProfilesSet per Customer/ContentType
        {
            vector<ContentType> contentTypes = { ContentType::Video, ContentType::Audio, ContentType::Image };
            
            for (ContentType contentType: contentTypes)
            {
                {
                    lastSQLCommand = 
                        "insert into CMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values ("
                        "NULL, ?, ?, NULL)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customerKey);
                    preparedStatement->executeUpdate();
                }
                        
                int64_t encodingProfilesSetKey = getLastInsertId(conn);

		// by default this new customer will inherited the profiles associated to 'global' 
                {
                    lastSQLCommand = 
                        "insert into CMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey) " 
                        "(select ?, EncodingProfileKey from CMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = " 
                            "(select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null))";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->executeUpdate();
                }
            }
        }

        conn->_sqlConnection->commit();     // or execute COMMIT
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
            conn->_sqlConnection->rollback();     // or execute ROLLBACK
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return customerKey;
}

int64_t CMSEngineDBFacade::addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string territoryName
)
{
    int64_t         territoryKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into CMS_Territories (TerritoryKey, CustomerKey, Name, Currency) values ("
    		"NULL, ?, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, territoryName);
            string currency("");
            if (currency == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, currency);

            preparedStatement->executeUpdate();
        }
        
        territoryKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return territoryKey;
}

int64_t CMSEngineDBFacade::addUser (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string userName,
        string password,
        int type,
        string emailAddress,
        chrono::system_clock::time_point expirationDate
)
{
    int64_t         userKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into CMS_Users2 (UserKey, UserName, Password, CustomerKey, Type, EMailAddress, CreationDate, ExpirationDate) values ("
                "NULL, ?, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, password);
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setInt(queryParameterIndex++, type);
            if (emailAddress == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, emailAddress);
            {
                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(expirationDate);
                
                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }
            
            preparedStatement->executeUpdate();
        }
        
        userKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return userKey;
}

int64_t CMSEngineDBFacade::addIngestionJob (
	int64_t customerKey,
        string metadataFileName,
        string metadataFileContent,
        IngestionType ingestionType,
        IngestionStatus ingestionStatus,
        string errorMessage
)
{
    int64_t         ingestionJobKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select c.IsEnabled, c.CustomerType from CMS_Customers c where c.CustomerKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int isEnabled = resultSet->getInt("IsEnabled");
                int customerType = resultSet->getInt("CustomerType");
                
                if (isEnabled != 1)
                {
                    string errorMessage = string("Customer is not enabled")
                        + ", customerKey: " + to_string(customerKey);
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
                else if (customerType != static_cast<int>(CustomerType::IngestionAndDelivery) &&
                        customerType != static_cast<int>(CustomerType::EncodingOnly))
                {
                    string errorMessage = string("Customer is not enabled to ingest content")
                        + ", customerKey: " + to_string(customerKey);
                        + ", customerType: " + to_string(static_cast<int>(customerType));
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = string("Customer is not present/configured")
                    + ", customerKey: " + to_string(customerKey);
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        {
            lastSQLCommand = 
                "insert into CMS_IngestionJobs (IngestionJobKey, CustomerKey, MediaItemKey, MetaDataFileName, MetadataFileContent, IngestionType, StartIngestion, EndIngestion, Status, ErrorMessage) values ("
                "NULL, ?, NULL, ?, ?, ?, NULL, NULL, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, metadataFileName);
            preparedStatement->setString(queryParameterIndex++, metadataFileContent);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(ingestionType));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(ingestionStatus));
            if (errorMessage == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessage);

            preparedStatement->executeUpdate();
        }
        
        ingestionJobKey = getLastInsertId(conn);
                
        _connectionPool->unborrow(conn);

    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return ingestionJobKey;
}

void CMSEngineDBFacade::updateIngestionJob (
    int64_t ingestionJobKey,
    IngestionStatus newIngestionStatus,
    string errorMessage
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        string errorMessageForSQL;
        if (errorMessage == "")
            errorMessageForSQL = errorMessage;
        else
        {
            if (errorMessageForSQL.length() >= 1024)
                errorMessageForSQL.substr(0, 1024);
            else
                errorMessageForSQL = errorMessage;
        }
        
        bool finalState;

        conn = _connectionPool->borrow();	

        if (newIngestionStatus == IngestionStatus::End_IngestionFailure ||
                newIngestionStatus == IngestionStatus::End_IngestionSuccess_EncodingError ||
                newIngestionStatus == IngestionStatus::End_IngestionSuccess)
        {
            finalState			= true;
        }
        else
        {
            finalState          = false;
        }

        if (finalState)
        {
            lastSQLCommand = 
                "update CMS_IngestionJobs set Status = ?, EndIngestion = NOW(), ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newIngestionStatus));
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            preparedStatement->executeUpdate();
        }
        else
        {
            lastSQLCommand = 
                "update CMS_IngestionJobs set Status = ?, ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newIngestionStatus));
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            preparedStatement->executeUpdate();
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
}

void CMSEngineDBFacade::getEncodingJobs(
        bool resetToBeDone,
        string processorCMS,
        vector<shared_ptr<CMSEngineDBFacade::EncodingItem>>& encodingItems
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        conn->_sqlConnection->setAutoCommit(autoCommit);    // or execute the statement START TRANSACTION
        
        if (resetToBeDone)
        {
            lastSQLCommand = 
                "update CMS_EncodingJobs set Status = ?, ProcessorCMS = null where ProcessorCMS = ? and Status <> ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorCMS);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::Processing));

            preparedStatement->executeUpdate();
        }
        else
        {
            int retentionDaysToReset = 7;
            
            lastSQLCommand = 
                "update CMS_EncodingJobs set Status = ?, ProcessorCMS = null where ProcessorCMS = ? and Status = ? and DATE_ADD(EncodingJobStart, INTERVAL ? DAY) <= NOW()";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorCMS);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::Processing));
            preparedStatement->setInt(queryParameterIndex++, retentionDaysToReset);

            preparedStatement->executeUpdate();
        }
        
        {
            lastSQLCommand = 
                "select EncodingJobKey, IngestionJobKey, SourcePhysicalPathKey, EncodingPriority, EncodingProfileKey from CMS_EncodingJobs " 
                "where ProcessorCMS is null and Status = ? and EncodingJobStart <= NOW() "
                "order by EncodingPriority desc, EncodingJobStart asc, FailuresNumber asc for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::ToBeProcessed));

            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatement->executeQuery());
            while (encodingResultSet->next())
            {
                shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<CMSEngineDBFacade::EncodingItem>();
                
                encodingItem->_encodingJobKey = encodingResultSet->getInt64("EncodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("IngestionJobKey");
                encodingItem->_physicalPathKey = encodingResultSet->getInt64("SourcePhysicalPathKey");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(encodingResultSet->getInt("EncodingPriority"));
                encodingItem->_encodingProfileKey = encodingResultSet->getInt64("EncodingProfileKey");
                
                {
                    lastSQLCommand = 
                        "select p.CMSPartitionNumber, p.MediaItemKey, p.FileName, p.RelativePath, ep.Technology "
                        "from CMS_PhysicalPaths p, CMS_EncodingProfiles ep "
                        "where p.EncodingProfileKey = ep.EncodingProfileKey and p.EncodingProfileKey = ? and p.PhysicalPathKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, encodingItem->_encodingProfileKey);
                    preparedStatement->setInt64(queryParameterIndex++, encodingItem->_physicalPathKey);

                    shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatement->executeQuery());
                    if (physicalPathResultSet->next())
                    {
                        encodingItem->_cmsPartitionNumber = physicalPathResultSet->getInt("CMSPartitionNumber");
                        encodingItem->_mediaItemKey = physicalPathResultSet->getInt64("MediaItemKey");
                        encodingItem->_fileName = physicalPathResultSet->getString("FileName");
                        encodingItem->_relativePath = physicalPathResultSet->getString("RelativePath");
                        encodingItem->_encodingProfileTechnology = static_cast<EncodingTechnology>(physicalPathResultSet->getInt("Technology"));
                    }
                    else
                    {
                        string errorMessage = string("select failed")
                                + ", encodingItem->_encodingProfileKey: " + to_string(encodingItem->_encodingProfileKey)
                                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey);
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
                {
                    lastSQLCommand = 
                        "select CustomerKey, ContentType from CMS_MediaItems where MediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, encodingItem->_mediaItemKey);

                    shared_ptr<sql::ResultSet> mediaItemResultSet (preparedStatement->executeQuery());
                    if (mediaItemResultSet->next())
                    {
                        encodingItem->_contentType = static_cast<ContentType>(mediaItemResultSet->getInt("ContentType"));
                        encodingItem->_customer = getCustomer(mediaItemResultSet->getInt64("CustomerKey"));
                    }
                    else
                    {
                        string errorMessage = string("select failed")
                                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey);
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
                
                encodingItems.push_back(encodingItem);
            }
        }

        conn->_sqlConnection->commit();     // or execute COMMIT
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
            conn->_sqlConnection->rollback();     // or execute ROLLBACK
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
}

void CMSEngineDBFacade::updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t ingestionJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        conn->_sqlConnection->setAutoCommit(autoCommit);    // or execute the statement START TRANSACTION
        
        EncodingStatus newEncodingStatus;
        if (encodingError == EncodingError::PunctualError)
        {
            int encodingFailureNumber;
            {
                lastSQLCommand = 
                    "select FailuresNumber from CMS_EncodingJobs where EncodingJobKey = ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingFailureNumber = resultSet->getInt(1);

                }
                else
                {
                    string errorMessage = string("EncodingJob not found")
                            + ", EncodingJobKey: " + to_string(encodingJobKey);
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
            if (encodingFailureNumber + 1 >= _maxEncodingFailures)
                    newEncodingStatus          = EncodingStatus::End_Failed;
            else
                    newEncodingStatus          = EncodingStatus::ToBeProcessed;

            {
                lastSQLCommand = 
                    "update CMS_EncodingJobs set Status = ?, ProcessorCMS = NULL, FailuresNumber = ? where EncodingJobKey = ? and Status = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newEncodingStatus));
                preparedStatement->setInt(queryParameterIndex++, encodingFailureNumber + 1);
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::Processing));

                preparedStatement->executeUpdate();
            }
        }
        else if (encodingError == EncodingError::MaxCapacityReached || encodingError == EncodingError::ErrorBeforeEncoding)
        {
            newEncodingStatus       = EncodingStatus::ToBeProcessed;
            
            lastSQLCommand = 
                "update CMS_EncodingJobs set Status = ?, ProcessorCMS = NULL where EncodingJobKey = ? and Status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newEncodingStatus));
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::Processing));

            preparedStatement->executeUpdate();
        }
        else    // success
        {
            newEncodingStatus       = EncodingStatus::End_ProcessedSuccessful;

            lastSQLCommand = 
                "update CMS_EncodingJobs set Status = ?, ProcessorCMS = NULL, EncodingJobEnd = NOW()     where EncodingJobKey = ? and Status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::Processing));

            preparedStatement->executeUpdate();
        }
        
        if (newEncodingStatus == EncodingStatus::End_ProcessedSuccessful || newEncodingStatus == EncodingStatus::End_Failed)
        {
            lastSQLCommand = 
                "select count(*) from CMS_EncodingJobs where IngestionJobKey = ? and (Status <> ? and Status <> ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::End_ProcessedSuccessful));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::End_Failed));

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                if (resultSet->getInt(1) == 0)  // ingestionJob is finished
                {
                    lastSQLCommand = 
                        "select count(*) from CMS_EncodingJobs where IngestionJobKey = ? and Status == ?)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::End_Failed));

                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                    if (resultSet->next())
                    {
                        IngestionStatus ingestionStatus;
                        
                        if (resultSet->getInt(1) == 0)  // no failures
                            ingestionStatus = IngestionStatus::End_IngestionSuccess;
                        else
                            ingestionStatus = IngestionStatus::End_IngestionSuccess_EncodingError;

                        string errorMessage = "";
                        updateIngestionJob (ingestionJobKey, IngestionStatus::End_IngestionSuccess_EncodingError, errorMessage);
                    }
                    else
                    {
                        string errorMessage = string("count(*) failure")
                                + ", IngestionJobKey: " + to_string(encodingJobKey);
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
            }
            else
            {
                string errorMessage = string("EncodingJob not found")
                        + ", EncodingJobKey: " + to_string(encodingJobKey);
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        conn->_sqlConnection->commit();     // or execute COMMIT
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
            conn->_sqlConnection->rollback();     // or execute ROLLBACK
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
}

string CMSEngineDBFacade::checkCustomerMaxIngestionNumber (
    int64_t customerKey
)
{
    string      relativePathToBeUsed;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        int maxIngestionsNumber;
        int currentIngestionsNumber;
        int encodingPeriod;
        string periodStartDateTime;
        string periodEndDateTime;
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select c.MaxIngestionsNumber, cmi.CurrentIngestionsNumber, c.EncodingPeriod, " 
                    "DATE_FORMAT(cmi.StartDateTime, '%Y-%m-%d %H:%i:%s') as LocalStartDateTime, DATE_FORMAT(cmi.EndDateTime, '%Y-%m-%d %H:%i:%s') as LocalEndDateTime, "
                    "cmi.CurrentDirLevel1, cmi.CurrentDirLevel2, cmi.CurrentDirLevel3 "
                "from CMS_Customers c, CMS_CustomerMoreInfo cmi where c.CustomerKey = cmi.CustomerKey and c.CustomerKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                maxIngestionsNumber = resultSet->getInt("MaxIngestionsNumber");
                currentIngestionsNumber = resultSet->getInt("CurrentIngestionsNumber");
                encodingPeriod = resultSet->getInt("EncodingPeriod");
                periodStartDateTime = resultSet->getString("LocalStartDateTime");
                periodEndDateTime = resultSet->getString("LocalEndDateTime");                
                currentDirLevel1 = resultSet->getInt("CurrentDirLevel1");
                currentDirLevel2 = resultSet->getInt("CurrentDirLevel2");
                currentDirLevel3 = resultSet->getInt("CurrentDirLevel3");
            }
            else
            {
                string errorMessage = string("Customer is not present/configured")
                    + ", customerKey: " + to_string(customerKey);
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        bool ingestionsAllowed = true;
        bool periodExpired = false;
        char newPeriodStartDateTime [64];
        char newPeriodEndDateTime [64];

        {
            char                strDateTimeNow [64];
            tm                  tmDateTimeNow;
            chrono::system_clock::time_point now = chrono::system_clock::now();
            time_t utcTimeNow = chrono::system_clock::to_time_t(now);
            localtime_r (&utcTimeNow, &tmDateTimeNow);

            sprintf (strDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d",
                tmDateTimeNow. tm_year + 1900,
                tmDateTimeNow. tm_mon + 1,
                tmDateTimeNow. tm_mday,
                tmDateTimeNow. tm_hour,
                tmDateTimeNow. tm_min,
                tmDateTimeNow. tm_sec);

            if (periodStartDateTime.compare(strDateTimeNow) <= 0 && periodEndDateTime.compare(strDateTimeNow) >= 0)
            {
                // Period not expired

                // periodExpired = false; already initialized
                
                if (currentIngestionsNumber >= maxIngestionsNumber)
                {
                    // no more ingestions are allowed for this customer

                    ingestionsAllowed = false;
                }
            }
            else
            {
                // Period expired

                periodExpired = true;
                
                if (encodingPeriod == static_cast<int>(EncodingPeriod::Daily))
                {
                    sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            0,  // tmDateTimeNow. tm_hour,
                            0,  // tmDateTimeNow. tm_min,
                            0  // tmDateTimeNow. tm_sec
                    );
                    sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            23,  // tmCurrentDateTime. tm_hour,
                            59,  // tmCurrentDateTime. tm_min,
                            59  // tmCurrentDateTime. tm_sec
                    );
                }
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Weekly))
                {
                    // from monday to sunday
                    // monday
                    {
                        int daysToHavePreviousMonday;

                        if (tmDateTimeNow.tm_wday == 0)  // Sunday
                            daysToHavePreviousMonday = 6;
                        else
                            daysToHavePreviousMonday = tmDateTimeNow.tm_wday - 1;

                        chrono::system_clock::time_point mondayOfCurrentWeek;
                        if (daysToHavePreviousMonday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHavePreviousMonday);
                            mondayOfCurrentWeek = now - days;
                        }
                        else
                            mondayOfCurrentWeek = now;

                        tm                  tmMondayOfCurrentWeek;
                        time_t utcTimeMondayOfCurrentWeek = chrono::system_clock::to_time_t(mondayOfCurrentWeek);
                        localtime_r (&utcTimeMondayOfCurrentWeek, &tmMondayOfCurrentWeek);

                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmMondayOfCurrentWeek. tm_year + 1900,
                                tmMondayOfCurrentWeek. tm_mon + 1,
                                tmMondayOfCurrentWeek. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // sunday
                    {
                        int daysToHaveNextSunday;

                        daysToHaveNextSunday = 7 - tmDateTimeNow.tm_wday;

                        chrono::system_clock::time_point sundayOfCurrentWeek;
                        if (daysToHaveNextSunday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHaveNextSunday);
                            sundayOfCurrentWeek = now + days;
                        }
                        else
                            sundayOfCurrentWeek = now;

                        tm                  tmSundayOfCurrentWeek;
                        time_t utcTimeSundayOfCurrentWeek = chrono::system_clock::to_time_t(sundayOfCurrentWeek);
                        localtime_r (&utcTimeSundayOfCurrentWeek, &tmSundayOfCurrentWeek);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmSundayOfCurrentWeek. tm_year + 1900,
                                tmSundayOfCurrentWeek. tm_mon + 1,
                                tmSundayOfCurrentWeek. tm_mday,
                                23,  // tmSundayOfCurrentWeek. tm_hour,
                                59,  // tmSundayOfCurrentWeek. tm_min,
                                59  // tmSundayOfCurrentWeek. tm_sec
                        );
                    }
                }
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Monthly))
                {
                    // first day of the month
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        tm                  tmLastDayOfCurrentMonth = tmDateTimeNow;

                        tmLastDayOfCurrentMonth.tm_mday = 1;

                        // Next month 0=Jan
                        if (tmLastDayOfCurrentMonth.tm_mon == 11)    // Dec
                        {
                            tmLastDayOfCurrentMonth.tm_mon = 0;
                            tmLastDayOfCurrentMonth.tm_year++;
                        }
                        else
                        {
                            tmLastDayOfCurrentMonth.tm_mon++;
                        }

                        // Get the first day of the next month
                        time_t utcTimeLastDayOfCurrentMonth = mktime (&tmLastDayOfCurrentMonth);

                        // Subtract 1 day
                        utcTimeLastDayOfCurrentMonth -= 86400;

                        // Convert back to date and time
                        localtime_r (&utcTimeLastDayOfCurrentMonth, &tmLastDayOfCurrentMonth);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmLastDayOfCurrentMonth. tm_year + 1900,
                                tmLastDayOfCurrentMonth. tm_mon + 1,
                                tmLastDayOfCurrentMonth. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
                else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
                {
                    // first day of the year
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                1,  // tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                12, // tmDateTimeNow. tm_mon + 1,
                                31, // tmDateTimeNow. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
            }
        }
        
        if (periodExpired)
        {
            lastSQLCommand = 
                "update CMS_CustomerMoreInfo set CurrentIngestionsNumber = 0, StartDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), EndDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S') where CustomerKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, newPeriodStartDateTime);
            preparedStatement->setString(queryParameterIndex++, newPeriodEndDateTime);
            preparedStatement->setInt64(queryParameterIndex++, customerKey);

            preparedStatement->executeUpdate();
        }
        
        _connectionPool->unborrow(conn);

        if (!ingestionsAllowed)
        {
            string errorMessage = string("Reached the max number of Ingestions in your period")
                + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
                + ", encodingPeriod: " + to_string(static_cast<int>(encodingPeriod))
            ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
        
        {
            char pCurrentRelativePath [64];
            
            sprintf (pCurrentRelativePath, "/%03d/%03d/%03d/", 
                currentDirLevel1, currentDirLevel2, currentDirLevel3);
            
            relativePathToBeUsed = pCurrentRelativePath;
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    
    return relativePathToBeUsed;
}

int64_t CMSEngineDBFacade::saveIngestedContentMetadata(
        shared_ptr<Customer> customer,
        int64_t ingestionJobKey,
        Json::Value metadataRoot,
        string relativePath,
        int cmsPartitionIndexUsed,
        int sizeInBytes,
        int64_t videoOrAudioDurationInMilliSeconds,
        int imageWidth,
        int imageHeight
)
{
    int64_t         mediaItemKey;
    int64_t         physicalPathKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        conn->_sqlConnection->setAutoCommit(autoCommit);    // or execute the statement START TRANSACTION
        
        Json::Value contentIngestion = metadataRoot["ContentIngestion"]; 

        // get ContentProviderKey
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
            if (isMetadataPresent(contentIngestion, "ContentProviderName"))
                contentProviderName = contentIngestion.get("ContentProviderName", "XXX").asString();
            else
                contentProviderName = _defaultContentProviderName;

            lastSQLCommand = 
                "select ContentProviderKey from CMS_ContentProviders where CustomerKey = ? and Name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setString(queryParameterIndex++, contentProviderName);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentProviderKey = resultSet->getInt64("ContentProviderKey");
            }
            else
            {
                string errorMessage = string("ContentProvider is not present")
                    + ", customer->_customerKey: " + to_string(customer->_customerKey)
                    + ", contentProviderName: " + contentProviderName
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        ContentType contentType;
        int64_t encodingProfileSetKey;
        {
            string title = "";
            string subTitle = "";
            string ingester = "";
            string keywords = "";
            string description = "";
            string sContentType;
            string logicalType = "";
            string encodingProfilesSet;

            title = contentIngestion.get("Title", "XXX").asString();
            
            if (isMetadataPresent(contentIngestion, "SubTitle"))
                subTitle = contentIngestion.get("SubTitle", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Ingester"))
                ingester = contentIngestion.get("Ingester", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Keywords"))
                keywords = contentIngestion.get("Keywords", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Description"))
                description = contentIngestion.get("Description", "XXX").asString();

            sContentType = contentIngestion.get("ContentType", "XXX").asString();
            if (sContentType == "video")
                contentType = ContentType::Video;
            else if (sContentType == "audio")
                contentType = ContentType::Audio;
            else if (sContentType == "image")
                contentType = ContentType::Image;
            else
            {
                string errorMessage = string("ContentType is wrong")
                    + ", sContentType: " + sContentType
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            

            if (isMetadataPresent(contentIngestion, "LogicalType"))
                logicalType = contentIngestion.get("LogicalType", "XXX").asString();

            {
                encodingProfilesSet = contentIngestion.get("EncodingProfilesSet", "XXX").asString();
                if (encodingProfilesSet == "systemDefault")
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                }
                else if (encodingProfilesSet == "customerDefault")
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                }
                else
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name = ?";
                }
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                if (encodingProfilesSet == "systemDefault")
                {
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                }
                else if (encodingProfilesSet == "customerDefault")
                {
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                }
                else
                {
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                    preparedStatement->setString(queryParameterIndex++, encodingProfilesSet);
                }
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingProfileSetKey = resultSet->getInt64("EncodingProfilesSetKey");
                }
                else
                {
                    string errorMessage = string("EncodingProfilesSetKey is not present")
                        + ", contentType: " + to_string(static_cast<int>(contentType))
                        + ", customer->_customerKey: " + to_string(customer->_customerKey)
                        + ", encodingProfilesSet: " + encodingProfilesSet
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }


            lastSQLCommand = 
                "insert into CMS_MediaItems (MediaItemKey, CustomerKey, ContentProviderKey, GenreKey, Title, SubTitle, Ingester, Keywords, Description, " 
                "IngestionDate, ContentType, LogicalType, EncodingProfilesSetKey) values ("
                "NULL, ?, ?, NULL, ?, ?, ?, ?, ?, NULL, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setInt64(queryParameterIndex++, contentProviderKey);
            preparedStatement->setString(queryParameterIndex++, title);
            if (subTitle == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, subTitle);
            if (ingester == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, ingester);
            if (keywords == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, keywords);
            if (description == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, description);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
            if (logicalType == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, logicalType);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileSetKey);

            preparedStatement->executeUpdate();
        }
        
        mediaItemKey = getLastInsertId(conn);

        {
            if (contentType == ContentType::Video)
            {
                lastSQLCommand = 
                    "insert into CMS_VideoItems (MediaItemKey, DurationInMilliSeconds) values ("
                    "?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                if (videoOrAudioDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, videoOrAudioDurationInMilliSeconds);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Audio)
            {
                lastSQLCommand = 
                    "insert into CMS_AudioItems (MediaItemKey, DurationInMilliSeconds) values ("
                    "?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                if (videoOrAudioDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, videoOrAudioDurationInMilliSeconds);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Image)
            {
                lastSQLCommand = 
                    "insert into CMS_ImageItems (MediaItemKey, Width, Height) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setInt64(queryParameterIndex++, imageWidth);
                preparedStatement->setInt64(queryParameterIndex++, imageHeight);

                preparedStatement->executeUpdate();
            }
            else
            {
                string errorMessage = string("ContentType is wrong")
                    + ", contentType: " + to_string(static_cast<int>(contentType))
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }        

        {
            string sourceFileName = "";
            int drm = 0;

            sourceFileName = contentIngestion.get("SourceFileName", "XXX").asString();

            lastSQLCommand = 
                "insert into CMS_PhysicalPaths(PhysicalPathKey, MediaItemKey, DRM, FileName, RelativePath, CMSPartitionNumber, SizeInBytes, EncodingProfileKey, CreationDate) values ("
		"NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, sourceFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, cmsPartitionIndexUsed);
            preparedStatement->setInt(queryParameterIndex++, sizeInBytes);
            preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);

            preparedStatement->executeUpdate();
        }

        physicalPathKey = getLastInsertId(conn);

        // territories
        {
            string field = "Territories";
            if (isMetadataPresent(contentIngestion, field))
            {
                const Json::Value territories = contentIngestion[field];
                
                lastSQLCommand = 
                    "select TerritoryKey, Name from CMS_Territories where CustomerKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                while (resultSet->next())
                {
                    int64_t territoryKey = resultSet->getInt64("TerritoryKey");
                    string territoryName = resultSet->getString("Name");

                    string startPublishing = "NOW";
                    string endPublishing = "FOREVER";
                    if (isMetadataPresent(territories, territoryName))
                    {
                        Json::Value territory = territories[territoryName];
                        
                        field = "startPublishing";
                        if (isMetadataPresent(territory, field))
                            startPublishing = territory.get(field, "XXX").asString();

                        field = "endPublishing";
                        if (isMetadataPresent(territory, field))
                            endPublishing = territory.get(field, "XXX").asString();
                    }
                    
                    if (startPublishing == "NOW")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point now = chrono::system_clock::now();
                        time_t utcTime = chrono::system_clock::to_time_t(now);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        startPublishing = strDateTime;
                    }
                    
                    if (endPublishing == "FOREVER")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 20);
                        
                        time_t utcTime = chrono::system_clock::to_time_t(forever);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        endPublishing = strDateTime;
                    }
                    
                    {
                        lastSQLCommand = 
                            "insert into CMS_DefaultTerritoryInfo(DefaultTerritoryInfoKey, MediaItemKey, TerritoryKey, StartPublishing, EndPublishing) values ("
                            "NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";

                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatement->setInt(queryParameterIndex++, territoryKey);
                        preparedStatement->setString(queryParameterIndex++, startPublishing);
                        preparedStatement->setString(queryParameterIndex++, endPublishing);

                        preparedStatement->executeUpdate();
                    }
                }                
            }
        }
        
        {
            int currentDirLevel1;
            int currentDirLevel2;
            int currentDirLevel3;

            {
                lastSQLCommand = 
                    "select CurrentDirLevel1, CurrentDirLevel2, CurrentDirLevel3 "
                    "from CMS_CustomerMoreInfo where CustomerKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    currentDirLevel1 = resultSet->getInt("CurrentDirLevel1");
                    currentDirLevel2 = resultSet->getInt("CurrentDirLevel2");
                    currentDirLevel3 = resultSet->getInt("CurrentDirLevel3");
                }
                else
                {
                    string errorMessage = string("Customer is not present/configured")
                        + ", customer->_customerKey: " + to_string(customer->_customerKey);
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
                lastSQLCommand = 
                    "update CMS_CustomerMoreInfo set CurrentDirLevel1 = ?, CurrentDirLevel2 = ?, CurrentDirLevel3 = ?, CurrentIngestionsNumber = CurrentIngestionsNumber + 1 where CustomerKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel1);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel2);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel3);
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);

                preparedStatement->executeUpdate();
            }
        }
        
        {
            EncodingPriority encodingPriority;
            string field = "EncodingPriority";
            if (isMetadataPresent(contentIngestion, field))
            {
                string strEncodingPriority = contentIngestion.get(field, "XXX").asString();
                if (strEncodingPriority == "low")
                {
                   encodingPriority = EncodingPriority::Low; 
                }
                else if (strEncodingPriority == "default")
                {
                   encodingPriority = EncodingPriority::Default;
                }
                else if (strEncodingPriority == "high")
                {
                   encodingPriority = EncodingPriority::High; 
                }
                else
                {
                    string errorMessage = string("Field 'EncodingPriority' is wrong")
                            + ", EncodingPriority: " + to_string(static_cast<int>(encodingPriority));
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                if (static_cast<int>(encodingPriority) > customer->_maxEncodingPriority)
                    encodingPriority = static_cast<EncodingPriority>(customer->_maxEncodingPriority); 
            }
            else
                encodingPriority = static_cast<EncodingPriority>(customer->_maxEncodingPriority);

            lastSQLCommand = 
                "insert into CMS_EncodingJobs(EncodingJobKey, IngestionJobKey, SourcePhysicalPathKey, EncodingPriority, EncodingProfileKey, EncodingJobStart, EncodingJobEnd, Status, ProcessorCMS, FailuresNumber)"
	        " select                      NULL,           ?,               ?,                     ?,                 EncodingProfileKey, NULL,             NULL,           ?,      NULL,         0 from CMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPriority));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingStatus::ToBeProcessed));
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileSetKey);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                "update CMS_IngestionJobs set MediaItemKey = ?, Status = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(IngestionStatus::QueuedForEncoding));
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            preparedStatement->executeUpdate();
        }

        conn->_sqlConnection->commit();     // or execute COMMIT
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
            conn->_sqlConnection->rollback();     // or execute ROLLBACK
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return mediaItemKey;
}

int64_t CMSEngineDBFacade::saveEncodedContentMetadata(
        int64_t customerKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int cmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey
)
{
    int64_t     encodedPhysicalPathKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        conn->_sqlConnection->setAutoCommit(autoCommit);    // or execute the statement START TRANSACTION
        
        {
            int drm = 0;

            lastSQLCommand = 
                "insert into CMS_PhysicalPaths(PhysicalPathKey, MediaItemKey, DRM, FileName, RelativePath, CMSPartitionNumber, SizeInBytes, EncodingProfileKey, CreationDate) values ("
        	"NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, encodedFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, cmsPartitionIndexUsed);
            preparedStatement->setInt(queryParameterIndex++, sizeInBytes);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            preparedStatement->executeUpdate();
        }

        encodedPhysicalPathKey = getLastInsertId(conn);

        // publishing territories
        {
            lastSQLCommand = 
                "select t.TerritoryKey, t.Name, DATE_FORMAT(d.StartPublishing, '%Y-%m-%d %H:%i:%s') as StartPublishing, DATE_FORMAT(d.EndPublishing, '%Y-%m-%d %H:%i:%s') as EndPublishing from CMS_Territories t, CMS_DefaultTerritoryInfo d "
                "where t.TerritoryKey = d.TerritoryKey and t.CustomerKey = ? and d.MediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                int64_t territoryKey = resultSet->getInt64("TerritoryKey");
                string territoryName = resultSet->getString("Name");
                string startPublishing = resultSet->getString("StartPublishing");
                string endPublishing = resultSet->getString("EndPublishing");
                
                lastSQLCommand = 
                    "select PublishingStatus from CMS_Publishing2 where MediaItemKey = ? and TerritoryKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setInt64(queryParameterIndex++, territoryKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    int publishingStatus = resultSet->getInt("PublishingStatus");
                    
                    if (publishingStatus == 1)
                    {
                        lastSQLCommand = 
                            "update CMS_Publishing2 set PublishingStatus = 0 where MediaItemKey = ? and TerritoryKey = ?";

                        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatement->setInt(queryParameterIndex++, territoryKey);

                        preparedStatement->executeUpdate();
                    }
                }
                else
                {
                    lastSQLCommand = 
                        "insert into CMS_Publishing2 (PublishingKey, MediaItemKey, TerritoryKey, StartPublishing, EndPublishing, PublishingStatus) values ("
	        	"NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), 0)";

                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                    preparedStatement->setInt(queryParameterIndex++, territoryKey);
                    preparedStatement->setString(queryParameterIndex++, startPublishing);
                    preparedStatement->setString(queryParameterIndex++, endPublishing);

                    preparedStatement->executeUpdate();
                }
            }                
        }

        conn->_sqlConnection->commit();     // or execute COMMIT
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
            conn->_sqlConnection->rollback();     // or execute ROLLBACK
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return encodedPhysicalPathKey;
}

bool CMSEngineDBFacade::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
}

int64_t CMSEngineDBFacade::getLastInsertId(shared_ptr<MySQLConnection> conn)
{
    int64_t         lastInsertId;
    
    string      lastSQLCommand;

    try
    {
        lastSQLCommand = 
            "select LAST_INSERT_ID()";
        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

        if (resultSet->next())
        {
            lastInsertId = resultSet->getInt64(1);
        }
        else
        {
            string error ("select LAST_INSERT_ID failed");
            
            _logger->error(error);
            
            throw runtime_error(error);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    
    return lastInsertId;
}

void CMSEngineDBFacade::createTablesIfNeeded()
{
    string      lastSQLCommand;

    try
    {
        shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

        shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());

        try
        {
            // MaxEncodingPriority (0: low, 1: default, 2: high)
            // CustomerType: (0: Live Sessions only, 1: Ingestion + Delivery, 2: Encoding Only)
            // EncodingPeriod: 0: Daily, 1: Weekly, 2: Monthly

            lastSQLCommand = 
                "create table if not exists CMS_Customers ("
                    "CustomerKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CreationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "Name                           VARCHAR (64) NOT NULL,"
                    "DirectoryName                  VARCHAR (64) NOT NULL,"
                    "Street                         VARCHAR (128) NULL,"
                    "City                           VARCHAR (64) NULL,"
                    "State                          VARCHAR (64) NULL,"
                    "ZIP                            VARCHAR (32) NULL,"
                    "Phone                          VARCHAR (32) NULL,"
                    "CountryCode                    VARCHAR (64) NULL,"
                    "CustomerType                   TINYINT NOT NULL,"
                    "DeliveryURL                    VARCHAR (256) NULL,"
                    "IsEnabled                      TINYINT (1) NOT NULL,"
                    "MaxEncodingPriority            TINYINT NOT NULL,"
                    "EncodingPeriod                 TINYINT NOT NULL,"
                    "MaxIngestionsNumber            INT NOT NULL,"
                    "MaxStorageInGB                 INT NOT NULL,"
                    "CurrentStorageUsageInGB        INT DEFAULT 0,"
                    "SuperDeliveryRights            INT DEFAULT 0,"
                    "LanguageCode                   VARCHAR (16) NOT NULL,"
                    "constraint CMS_Customers_PK PRIMARY KEY (CustomerKey),"
                    "UNIQUE (Name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create unique index CMS_Customers_idx on CMS_Customers (DirectoryName)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // The territories are present only if the Customer is a 'Content Provider'.
            // In this case we could have two scenarios:
            // - customer not having territories (we will have just one row in this table with Name set as 'default')
            // - customer having at least one territory (we will as much rows in this table according the number of territories)
            lastSQLCommand = 
                "create table if not exists CMS_Territories ("
                    "TerritoryKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey  				BIGINT UNSIGNED NOT NULL,"
                    "Name					VARCHAR (64) NOT NULL,"
                    "Currency					VARCHAR (16) DEFAULT NULL,"
                    "constraint CMS_Territories_PK PRIMARY KEY (TerritoryKey),"
                    "constraint CMS_Territories_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (CustomerKey, Name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // create table CMS_CustomerMoreInfo. This table was created to move the fields
            //		that are updated during the ingestion from CMS_Customers.
            //		That will avoid to put a lock in the CMS_Customers during the update
            //		since the CMS_Customers is a wide used table
            lastSQLCommand = 
                "create table if not exists CMS_CustomerMoreInfo ("
                    "CustomerKey  			BIGINT UNSIGNED NOT NULL,"
                    "CurrentDirLevel1			INT NOT NULL,"
                    "CurrentDirLevel2			INT NOT NULL,"
                    "CurrentDirLevel3			INT NOT NULL,"
                    "StartDateTime			DATETIME NOT NULL,"
                    "EndDateTime			DATETIME NOT NULL,"
                    "CurrentIngestionsNumber	INT NOT NULL,"
                    "constraint CMS_CustomerMoreInfo_PK PRIMARY KEY (CustomerKey), "
                    "constraint CMS_CustomerMoreInfo_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // create table CMS_Users2
            // Type (bits: ...9876543210)
            //      bit 0: CMSAdministrator
            //      bin 1: CMSUser
            //      bit 2: EndUser
            //      bit 3: CMSEditorialUser
            //      bit 4: BillingAdministrator
            lastSQLCommand = 
                "create table if not exists CMS_Users2 ("
                    "UserKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "UserName				VARCHAR (128) NOT NULL,"
                    "Password				VARCHAR (128) NOT NULL,"
                    "CustomerKey                            BIGINT UNSIGNED NOT NULL,"
                    "Type					INT NOT NULL,"
                    "EMailAddress				VARCHAR (128) NULL,"
                    "CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ExpirationDate				DATETIME NOT NULL,"
                    "constraint CMS_Users2_PK PRIMARY KEY (UserKey), "
                    "constraint CMS_Users2_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // this index is important because UserName is used by the EndUser to login
            //  (i.e.: it is the end-user email address) and we cannot have
            //  two equal UserName since we will not be able to understand the CustomerKey
            lastSQLCommand = 
                "create unique index CMS_Users2_idx on CMS_Users2 (CustomerKey, UserName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists CMS_ContentProviders ("
                    "ContentProviderKey                     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey                            BIGINT UNSIGNED NOT NULL,"
                    "Name					VARCHAR (64) NOT NULL,"
                    "constraint CMS_ContentProviders_PK PRIMARY KEY (ContentProviderKey), "
                    "constraint CMS_ContentProviders_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (CustomerKey, Name))" 
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // Technology.
            //      0: Images (Download),
            //      1: 3GPP (Streaming+Download),
            //      2: MPEG2-TS (IPhone Streaming),
            //      3: WEBM (VP8 and Vorbis)
            //      4: WindowsMedia,
            //      5: Adobe
            // The encoding profile format is:
            //      [<video enc. params>][<audio enc. params>][<image enc. params>][<ringtone enc. params>][<application enc. params>][file format params]
            //  The <video enc. params> format is:
            //      _V1<video_codec>_V21<video_bitrate1>_<frame_rate_fps>_..._V2M<video_bitrateM>_<frame_rate_fps>_V3<screen_size>_V5<key_frame_interval_in_seconds>_V6<profile>
            //      <video_codec>: mpeg4, h263, h264, VP8, wmv
            //      <video_bitrate1>: 68000, 70000
            //      <screen_size>: QCIF: 176x144, SQCIF: 128x96, QSIF: 160x120, QVGA: 320x240, HVGA: 480x320, 480x360, CIF: 352x288, VGA: 640x480, SD: 720x576
            //      <frame_rate_fps>: 15, 12.5, 10, 8
            //      <key_frame_interval_in_seconds>: 3
            //      <profile>: baseline, main, high
            //  The <audio enc. params> format is:
            //      _A1<audio_codec>_A21<audio_bitrate1>_..._A2N<audio_bitrateN>_A3<Channels>_A4<sampling_rate>_
            //      <audio_codec>: amr-nb, amr-wb, aac-lc, enh-aacplus, he-aac, wma, vorbis
            //      <audio_bitrate1>: 12200
            //      <Channels>: S: Stereo, M: Mono
            //      <sampling_rate>: 8000, 11025, 32000, 44100, 48000
            //  The <image enc. params> format is:
            //	_I1<image_format>_I2<width>_I3<height>_I4<AspectRatio>_I5<Interlace>
            //	<image_format>: PNG, JPG, GIF
            //	<AspectRatio>: 1, 0
            //	<Interlace>: 0: NoInterlace, 1: LineInterlace, 2: PlaneInterlace, 3: PartitionInterlace
            //  The <ringtone enc. params> format is:
            //	...
            //  The <application enc. params> format is:
            //	...
            //  The <file format params> format is:
            //	_F1<hinter>
            //	<hinter>: 1, 0
            //  Temporary fields used by XHP: Width, Height, VideoCodec, AudioCodec
            lastSQLCommand = 
                "create table if not exists CMS_EncodingProfiles ("
                    "EncodingProfileKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ContentType			TINYINT NOT NULL,"
                    "Technology			TINYINT NOT NULL,"
                    "Description			VARCHAR (256) NOT NULL,"
                    "Label				VARCHAR (64) NULL,"
                    "Width				INT NOT NULL DEFAULT 0,"
                    "Height				INT NOT NULL DEFAULT 0,"
                    "VideoCodec			VARCHAR (32) null,"
                    "AudioCodec			VARCHAR (32) null,"
                    "constraint CMS_EncodingProfiles_PK PRIMARY KEY (EncodingProfileKey), "
                    "UNIQUE (ContentType, Technology, Description)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // CustomerKey and Name. If they are NULL, it means the EncodingProfileSet is the default one
            //      for the specified ContentType.
            // A customer can have custom EncodingProfilesSet specifying together CustomerKey and Name.
            lastSQLCommand = 
                "create table if not exists CMS_EncodingProfilesSet ("
                    "EncodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ContentType				TINYINT NOT NULL,"
                    "CustomerKey  				BIGINT UNSIGNED NULL,"
                    "Name						VARCHAR (64) NULL,"
                    "constraint CMS_EncodingProfilesSet_PK PRIMARY KEY (EncodingProfilesSetKey)," 
                    "constraint CMS_EncodingProfilesSet_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (ContentType, CustomerKey, Name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            //  insert global EncodingProfilesSet per ContentType
            vector<ContentType> contentTypes = { ContentType::Video, ContentType::Audio, ContentType::Image };
            
            for (ContentType contentType: contentTypes)
            {
                int     encodingProfilesSetCount = -1;
                {
                    lastSQLCommand = 
                        "select count(*) from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is NULL and Name is NULL";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));

                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                    if (resultSet->next())
                    {
                        encodingProfilesSetCount = resultSet->getInt(1);
                    }
                }

                if (encodingProfilesSetCount == 0)
                {
                    lastSQLCommand = 
                        "insert into CMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (NULL, ?, NULL, NULL)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt(queryParameterIndex++, static_cast<int>(contentType));
                    preparedStatement->executeUpdate();
                }
            }
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    
        
        try
        {
            // create table EncodingProfilesSetMapping
            lastSQLCommand = 
                "create table if not exists CMS_EncodingProfilesSetMapping ("
                    "EncodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "constraint CMS_EncodingProfilesSetMapping_PK PRIMARY KEY (EncodingProfilesSetKey, EncodingProfileKey), "
                    "constraint CMS_EncodingProfilesSetMapping_FK1 foreign key (EncodingProfilesSetKey) "
                        "references CMS_EncodingProfilesSet (EncodingProfilesSetKey), "
                    "constraint CMS_EncodingProfilesSetMapping_FK2 foreign key (EncodingProfileKey) "
                        "references CMS_EncodingProfiles (EncodingProfileKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // Status.
            //  1: StartIingestion
            //  2: MetaDataSavedInDB
            //  3: MediaFileMovedInCMS
            //  8: End_IngestionFailure  # nothing done
            //  9: End_IngestionSrcSuccess_EncodingError (we will have this state if just one of the encoding failed.
            //      One encoding is considered a failure only after that the MaxFailuresNumer for this encoding is reached)
            //  10: End_IngestionSuccess  # all done
            // So, at the beginning the status is 1
            //      from 1 it could become 2 or 8 in case of failure
            //      from 2 it could become 3 or 8 in case of failure
            //      from 3 it could become 10 or 9 in case of encoding failure
            //      The final states are 8, 9 and 10
            // IngestionType could be:
            //      NULL: XML not parsed yet to know the type
            //      0: insert
            //      1: update
            //      2. remove
            // MetaDataFileName could be null to implement the following scenario done through XHP GUI:
            //      allow the user to extend the content specifying a new encoding profile to be used
            //      for a content that was already ingested previously. In this scenario we do not have
            //      any meta data file.
            lastSQLCommand = 
                "create table if not exists CMS_IngestionJobs ("
                    "IngestionJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey			BIGINT UNSIGNED NOT NULL,"
                    "MediaItemKey			BIGINT UNSIGNED NULL,"
                    "MetaDataFileName                   VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "MetaDataFileContent		TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "IngestionType                      TINYINT (2) NOT NULL,"
                    "StartIngestion			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "EndIngestion			DATETIME NULL,"
                    "Status           			TINYINT (2) NOT NULL,"
                    "ErrorMessage			VARCHAR (1024) NULL,"
                    "constraint CMS_IngestionJobs_PK PRIMARY KEY (IngestionJobKey), "
                    "constraint CMS_IngestionJobs_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists CMS_Genres ("
                    "GenreKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "Name				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "constraint CMS_Genres_PK PRIMARY KEY (GenreKey), "
                    "UNIQUE (Name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // CustomerKey is the owner of the content
            // ContentType: 0: video, 1: audio, 2: image, 3: application, 4: ringtone (it uses the same audio tables),
            //		5: playlist, 6: live
            // IngestedRelativePath MUST start always with '/' and ends always with '/'
            // IngestedFileName and IngestedRelativePath refer the ingested content independently
            //		if it is encoded or uncompressed
            // if EncodingProfilesSet is NULL, it means the ingested content is already encoded
            // The ContentProviderKey is the entity owner of the content. For example H3G is our customer and EMI is the ContentProvider.
            lastSQLCommand = 
                "create table if not exists CMS_MediaItems ("
                    "MediaItemKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey			BIGINT UNSIGNED NOT NULL,"
                    "ContentProviderKey			BIGINT UNSIGNED NOT NULL,"
                    "GenreKey				BIGINT UNSIGNED NULL,"
                    "Title      			VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "SubTitle				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "Ingester				VARCHAR (128) NULL,"
                    "Keywords				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "Description			TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "IngestionDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ContentType                        TINYINT NOT NULL,"
                    "LogicalType			VARCHAR (32) NULL,"
                    "EncodingProfilesSetKey		BIGINT UNSIGNED NULL,"
                    "constraint CMS_MediaItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint CMS_MediaItems_FK foreign key (CustomerKey) "
                        "references CMS_Customers (CustomerKey) on delete cascade, "
                    "constraint CMS_MediaItems_FK2 foreign key (ContentProviderKey) "
                        "references CMS_ContentProviders (ContentProviderKey), "
                    "constraint CMS_MediaItems_FK3 foreign key (EncodingProfilesSetKey) "
                        "references CMS_EncodingProfilesSet (EncodingProfilesSetKey), "
                    "constraint CMS_MediaItems_FK4 foreign key (GenreKey) "
                        "references CMS_Genres (GenreKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index CMS_MediaItems_idx2 on CMS_MediaItems (ContentType, LogicalType, IngestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index CMS_MediaItems_idx3 on CMS_MediaItems (ContentType, IngestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index CMS_MediaItems_idx4 on CMS_MediaItems (ContentType, Title)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // DRM. 0: NO DRM, 1: YES DRM
            // EncodedFileName and EncodedRelativePath are NULL only if the content is un-compressed.
            //  EncodedRelativePath MUST start always with '/' and ends always with '/'
            // EncodingProfileKey will be NULL only in case of
            //      - an un-compressed video or audio
            //      - an Application
            // CMSPartitionNumber. -1: live partition, >= 0: partition for any other content
            // IsAlias (0: false): it is used for a PhysicalPath that is an alias and
            //  it really refers another existing PhysicalPath. It was introduced to manage the XLE live profile
            //  supporting really multi profiles: rtsp, hls, adobe. So for every different profiles, we will
            //  create just an alias
            lastSQLCommand = 
                "create table if not exists CMS_PhysicalPaths ("
                    "PhysicalPathKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DRM	             		TINYINT NOT NULL,"
                    "FileName				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "RelativePath			VARCHAR (256) NOT NULL,"
                    "CMSPartitionNumber			INT NULL,"
                    "SizeInBytes			BIGINT UNSIGNED NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NULL,"
                    "IsAlias				INT NOT NULL DEFAULT 0,"
                    "CreationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint CMS_PhysicalPaths_PK PRIMARY KEY (PhysicalPathKey), "
                    "constraint CMS_PhysicalPaths_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint CMS_PhysicalPaths_FK2 foreign key (EncodingProfileKey) "
                        "references CMS_EncodingProfiles (EncodingProfileKey), "
                    "UNIQUE (MediaItemKey, RelativePath, FileName, IsAlias), "
                    "UNIQUE (MediaItemKey, EncodingProfileKey)) "	// it is not possible to have the same content using the same encoding profile key
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index CMS_PhysicalPaths_idx2 on CMS_PhysicalPaths (MediaItemKey, PhysicalPathKey, EncodingProfileKey, CMSPartitionNumber)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index CMS_PhysicalPaths_idx3 on CMS_PhysicalPaths (RelativePath, FileName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists CMS_VideoItems ("
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DurationInMilliSeconds		BIGINT NULL,"
                    "constraint CMS_VideoItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint CMS_VideoItems_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists CMS_AudioItems ("
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DurationInMilliSeconds		BIGINT NULL,"
                    "constraint CMS_AudioItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint CMS_AudioItems_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists CMS_ImageItems ("
                    "MediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "Width						INT NOT NULL,"
                    "Height						INT NOT NULL,"
                    "constraint CMS_ImageItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint CMS_ImageItems_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
            lastSQLCommand = 
                "create table if not exists CMS_DefaultTerritoryInfo ("
                    "DefaultTerritoryInfoKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "TerritoryKey				BIGINT UNSIGNED NOT NULL,"
                    /*
                    "DownloadChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "DownloadChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "DownloadReserveCredit			TINYINT (1) NULL,"
                    "DownloadExternalBillingName		VARCHAR (64) NULL,"
                    "DownloadMaxRetries				INT NOT NULL,"
                    "DownloadTTLInSeconds			INT NOT NULL,"
                    "StreamingChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "StreamingChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "StreamingReserveCredit			TINYINT (1) NULL,"
                    "StreamingExternalBillingName		VARCHAR (64) NULL,"
                    "StreamingMaxRetries			INT NOT NULL,"
                    "StreamingTTLInSeconds			INT NOT NULL,"
                    */
                    "StartPublishing				DATETIME NOT NULL,"
                    "EndPublishing				DATETIME NOT NULL,"
                    "constraint CMS_DefaultTerritoryInfo_PK PRIMARY KEY (DefaultTerritoryInfoKey), "
                    "constraint CMS_DefaultTerritoryInfo_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint CMS_DefaultTerritoryInfo_FK2 foreign key (TerritoryKey) "
                        "references CMS_Territories (TerritoryKey) on delete cascade, "
                    /*
                    "constraint CMS_DefaultTerritoryInfo_FK3 foreign key (DownloadChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint CMS_DefaultTerritoryInfo_FK4 foreign key (DownloadChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint CMS_DefaultTerritoryInfo_FK5 foreign key (StreamingChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint CMS_DefaultTerritoryInfo_FK6 foreign key (StreamingChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                     */
                    "UNIQUE (MediaItemKey, TerritoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In CMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create table if not exists CMS_Publishing2 ("
                    "PublishingKey                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey                   BIGINT UNSIGNED NOT NULL,"
                    "TerritoryKey                   BIGINT UNSIGNED NOT NULL,"
                    "StartPublishing                DATETIME NOT NULL,"
                    "EndPublishing                  DATETIME NOT NULL,"
                    "PublishingStatus               TINYINT (1) NOT NULL,"
                    "ProcessorCMS                   VARCHAR (24) NULL,"
                    "constraint CMS_Publishing2_PK PRIMARY KEY (PublishingKey), "
                    "constraint CMS_Publishing2_FK foreign key (MediaItemKey) "
                        "references CMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint CMS_Publishing2_FK2 foreign key (TerritoryKey) "
                        "references CMS_Territories (TerritoryKey) on delete cascade, "
                    "UNIQUE (MediaItemKey, TerritoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In CMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index CMS_Publishing2_idx2 on CMS_Publishing2 (MediaItemKey, StartPublishing, EndPublishing, PublishingStatus)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In CMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index CMS_Publishing2_idx3 on CMS_Publishing2 (PublishingStatus, StartPublishing, EndPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In CMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index CMS_Publishing2_idx4 on CMS_Publishing2 (PublishingStatus, EndPublishing, StartPublishing)";            
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // The CMS_EncodingJobs table include all the contents that have to be encoded
            //  OriginatingProcedure.
            //      0: ContentIngestion1_0
            //          Used fields: FileName, RelativePath, CustomerKey, PhysicalPathKey, EncodingProfileKey
            //          The other fields will be NULL
            //      1: Encoding1_0
            //          Used fields: FileName, RelativePath, CustomerKey, FTPIPAddress (optional), FTPPort (optional),
            //              FTPUser (optional), FTPPassword (optional), EncodingProfileKey
            //          The other fields will be NULL
            //  RelativePath: it is the relative path of the original uncompressed file name
            //  PhysicalPathKey: it is the physical path key of the original uncompressed file name
            //  The ContentType was added just to avoid a big join to retrieve this info
            //  ProcessorCMS is the CMSEngine processing the encoding
            //  Status.
            //      0: TOBEPROCESSED
            //      1: PROCESSING
            //      2: SUCCESS (PROCESSED)
            //      3: FAILED
            //  EncodingPriority:
            //      0: low
            //      1: default
            //      2: high
            lastSQLCommand = 
                "create table if not exists CMS_EncodingJobs ("
                    "EncodingJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "IngestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "SourcePhysicalPathKey		BIGINT UNSIGNED NULL,"
                    "EncodingPriority			TINYINT NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "EncodingJobStart			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "EncodingJobEnd			DATETIME NULL,"
                    "Status           			TINYINT (2) NOT NULL,"
                    "ProcessorCMS			VARCHAR (24) NULL,"
                    "FailuresNumber           	INT NOT NULL,"
                    "constraint CMS_EncodingJobs_PK PRIMARY KEY (EncodingJobKey), "
                    "constraint CMS_EncodingJobs_FK foreign key (IngestionJobKey) "
                        "references CMS_IngestionJobs (IngestionJobKey) on delete cascade, "
                    "constraint CMS_EncodingJobs_FK3 foreign key (SourcePhysicalPathKey) "
                    // on delete cascade is necessary because when the ingestion fails, it is important that the 'removeMediaItemMetaData'
                    //      remove the rows from this table too, otherwise we will be flooded by the errors: PartitionNumber is null
                    // The consequence is that when the PhysicalPath is removed in general, also the rows from this table will be removed
                        "references CMS_PhysicalPaths (PhysicalPathKey) on delete cascade, "
                    "constraint CMS_EncodingJobs_FK4 foreign key (EncodingProfileKey) "
                        "references CMS_EncodingProfiles (EncodingProfileKey) on delete cascade, "
                    "UNIQUE (SourcePhysicalPathKey, EncodingProfileKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important because it will be used by the query looking every 15 seconds if there are
            // contents to be encoded
            lastSQLCommand = 
                "create index CMS_EncodingJobs_idx2 on CMS_EncodingJobs (Status, ProcessorCMS, FailuresNumber, EncodingJobStart)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(string("SQL exception")
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
    
    
        /*
    # create table CMS_HTTPSessions
    # One session is per UserKey and UserAgent
    create table if not exists CMS_HTTPSessions (
            HTTPSessionKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            UserKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ExpirationDate				DATETIME NOT NULL,
            constraint CMS_HTTPSessions_PK PRIMARY KEY (HTTPSessionKey), 
            constraint CMS_HTTPSessions_FK foreign key (UserKey) 
                    references CMS_Users2 (UserKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_HTTPSessions_idx on CMS_HTTPSessions (UserKey, UserAgent);

    # create table CMS_ReportsConfiguration
    # Type. 0: Billing Statistics, 1: Content Access, 2: Active Users,
    #		3: Streaming Statistics, 4: Usage (how to call the one in XHP today?)
    # Period. 0: Hourly, 1: Daily, 2: Weekly, 3: Monthly, 4: Yearly
    # Format. 0: CSV, 1: HTML
    # EmailAddresses. List of email addresses separated by ‘;’
    create table if not exists CMS_ReportsConfiguration (
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey             	BIGINT UNSIGNED NOT NULL,
            Type						INT NOT NULL,
            Period						INT NOT NULL,
            TimeOfDay					INT NOT NULL,
            Format						INT NOT NULL,
            EmailAddresses				VARCHAR (1024) NULL,
            constraint CMS_ReportsConfiguration_PK PRIMARY KEY (ReportConfigurationKey), 
            constraint CMS_ReportsConfiguration_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            UNIQUE (CustomerKey, Type, Period)) 
            ENGINE=InnoDB;

    # create table CMS_ReportURLCategory
    create table if not exists CMS_ReportURLCategory (
            ReportURLCategoryKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name             			VARCHAR (128) NOT NULL,
            URLsPattern				VARCHAR (512) NOT NULL,
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL,
            constraint CMS_ReportURLCategory_PK PRIMARY KEY (ReportURLCategoryKey), 
            constraint CMS_ReportURLCategory_FK foreign key (ReportConfigurationKey) 
                    references CMS_ReportsConfiguration (ReportConfigurationKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_CustomersSharable
    create table if not exists CMS_CustomersSharable (
            CustomerKeyOwner			BIGINT UNSIGNED NOT NULL,
            CustomerKeySharable		BIGINT UNSIGNED NOT NULL,
            constraint CMS_CustomersSharable_PK PRIMARY KEY (CustomerKeyOwner, CustomerKeySharable), 
            constraint CMS_CustomersSharable_FK1 foreign key (CustomerKeyOwner) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_CustomersSharable_FK2 foreign key (CustomerKeySharable) 
                    references CMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table Handsets
    # It represent a family of handsets
    # Description is something like: +H.264, +enh-aac-plus, ...
    # FamilyType: 0: Delivery, 1: Music/Presentation (used by CMS Application Images)
    # SupportedDelivery: see above the definition for iSupportedDelivery_*
    create table if not exists CMS_HandsetsFamilies (
            HandsetFamilyKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Description				VARCHAR (128) NOT NULL,
            FamilyType					INT NOT NULL,
            SupportedDelivery			INT NOT NULL DEFAULT 3,
            SmallSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            MediumSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            SmallIconProfileKey		BIGINT UNSIGNED NULL,
            MediumIconProfileKey		BIGINT UNSIGNED NULL,
            LargeIconProfileKey		BIGINT UNSIGNED NULL,
            constraint CMS_HandsetsFamilies_PK PRIMARY KEY (HandsetFamilyKey)) 
            ENGINE=InnoDB;

    # create table Handsets
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    # HTTPRedirectionForRTSP. 0 if supported, 0 if not supported
    # DRMMethod. 0: no DRM, 1: oma1forwardlock, 2: cfm, 3: cfm+
    # If HandsetFamilyKey is NULL, it means the handset is not connected to his family
    # SupportedNetworkCoverage. NULL: no specified, 0: 2G, 1: 2.5G, 2: 3G
    create table if not exists CMS_Handsets (
            HandsetKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetFamilyKey			BIGINT UNSIGNED NULL,
            MusicHandsetFamilyKey		BIGINT UNSIGNED NULL,
            Brand						VARCHAR (32) NOT NULL,
            Model						VARCHAR (32) NOT NULL,
            Alias						VARCHAR (32) NULL,
            OperativeSystem			VARCHAR (32) NOT NULL,
            HTTPRedirectionForRTSP		TINYINT (1) NOT NULL,
            DRMMethod					TINYINT NOT NULL,
            ScreenWidth				INT NOT NULL,
            ScreenHeight				INT NOT NULL,
            ScreenDensity				INT NULL,
            SupportedNetworkCoverage	INT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint CMS_Handsets_PK PRIMARY KEY (HandsetKey), 
            constraint CMS_Handsets_FK foreign key (HandsetFamilyKey) 
                    references CMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            constraint CMS_Handsets_FK2 foreign key (MusicHandsetFamilyKey) 
                    references CMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            UNIQUE (Brand, Model)) 
            ENGINE=InnoDB;

    # create table UserAgents
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    create table if not exists CMS_UserAgents (
            UserAgentKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint CMS_UserAgents_PK PRIMARY KEY (UserAgentKey), 
            constraint CMS_UserAgents_FK foreign key (HandsetKey) 
                    references CMS_Handsets (HandsetKey) on delete cascade, 
            UNIQUE (UserAgent)) 
            ENGINE=InnoDB;

    # create table HandsetsProfilesMapping
    # This table perform a mapping between (HandsetKey, NetworkCoverage) with EncodingProfileKey and a specified Priority
    # NetworkCoverage: 0: 2G, 1: 2.5G, 2: 3G.
    # If CustomerKey is NULL, it means the mapping is the default mapping
    # Priority: 1 (the best), 2, ...
    create table if not exists CMS_HandsetsProfilesMapping (
            HandsetProfileMappingKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey  				BIGINT UNSIGNED NULL,
            ContentType                TINYINT NOT NULL,
            HandsetFamilyKey			BIGINT UNSIGNED NOT NULL,
            NetworkCoverage			TINYINT NOT NULL,
            EncodingProfileKey			BIGINT UNSIGNED NOT NULL,
            Priority					INT NOT NULL,
            constraint CMS_HandsetsProfilesMapping_PK primary key (HandsetProfileMappingKey), 
            constraint CMS_HandsetsProfilesMapping_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_HandsetsProfilesMapping_FK2 foreign key (HandsetFamilyKey) 
                    references CMS_HandsetsFamilies (HandsetFamilyKey) on delete cascade, 
            constraint CMS_HandsetsProfilesMapping_FK3 foreign key (EncodingProfileKey) 
                    references CMS_EncodingProfiles (EncodingProfileKey), 
            UNIQUE (CustomerKey, ContentType, HandsetFamilyKey, NetworkCoverage, EncodingProfileKey, Priority)) 
            ENGINE=InnoDB;


    # create table CMS_GenresTranslation
    create table if not exists CMS_GenresTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint CMS_GenresTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint CMS_GenresTranslation_FK foreign key (GenreKey) 
                    references CMS_Genres (GenreKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_GenresTranslation_idx on CMS_GenresTranslation (GenreKey, Field, LanguageCode);


    # create table CMS_MediaItemsTranslation
    create table if not exists CMS_MediaItemsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint CMS_MediaItemsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint CMS_MediaItemsTranslation_FK foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_MediaItemsTranslation_idx on CMS_MediaItemsTranslation (MediaItemKey, Field, LanguageCode);

    # create table CMS_GenresMediaItemsMapping
    create table if not exists CMS_GenresMediaItemsMapping (
            GenresMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            constraint CMS_GenresMediaItemsMapping_PK PRIMARY KEY (GenresMediaItemsMappingKey), 
            constraint CMS_GenresMediaItemsMapping_FK foreign key (GenreKey) 
                    references CMS_Genres (GenreKey) on delete cascade, 
            constraint CMS_GenresMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (GenreKey, MediaItemKey))
            ENGINE=InnoDB;

    # create table CMS_MediaItemsCustomerMapping
    # CustomerType could be 0 (Owner of the content) or 1 (User of the shared content)
    # CMS_MediaItemsCustomerMapping table will contain one row for the Customer Ownerof the content and one row for each shared content
    create table if not exists CMS_MediaItemsCustomerMapping (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            CustomerType				TINYINT NOT NULL,
            constraint CMS_MediaItemsCustomerMapping_PK PRIMARY KEY (MediaItemKey, CustomerKey), 
            constraint CMS_MediaItemsCustomerMapping_FK1 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_MediaItemsCustomerMapping_FK2 foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table CMS_ExternalKeyMapping
    # CustomerKey is the owner of the content
    create table if not exists CMS_ExternalKeyMapping (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ExternalKey				VARCHAR (64) NOT NULL,
            constraint CMS_ExternalKeyMapping_PK PRIMARY KEY (CustomerKey, MediaItemKey), 
            constraint CMS_ExternalKeyMapping_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_ExternalKeyMapping_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_ExternalKeyMapping_idx on CMS_ExternalKeyMapping (CustomerKey, ExternalKey);

    # create table MediaItemsRemoved
    create table if not exists CMS_MediaItemsRemoved (
            MediaItemRemovedKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            DeletionDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            MediaItemKey  				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            ContentProviderKey			BIGINT UNSIGNED NOT NULL,
            DisplayName				VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Ingester					VARCHAR (128) NULL,
            Description				TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Country					VARCHAR (32) NULL,
            IngestionDate				TIMESTAMP,
            ContentType                TINYINT NOT NULL,
            LogicalType				VARCHAR (32) NULL,
            IngestedFileName			VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            IngestedRelativePath		VARCHAR (256) NULL, 
            constraint CMS_MediaItemsRemoved_PK PRIMARY KEY (MediaItemRemovedKey)) 
            ENGINE=InnoDB;

    # create table CMS_CrossReferences
    # This table will be used to set cross references between MidiaItems
    # Type could be:
    #	0: <not specified>
    #	1: IsScreenshotOfVideo
    #	2: IsImageOfAlbum
    create table if not exists CMS_CrossReferences (
            SourceMediaItemKey		BIGINT UNSIGNED NOT NULL,
            Type					TINYINT NOT NULL,
            TargetMediaItemKey		BIGINT UNSIGNED NOT NULL,
            constraint CMS_CrossReferences_PK PRIMARY KEY (SourceMediaItemKey, TargetMediaItemKey), 
            constraint CMS_CrossReferences_FK1 foreign key (SourceMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_CrossReferences_FK2 foreign key (TargetMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create index CMS_CrossReferences_idx on CMS_CrossReferences (SourceMediaItemKey, TargetMediaItemKey);

    # create table CMS_3SWESubscriptions
    # This table will be used to set cross references between MidiaItems
    create table if not exists CMS_3SWESubscriptions (
            ThreeSWESubscriptionKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name						VARCHAR (64) NOT NULL,
            constraint CMS_3SWESubscriptions_PK PRIMARY KEY (ThreeSWESubscriptionKey), 
            UNIQUE (Name)) 
            ENGINE=InnoDB;

    # create table CMS_3SWESubscriptionsMapping
    # This table will be used to specified the contents to be added in an HTML presentation
    create table if not exists CMS_3SWESubscriptionsMapping (
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ThreeSWESubscriptionKey	BIGINT UNSIGNED NOT NULL,
            constraint CMS_3SWESubscriptionsMapping_PK PRIMARY KEY (MediaItemKey, ThreeSWESubscriptionKey), 
            constraint CMS_3SWESubscriptionsMapping_FK1 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_3SWESubscriptionsMapping_FK2 foreign key (ThreeSWESubscriptionKey) 
                    references CMS_3SWESubscriptions (ThreeSWESubscriptionKey) on delete cascade) 
            ENGINE=InnoDB;
    create index CMS_3SWESubscriptionsMapping_idx on CMS_3SWESubscriptionsMapping (MediaItemKey, ThreeSWESubscriptionKey);

    # create table CMS_3SWEMoreChargingInfo
    # This table include the information included into the Billing definition and
    # missing in ChargingInfo table
    create table if not exists CMS_3SWEMoreChargingInfo (
            ChargingKey				BIGINT UNSIGNED NOT NULL,
            AssetType					VARCHAR (16) NOT NULL,
            AmountTax					INT NOT NULL,
            PartnerID					VARCHAR (32) NOT NULL,
            Category					VARCHAR (32) NOT NULL,
            RetailAmount				INT NOT NULL,
            RetailAmountTax			INT NOT NULL,
            RetailAmountWithSub		INT NOT NULL,
            RetailAmountTaxWithSub		INT NOT NULL,
            constraint CMS_3SWEMoreChargingInfo_PK PRIMARY KEY (ChargingKey), 
            constraint CMS_3SWEMoreChargingInfo_FK foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_Advertisements
    # TerritoryKey: if NULL the ads is valid for any territory
    # Type:
    #		0: pre-roll
    #		1: post-roll
    create table if not exists CMS_Advertisements (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NULL,
            Name						VARCHAR (32) NOT NULL,
            ContentType				TINYINT NOT NULL,
            IsEnabled	                TINYINT (1) NOT NULL,
            Type						TINYINT NOT NULL,
            ValidityStart				TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            ValidityEnd				TIMESTAMP NOT NULL,
            constraint CMS_Advertisements_PK PRIMARY KEY (AdvertisementKey), 
            constraint CMS_Advertisements_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_Advertisements_FK2 foreign key (TerritoryKey) 
                    references CMS_Territories (TerritoryKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_Advertisements_idx on CMS_Advertisements (CustomerKey, TerritoryKey, Name);

    # create table CMS_AdvertisementAdvertisings
    create table if not exists CMS_Advertisement_Ads (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint CMS_Advertisement_Ads_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint CMS_Advertisement_Ads_FK foreign key (AdvertisementKey) 
                    references CMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint CMS_Advertisement_Ads_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_AdvertisementContents
    create table if not exists CMS_Advertisement_Contents (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint CMS_Advertisement_Contents_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint CMS_Advertisement_Contents_FK foreign key (AdvertisementKey) 
                    references CMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint CMS_Advertisement_Contents_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table CMS_RequestsAuthorization
    # MediaItemKey or ExternalKey cannot be both null
    # DeliveryMethod:
    #		0: download
    #		1: 3gpp streaming
    #		2: RTMP Flash Streaming
    #		3: WindowsMedia Streaming
    # SwitchingType: 0: None, 1: FCS, 2: FTS
    # NetworkCoverage. 0: 2G, 1: 2.5G, 2: 3G
    # IngestedPathName: [<live prefix>]/<customer name>/<territory name>/<relative path>/<content name>
    # ToBeContinued. 0 or 1
    # ForceHTTPRedirection. 0: HTML page, 1: HTTP Redirection
    # TimeToLive is measured in seconds
    create table if not exists CMS_RequestsAuthorization (
            RequestAuthorizationKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            PlayerIP					VARCHAR (16) NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            Shuffling					TINYINT NULL,
            PartyID					VARCHAR (64) NOT NULL,
            MSISDN						VARCHAR (32) NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            ExternalKey				VARCHAR (64) NULL,
            EncodingProfileKey			BIGINT UNSIGNED NULL,
            EncodingLabel				VARCHAR (64) NULL,
            LanguageCode				VARCHAR (16) NULL,
            DeliveryMethod				TINYINT NULL,
            PreviewSeconds				INT NULL,
            SwitchingType				TINYINT NOT NULL default 0,
            ChargingKey				BIGINT UNSIGNED NULL,
            XSBTTLInSeconds			INT NULL,
            XSBMaxRetries				INT NULL,
            Sequence					VARCHAR (16) NULL,
            ToBeContinued				TINYINT NULL,
            ForceHTTPRedirection		TINYINT NULL,
            NetworkCoverage			TINYINT NULL,
            AuthorizationTimestamp		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            TimeToLive					INT NOT NULL,
            AuthorizationType			VARCHAR (64) NULL,
            AuthorizationData			VARCHAR (128) NULL,
            constraint CMS_RequestsAuthorization_PK PRIMARY KEY (RequestAuthorizationKey), 
            constraint CMS_RequestsAuthorization_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_RequestsAuthorization_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_RequestsAuthorization_FK3 foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_RequestsStatistics
    # Status:
    #	- 0: Received
    #	- 1: Failed (final status)
    #	- 2: redirected (final status)
    create table if not exists CMS_RequestsStatistics (
            RequestStatisticKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Status           			TINYINT (2) NOT NULL,
            ErrorMessage				VARCHAR (512) NULL,
            RedirectionURL				VARCHAR (256) NULL,
            RequestAuthorizationKey	BIGINT UNSIGNED NULL,
            CustomerKey				BIGINT UNSIGNED NULL,
            UserAgent					VARCHAR (512) NULL,
            HandsetKey					BIGINT UNSIGNED NULL,
            PartyID					VARCHAR (64) NULL,
            MSISDN						VARCHAR (32) NULL,
            PhysicalPathKey			BIGINT UNSIGNED NULL,
            AuthorizationKey			BIGINT UNSIGNED NULL,
            RequestReceivedTimestamp	TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ElapsedSecondsToManageTheRequest	INT NULL,
            constraint CMS_RequestsStatistics_PK PRIMARY KEY (RequestStatisticKey)) 
            ENGINE=InnoDB;
    create index CMS_RequestsStatistics_idx2 on CMS_RequestsStatistics (AuthorizationKey);



    # create table CMS_MediaItemsPublishing
    # In this table it is considered the publishing 'per content'.
    # In CMS_MediaItemsPublishing, a content is considered published if all his profiles are published.
    create table if not exists CMS_MediaItemsPublishing (
            MediaItemPublishingKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            constraint CMS_MediaItemsPublishing_PK PRIMARY KEY (TerritoryKey, MediaItemKey), 
            constraint CMS_MediaItemsPublishing_FK1 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_MediaItemsPublishing_FK2 foreign key (TerritoryKey) 
                    references CMS_Territories (TerritoryKey) on delete cascade)
            ENGINE=InnoDB;

    // Done by a Zoli music SQL script:
    //ALTER TABLE CMS_MediaItemsPublishing 
    //	ADD COLUMN AccessCount INT NOT NULL DEFAULT 0,
    //	ADD COLUMN Popularity DECIMAL(12, 2) NOT NULL DEFAULT 0,
    //	ADD COLUMN LastAccess DATETIME NOT NULL DEFAULT 0;
    //ALTER TABLE CMS_MediaItemsPublishing 
    //	ADD KEY idx_AccessCount (TerritoryKey, AccessCount),
    //	ADD KEY idx_Popularity (TerritoryKey, Popularity),
    //	ADD KEY idx_LastAccess (TerritoryKey, LastAccess);


    # create table MediaItemsBillingInfo
    # Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
    create table if not exists CMS_MediaItemsBillingInfo (
            MediaItemsBillingInfoKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            ChargingKey1				BIGINT UNSIGNED NOT NULL,
            ChargingKey2				BIGINT UNSIGNED NOT NULL,
            ReserveCredit				TINYINT (1) NULL,
            ExternalBillingName		VARCHAR (64) NULL,
            MaxRetries					INT NOT NULL,
            TTLInSeconds				INT NOT NULL,
            constraint CMS_MediaItemsBillingInfo_PK PRIMARY KEY (MediaItemsBillingInfoKey), 
            constraint CMS_MediaItemsBillingInfo_FK foreign key (PhysicalPathKey) 
                    references CMS_PhysicalPaths (PhysicalPathKey) on delete cascade, 
            constraint CMS_MediaItemsBillingInfo_FK2 foreign key (TerritoryKey) 
                    references CMS_Territories (TerritoryKey) on delete cascade, 
            constraint CMS_MediaItemsBillingInfo_FK3 foreign key (ChargingKey1) 
                    references ChargingInfo (ChargingKey), 
            constraint CMS_MediaItemsBillingInfo_FK4 foreign key (ChargingKey2) 
                    references ChargingInfo (ChargingKey), 
            UNIQUE (PhysicalPathKey, DeliveryMethod, TerritoryKey)) 
            ENGINE=InnoDB;


    # create table CMS_AllowedDeliveryMethods
    create table if not exists CMS_AllowedDeliveryMethods (
            ContentType				TINYINT NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            constraint CMS_AllowedDeliveryMethods_PK PRIMARY KEY (ContentType, DeliveryMethod)) 
            ENGINE=InnoDB;


    # create table CMS_Artists
    create table if not exists CMS_Artists (
            ArtistKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name						VARCHAR (255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Country					VARCHAR (128) NULL,
            HomePageURL				VARCHAR (256) NULL,
            constraint CMS_Artists_PK PRIMARY KEY (ArtistKey), 
            UNIQUE (Name)) 
            ENGINE=InnoDB;


    # create table CMS_ArtistsTranslation
    create table if not exists CMS_ArtistsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint CMS_ArtistsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint CMS_ArtistsTranslation_FK foreign key (ArtistKey) 
                    references CMS_Artists (ArtistKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_ArtistsTranslation_idx on CMS_ArtistsTranslation (ArtistKey, Field, LanguageCode);

    # create table CMS_CustomerCatalogMoreInfo
    # GlobalHomePage: 0 or 1 (it specifies if his home page has to be the global one or his private home page)
    # IsPublic: 0 or 1
    create table if not exists CMS_CustomerCatalogMoreInfo (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            GlobalHomePage				INT NOT NULL,
            IsPublic					INT NOT NULL,
            CatalogImageMediaItemKey	BIGINT UNSIGNED NULL,
            HeaderIsEnabled_0					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_0		INT UNSIGNED NULL,
            HeaderFunctionParameters_0			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_0			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_1					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_1		INT UNSIGNED NULL,
            HeaderFunctionParameters_1			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_2					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_2		INT UNSIGNED NULL,
            HeaderFunctionParameters_2			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_3					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_3		INT UNSIGNED NULL,
            HeaderFunctionParameters_3			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_4					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_4		INT UNSIGNED NULL,
            HeaderFunctionParameters_4			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            constraint CMS_CustomerCatalogMoreInfo_PK PRIMARY KEY (CustomerKey), 
            constraint CMS_CustomerCatalogMoreInfo_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade, 
            constraint CMS_CustomerCatalogMoreInfo_FK2 foreign key (CatalogImageMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_PresentationWorkspaces
    # Name: if NULL, it is the Production Workspace
    create table if not exists CMS_PresentationWorkspaces (
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            Name						VARCHAR (128) NULL,
            constraint CMS_PresentationWorkspaces_PK PRIMARY KEY (PresentationWorkspaceKey), 
            constraint CMS_PresentationWorkspaces_FK foreign key (CustomerKey) 
                    references CMS_Customers (CustomerKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_PresentationWorkspaces_idx on CMS_PresentationWorkspaces (CustomerKey, Name);

    # create table CMS_PresentationItems
    # PresentationItemType: see PresentationItemType in CMSClient.h
    # NodeType:
    #	0: internal (no root type)
    #	1: MainRoot
    #	2: Root_1
    #	3: Root_2
    #	4: Root_3
    #	5: Root_4
    create table if not exists CMS_PresentationItems (
            PresentationItemKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL,
            ParentPresentationItemKey	BIGINT UNSIGNED NULL,
            NodeType					INT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            PresentationItemType		INT UNSIGNED NOT NULL,
            ImageMediaItemKey			BIGINT UNSIGNED NULL,
            Title						VARCHAR (128) NULL,
            SubTitle					VARCHAR (256) NULL,
            Parameter					VARCHAR (256) NULL,
            PositionIndex				INT UNSIGNED NOT NULL,
            ToolbarIsEnabled_1					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_1		INT UNSIGNED NULL,
            ToolbarFunctionParameters_1			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_2					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_2		INT UNSIGNED NULL,
            ToolbarFunctionParameters_2			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_3					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_3		INT UNSIGNED NULL,
            ToolbarFunctionParameters_3			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_4					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_4		INT UNSIGNED NULL,
            ToolbarFunctionParameters_4			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_5					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_5		INT UNSIGNED NULL,
            ToolbarFunctionParameters_5			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_5			BIGINT UNSIGNED NULL,
            constraint CMS_PresentationItems_PK PRIMARY KEY (PresentationItemKey), 
            constraint CMS_PresentationItems_FK foreign key (PresentationWorkspaceKey) 
                    references CMS_PresentationWorkspaces (PresentationWorkspaceKey) on delete cascade, 
            constraint CMS_PresentationItems_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_PresentationItems_FK3 foreign key (ImageMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_PresentationItems_idx on CMS_PresentationItems (ParentPresentationItemKey, PositionIndex);

    # create table CMS_Albums
    # AlbumKey: it is the PlaylistMediaItemKey
    create table if not exists CMS_Albums (
            AlbumKey  					BIGINT UNSIGNED NOT NULL,
            Version					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            UPC						VARCHAR (32) NOT NULL,
            Type						VARCHAR (32) NULL,
            ReleaseDate				DATETIME NULL,
            Title						VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            LabelName					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Supplier					VARCHAR (64) NULL,
            SubSupplier				VARCHAR (64) NULL,
            PCopyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Copyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Explicit					VARCHAR (16) NULL,
            constraint CMS_Albums_PK PRIMARY KEY (AlbumKey), 
            constraint CMS_Albums_FK foreign key (AlbumKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_Albums_idx2 on CMS_Albums (Supplier, UPC);

    # create table CMS_ArtistsMediaItemsMapping
    # Role:
    #	- 'NOROLE' if not present in the XML
    #	- 'MAINARTIST' if compareToIgnoreCase(main artist)
    #	- 'FEATURINGARTIST' if compareToIgnoreCase(featuring artist)
    #	- any other string in upper case without any space
    create table if not exists CMS_ArtistsMediaItemsMapping (
            ArtistsMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            Role							VARCHAR (128) NOT NULL,
            constraint CMS_ArtistsMediaItemsMapping_PK PRIMARY KEY (ArtistsMediaItemsMappingKey), 
            constraint CMS_ArtistsMediaItemsMapping_FK foreign key (ArtistKey) 
                    references CMS_Artists (ArtistKey) on delete cascade, 
            constraint CMS_ArtistsMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (ArtistKey, MediaItemKey, Role))
            ENGINE=InnoDB;
    create index CMS_ArtistsMediaItemsMapping_idx on CMS_ArtistsMediaItemsMapping (ArtistKey, Role, MediaItemKey);
    create index CMS_ArtistsMediaItemsMapping_idx2 on CMS_ArtistsMediaItemsMapping (MediaItemKey, Role);

    # create table CMS_ISRCMapping
    # SourceISRC: VideoItem or Ringtone -----> TargetISRC: AudioItem (Track)
    create table if not exists CMS_ISRCMapping (
            SourceISRC					VARCHAR (32) NOT NULL,
            TargetISRC					VARCHAR (32) NOT NULL,
            constraint CMS_ISRCMapping_PK PRIMARY KEY (SourceISRC, TargetISRC)) 
            ENGINE=InnoDB;
    create unique index CMS_ISRCMapping_idx on CMS_ISRCMapping (TargetISRC, SourceISRC);


    # create table CMS_SubTitlesTranslation
    create table if not exists CMS_SubTitlesTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint CMS_SubTitlesTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint CMS_SubTitlesTranslation_FK foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index CMS_SubTitlesTranslation_idx on CMS_SubTitlesTranslation (MediaItemKey, Field, LanguageCode);


    # create table ApplicationItems
    create table if not exists CMS_ApplicationItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            constraint CMS_ApplicationItems_PK PRIMARY KEY (MediaItemKey), 
            constraint CMS_ApplicationItems_FK foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_ApplicationHandsetsMapping
    create table if not exists CMS_ApplicationHandsetsMapping (
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            JadFile					TEXT NULL,
            constraint CMS_ApplicationHandsetsMapping_PK PRIMARY KEY (PhysicalPathKey, HandsetKey), 
            constraint CMS_ApplicationHandsetsMapping_FK foreign key (PhysicalPathKey) 
                    references CMS_PhysicalPaths (PhysicalPathKey) on delete cascade, 
            constraint CMS_ApplicationHandsetsMapping_FK2 foreign key (HandsetKey) 
                    references CMS_Handsets (HandsetKey)) 
            ENGINE=InnoDB;
    create index CMS_ApplicationHandsetsMapping_idx on CMS_ApplicationHandsetsMapping (PhysicalPathKey, HandsetKey);

    # create table PlaylistItems
    # ClipType. 0 (iContentType_Video): video, 1 (iContentType_Audio): audio
    # ScheduledStartTime: used only in case of Linear Playlist (playlist of clips plus the scheduled_start_time field)
    create table if not exists CMS_PlaylistItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ClipType					INT NOT NULL,
            ScheduledStartTime			DATETIME NULL,
            constraint CMS_PlaylistItems_PK PRIMARY KEY (MediaItemKey), 
            constraint CMS_PlaylistItems_FK foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table CMS_PlaylistClips
    # if ClipMediaItemKey is null it means the playlist item is a live and the LiveChannelURL will be initialized
    # LiveType. NULL: ClipMediaItemKey is initialized, 0: live from XAC/XLE, 1: live from the SDP file
    # ClipDuration: duration of the clip in milliseconds (NULL in case of live)
    # Seekable: 0 or 1 (NULL in case of live)
    create table if not exists CMS_PlaylistClips (
            PlaylistClipKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PlaylistMediaItemKey		BIGINT UNSIGNED NOT NULL,
            ClipMediaItemKey			BIGINT UNSIGNED NULL,
            ClipSequence           	INT NOT NULL,
            ClipDuration           	BIGINT NULL,
            Seekable           		TINYINT NULL,
            WaitingProfileSince		DATETIME NULL,
            constraint CMS_PlaylistClips_PK PRIMARY KEY (PlaylistClipKey), 
            constraint CMS_PlaylistClips_FK foreign key (PlaylistMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint CMS_PlaylistClips_FK2 foreign key (ClipMediaItemKey) 
                    references CMS_MediaItems (MediaItemKey)) 
            ENGINE=InnoDB;
    create unique index CMS_PlaylistClips_idx2 on CMS_PlaylistClips (PlaylistMediaItemKey, ClipSequence);

    # create table LiveItems
    # FeedType. 0: video, 1: audio
    create table if not exists CMS_LiveItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            FeedType					INT NOT NULL,
            constraint CMS_LiveItems_PK PRIMARY KEY (MediaItemKey), 
            constraint CMS_LiveItems_FK foreign key (MediaItemKey) 
                    references CMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table MimeTypes
    # HandsetBrandPattern, HandsetModelPattern and HandsetOperativeSystem must be all different from null or all equal to null
    #		we cannot have i.e. HandsetBrandPattern == null and HandsetModelPattern != null
    create table if not exists CMS_MimeTypes (
            MimeTypeKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MimeType					VARCHAR (64) NOT NULL,
            ContentType                TINYINT NOT NULL,
            HandsetBrandPattern		VARCHAR (32) NULL,
            HandsetModelPattern		VARCHAR (32) NULL,
            HandsetOperativeSystem		VARCHAR (32) NULL,
            EncodingProfileNamePattern	VARCHAR (64) NOT NULL,
            Description             	VARCHAR (64) NULL,
            constraint CMS_MimeTypes_PK PRIMARY KEY (MimeTypeKey)) 
            ENGINE=InnoDB;
         */

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _logger->error(string("SQL exception")
            + ", lastSQLCommand: " + lastSQLCommand
            + ", se.what(): " + se.what()
        );
    }    
}

bool CMSEngineDBFacade::isRealDBError(string exceptionMessage)
{        
    transform(
            exceptionMessage.begin(), 
            exceptionMessage.end(), 
            exceptionMessage.begin(), 
            [](unsigned char c){return tolower(c); } );

    if (exceptionMessage.find("already exists") == string::npos &&            // error (table) on mysql
            exceptionMessage.find("duplicate key name") == string::npos    // error (index) on mysql
            )
    {
        return true;
    }
    else
    {
        return false;
    }
}
