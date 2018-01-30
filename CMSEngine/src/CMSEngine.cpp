/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMSEngine.cpp
 * Author: multi
 * 
 * Created on January 30, 2018, 3:00 PM
 */

#include "CMSEngine.h"


CMSEngine::CMSEngine(shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade) 
{
    _cmsEngineDBFacade = cmsEngineDBFacade;
}

CMSEngine::CMSEngine(const CMSEngine& orig) {
}

CMSEngine::~CMSEngine() {
}

void CMSEngine::addCustomer(
        string sCreationDate, 
        string sDefaultUserExpirationDate,
	string customerName,
        string password,
	string street,
        string city,
        string state,
	string zip,
        string phone,
        string countryCode,
	string deliveryURL,
        long enabled,
	long maxEncodingPriority,
        long period,
	long maxIngestionsNumber,
        long maxStorageInGB,
	string languageCode,
        string userName,
        string userPassword,
        string userEmailAddress,
        chrono::system_clock::time_point userExpirationDate
)
{    
    string customerDirectoryName;

    customerDirectoryName.resize(customerName.size());

    transform(
        customerName.begin(), 
        customerName.end(), 
        customerDirectoryName.begin(), 
        [](unsigned char c){
            if (isalpha(c)) 
                return c; 
            else 
                return (unsigned char) '_'; } 
    );

    int64_t customerKey = _cmsEngineDBFacade->addCustomer(
            customerName, 
            customerDirectoryName,
            street,
            city,
            state,
            zip,
            phone,
            countryCode,
            deliveryURL,
            enabled,
            maxEncodingPriority,
            period,
            maxIngestionsNumber,
            maxStorageInGB,
            languageCode,
            userName,
            userPassword,
            userEmailAddress,
            userExpirationDate);

}
/*

                         	// insert default EncodingProfilesSet per Customer/ContentType
         	{
         		long			lEncodingProfilesSetKey		= -1;
         		int				iContentType;
         		

         		// video
         		{
         			iContentType			= this.iContentType_Video;
		    			sQuery = new String ("insert into CMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (" +
		    				"NULL, ?, ?, NULL)");
			    	stmt.setInt(iQueryParameterIndex++, iContentType);
			    	stmt.setLong(iQueryParameterIndex++, lCustomerKey);
		        		sQuery = new String ("select LAST_INSERT_ID()");
			    		stmt = conn.prepareStatement(sQuery);
			    		iQueryParameterIndex			= 1;
			   			logger.debug("Query: " + sQuery + " ---> ");
			    		returnSQLObject = this.executeQuery(stmt, sQuery);
			    		lSQLElapsedInMillisecs		= ((Long) (returnSQLObject [0])).longValue();
			    		rs								= (ResultSet) (returnSQLObject [1]);
			   			logger.debug("Query: " + sQuery + " ---> " +
			    			" - SQL statistics (millisecs): @" + lSQLElapsedInMillisecs + "@");

		        		if (rs.next()) {
		        			lEncodingProfilesSetKey = rs.getLong(1);

                                                		        	// by default this new customer will inherited the profiles associated to 'global' 
		        	if (lDatabase == DB_MYSQL || lDatabase == DB_ORACLE || lDatabase == DB_MSSQL)
		    			sQuery = new String ("insert into CMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey) (select ?, EncodingProfileKey from CMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = (select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null))");
		    		stmt = conn.prepareStatement(sQuery);
		    		iQueryParameterIndex			= 1;
				    stmt.setLong(iQueryParameterIndex++, lEncodingProfilesSetKey);
			    	stmt.setInt(iQueryParameterIndex++, iContentType);

                                         		// audio
         		{
	         		iContentType			= this.iContentType_Audio;

                                		    			sQuery = new String ("insert into CMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (" +
		    				"NULL, ?, ?, NULL)");
			    	stmt.setInt(iQueryParameterIndex++, iContentType);
			    	stmt.setLong(iQueryParameterIndex++, lCustomerKey);
		        		sQuery = new String ("select LAST_INSERT_ID()");
			    		stmt = conn.prepareStatement(sQuery);
			    		iQueryParameterIndex			= 1;
			   			logger.debug("Query: " + sQuery + " ---> ");
			    		returnSQLObject = this.executeQuery(stmt, sQuery);
			    		lSQLElapsedInMillisecs		= ((Long) (returnSQLObject [0])).longValue();
			    		rs								= (ResultSet) (returnSQLObject [1]);
			   			logger.debug("Query: " + sQuery + " ---> " +
			    			" - SQL statistics (millisecs): @" + lSQLElapsedInMillisecs + "@");

		        		if (rs.next()) {
		        			lEncodingProfilesSetKey = rs.getLong(1);

                                                		        	// by default this new customer will inherited the profiles associated to 'global' 
		        	if (lDatabase == DB_MYSQL || lDatabase == DB_ORACLE || lDatabase == DB_MSSQL)
		    			sQuery = new String ("insert into CMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey) (select ?, EncodingProfileKey from CMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = (select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null))");
		    		stmt = conn.prepareStatement(sQuery);
		    		iQueryParameterIndex			= 1;
				    stmt.setLong(iQueryParameterIndex++, lEncodingProfilesSetKey);
			    	stmt.setInt(iQueryParameterIndex++, iContentType);

                                         		// image
         		{
	         		iContentType			= this.iContentType_Image;
		    			sQuery = new String ("insert into CMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (" +
		    				"NULL, ?, ?, NULL)");
			    	stmt.setInt(iQueryParameterIndex++, iContentType);
			    	stmt.setLong(iQueryParameterIndex++, lCustomerKey);

                                		        		sQuery = new String ("select LAST_INSERT_ID()");
			    		stmt = conn.prepareStatement(sQuery);
			    		iQueryParameterIndex			= 1;
			   			logger.debug("Query: " + sQuery + " ---> ");
			    		returnSQLObject = this.executeQuery(stmt, sQuery);
			    		lSQLElapsedInMillisecs		= ((Long) (returnSQLObject [0])).longValue();
			    		rs								= (ResultSet) (returnSQLObject [1]);
			   			logger.debug("Query: " + sQuery + " ---> " +
			    			" - SQL statistics (millisecs): @" + lSQLElapsedInMillisecs + "@");

		        		if (rs.next()) {
		        			lEncodingProfilesSetKey = rs.getLong(1);

                                                		        	// by default this new customer will inherited the profiles associated to 'global' 
		        	if (lDatabase == DB_MYSQL || lDatabase == DB_ORACLE || lDatabase == DB_MSSQL)
		    			sQuery = new String ("insert into CMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey) (select ?, EncodingProfileKey from CMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = (select EncodingProfilesSetKey from CMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null))");
		    		stmt = conn.prepareStatement(sQuery);
		    		iQueryParameterIndex			= 1;
				    stmt.setLong(iQueryParameterIndex++, lEncodingProfilesSetKey);
			    	stmt.setInt(iQueryParameterIndex++, iContentType);

                        
}
*/