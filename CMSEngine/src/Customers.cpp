
#include "Customers.h"


Customers:: Customers (void)
{
    retrieveInfoFromDB (-1);
}

Customers:: ~Customers (void)
{

}

shared_ptr<Customer> Customers:: getCustomer (string customerName)
{

    shared_ptr<Customer>                        customer;


    lock_guard<mutex>           locker(_mtCustomers);

    unordered_map<string, shared_ptr<Customer>>::const_iterator it =
            _customersByName.find(customerName);
    if (it == _customersByName.end())
        throw invalid_argument(string("Customer not found")
                + ", customer name: " + customerName
                );

    return it->second;
}

shared_ptr<Customer> Customers:: getCustomer (long long customerKey)
{

    shared_ptr<Customer>                        customer;


    lock_guard<mutex>           locker(_mtCustomers);

    unordered_map<long long, shared_ptr<Customer>>::const_iterator it =
            _customersByKey.find(customerKey);
    if (it == _customersByKey.end())
        throw invalid_argument(string("Customer not found")
                + ", customer key: " + to_string(customerKey)
                );

    return it->second;
}

/*
Error Customers:: addNewCustomer (long long llCustomerKey)

{

	if (retrieveInfoFromDB (llCustomerKey) != errNoError)
	{
		Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_CUSTOMERS_RETRIEVEINFOFROMDB_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}


	return errNoError;
}


Error Customers:: removeCustomer (long long llCustomerKey)

{

	if (_mtCustomers. lock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_LOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	if (!_phmCustomers -> Delete (llCustomerKey))
	{
		// false means the key was not found and the table was not modified

		Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_CUSTOMERS_CUSTOMERKEYNOTFOUNDINHASHMAP,
			1, llCustomerKey);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (_mtCustomers. unLock () != errNoError)
		{
			Error err = PThreadErrors (__FILE__, __LINE__,
				THREADLIB_PMUTEX_UNLOCK_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
				__FILE__, __LINE__);
		}

		return err;
	}

	if (_mtCustomers. unLock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_UNLOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}


	return errNoError;
}
 */

void Customers:: lockCustomers (CustomerHashMap::iterator *pItBegin,
    CustomerHashMap::iterator *pItEnd)
{

    _mtCustomers.lock();

    *pItBegin			= _customersByKey.begin();
    *pItEnd			= _customersByKey.end();
}

void Customers:: unLockCustomers (void)
{
    _mtCustomers.unlock();
}

void Customers:: retrieveInfoFromDB (long long llCustomerKey)
{
/*
	Buffer_t					bRelativePathWithoutParametersForHttpGet;
	Buffer_t					bURLParametersForHttpGet;
	Buffer_t					bHttpGetBodyResponse;
	HttpGetThread_t				hgGetCustomers;
	Customer_p					pcCustomer;
	Error_t						errRun;
	Error_t						errGetAvailableModule;
	char						pWebServerIPAddress [
		SCK_MAXIPADDRESSLENGTH];
	unsigned long				ulWebServerPort;


	if (bHttpGetBodyResponse. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bRelativePathWithoutParametersForHttpGet. init () !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bRelativePathWithoutParametersForHttpGet. setBuffer (
		"/CMSEngine/getCustomersTerritories") !=
		errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_SETBUFFER_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (bURLParametersForHttpGet. init () != errNoError)
	{
		Error err = ToolsErrors (__FILE__, __LINE__,
			TOOLS_BUFFER_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (llCustomerKey != -1)
	{
		if (WebUtility:: addURLParameter (&bURLParametersForHttpGet,
			"CustomerKey", llCustomerKey) != errNoError)
		{
			Error err = WebToolsErrors (__FILE__, __LINE__,
				WEBTOOLS_WEBUTILITY_ADDURLPARAMETER_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			return err;
		}
	}

	if ((errGetAvailableModule =
		_plbWebServerLoadBalancer -> getAvailableModule (
		"WebServers", pWebServerIPAddress,
		SCK_MAXIPADDRESSLENGTH,
		&ulWebServerPort)) != errNoError)
	{
		Error err = LoadBalancerErrors (
			__FILE__, __LINE__,
			LB_LOADBALANCER_GETAVAILABLEMODULE_FAILED,
			1, "WebServers");
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	{
		Message msg = CMSEngineMessages (__FILE__, __LINE__,
			CMS_HTTPGETTHREAD,
			8,
			"<not available>",
			"getCustomersTerritories",
			pWebServerIPAddress,
			ulWebServerPort,
			_pWebServerLocalIPAddress,
			bRelativePathWithoutParametersForHttpGet. str (),
			bURLParametersForHttpGet. str (),
			_ulWebServerTimeoutToWaitAnswerInSeconds);
		_ptSystemTracer -> trace (Tracer:: TRACER_LINFO,
			(const char *) msg, __FILE__, __LINE__);
	}

	if (hgGetCustomers. init (
		pWebServerIPAddress,
		ulWebServerPort,
		bRelativePathWithoutParametersForHttpGet. str (),
		bURLParametersForHttpGet. str (),
		(const char *) NULL,
		(const char *) NULL,
		_ulWebServerTimeoutToWaitAnswerInSeconds,
		0,
		_ulWebServerTimeoutToWaitAnswerInSeconds,
		0,
		_pWebServerLocalIPAddress) != errNoError)
	{
		Error err = WebToolsErrors (__FILE__, __LINE__,
			WEBTOOLS_HTTPGETTHREAD_INIT_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if ((errRun = hgGetCustomers. run (
		(Buffer_p) NULL, &bHttpGetBodyResponse,
		(Buffer_p) NULL, (Buffer_p) NULL, (Buffer_p) NULL)) != errNoError)
	{
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) errRun, __FILE__, __LINE__);

		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PTHREAD_RUN_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (hgGetCustomers. finish () !=
			errNoError)
		{
			Error err = WebToolsErrors (__FILE__, __LINE__,
				WEBTOOLS_HTTPGETTHREAD_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (strstr ((const char *) bHttpGetBodyResponse,
		"<Status><![CDATA[SUCCESS") == (char *) NULL)
	{
		Error err = CMSEngineErrors (__FILE__, __LINE__,
			CMS_SERVLETFAILED,
			4, "getCustomersTerritories", pWebServerIPAddress, "",
			(const char *) bHttpGetBodyResponse);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		if (hgGetCustomers. finish () !=
			errNoError)
		{
			Error err = WebToolsErrors (__FILE__, __LINE__,
				WEBTOOLS_HTTPGETTHREAD_FINISH_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);
		}

		return err;
	}

	if (hgGetCustomers. finish () != errNoError)
	{
		Error err = WebToolsErrors (__FILE__, __LINE__,
			WEBTOOLS_HTTPGETTHREAD_FINISH_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
			(const char *) err, __FILE__, __LINE__);

		return err;
	}

	if (_mtCustomers. lock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_LOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}

	{
		xmlDocPtr			pxdXMLDocument;
		xmlNodePtr			pxnCustomersXMLNode;	// Customers
		xmlNodePtr			pxnCustomerXMLNode;	// Customer
		xmlNodePtr			pxnTerritoryXMLNode;	// Territory
		xmlChar				*pxcValue;
		int					iDidInsert;


		if ((pxdXMLDocument = xmlParseMemory (
			(const char *) bHttpGetBodyResponse,
			(unsigned long) bHttpGetBodyResponse)) ==
			(xmlDocPtr) NULL)
		{
			// parse error
			Error err = CMSEngineErrors (__FILE__, __LINE__,
				CMS_LIBXML2_XMLPARSEMEMORY_FAILED,
				2,
				"getCustomersTerritories",
				(const char *) bHttpGetBodyResponse);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			if (_mtCustomers. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}
	
		// Customers
		if ((pxnCustomersXMLNode = xmlDocGetRootElement (pxdXMLDocument)) ==
			(xmlNodePtr) NULL)
		{
			// empty document
			Error err = CMSEngineErrors (__FILE__, __LINE__,
				CMS_LIBXML2_XMLDOCROOTELEMENT_FAILED);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			xmlFreeDoc (pxdXMLDocument);

			if (_mtCustomers. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		while (pxnCustomersXMLNode != (xmlNodePtr) NULL &&
			xmlStrcmp (pxnCustomersXMLNode -> name,
			(const xmlChar *) "Customers"))
			pxnCustomersXMLNode			= pxnCustomersXMLNode -> next;

		if (pxnCustomersXMLNode == (xmlNodePtr) NULL ||
			(pxnCustomersXMLNode = pxnCustomersXMLNode -> xmlChildrenNode) ==
			(xmlNodePtr) NULL)
		{
			Error err = CMSEngineErrors (__FILE__, __LINE__,
				CMS_XMLWRONG,
				2,
				"getCustomersTerritories",
				(const char *) bHttpGetBodyResponse);
			_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
				(const char *) err, __FILE__, __LINE__);

			xmlFreeDoc (pxdXMLDocument);

			if (_mtCustomers. unLock () != errNoError)
			{
				Error err = PThreadErrors (__FILE__, __LINE__,
					THREADLIB_PMUTEX_UNLOCK_FAILED);
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);
			}

			return err;
		}

		while (pxnCustomersXMLNode != (xmlNodePtr) NULL)
		{
			if (!xmlStrcmp (pxnCustomersXMLNode -> name, (const xmlChar *) 
				"Customer"))
			{
				if (pxnCustomersXMLNode == (xmlNodePtr) NULL ||
					(pxnCustomerXMLNode =
						pxnCustomersXMLNode -> xmlChildrenNode) ==
					(xmlNodePtr) NULL)
				{
					Error err = CMSEngineErrors (__FILE__, __LINE__,
						CMS_XMLWRONG,
						2,
						"getCustomersTerritories",
						(const char *) bHttpGetBodyResponse);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					xmlFreeDoc (pxdXMLDocument);

					if (_mtCustomers. unLock () != errNoError)
					{
						Error err = PThreadErrors (__FILE__, __LINE__,
							THREADLIB_PMUTEX_UNLOCK_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				if ((pcCustomer = new Customer_t) == (Customer_p) NULL)
				{
					Error err = CMSEngineErrors (
						__FILE__, __LINE__,
						CMS_NEW_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					xmlFreeDoc (pxdXMLDocument);

					if (_mtCustomers. unLock () != errNoError)
					{
						Error err = PThreadErrors (__FILE__, __LINE__,
							THREADLIB_PMUTEX_UNLOCK_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}

				while (pxnCustomerXMLNode != (xmlNodePtr) NULL)
				{
					if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "Name"))
					{
						if ((pxcValue = xmlNodeListGetString (
							pxdXMLDocument,
							pxnCustomerXMLNode -> xmlChildrenNode,
							1)) != (xmlChar *) NULL)
						{
							// xmlNodeListGetString NULL means an empty element

							if (xmlStrlen (pxcValue) >=
								CMS_CUSTOMERS_MAXCUSTOMERNAMELENGTH)
							{
								Error err = CMSEngineErrors (
									__FILE__, __LINE__,
									CMS_FIELDTOOLONG, 2, "Name",
									(unsigned long) xmlStrlen (pxcValue));
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								delete pcCustomer;
								xmlFree (pxcValue);
								xmlFreeDoc (pxdXMLDocument);

								if (_mtCustomers. unLock () != errNoError)
								{
									Error err = PThreadErrors (
										__FILE__, __LINE__,
										THREADLIB_PMUTEX_UNLOCK_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err,
										__FILE__, __LINE__);
								}

								return err;
							}

							pcCustomer -> _bName. setBuffer (
								(const char *) pxcValue);

							xmlFree (pxcValue);
						}
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "DirectoryName"))
					{
						if ((pxcValue = xmlNodeListGetString (
							pxdXMLDocument,
							pxnCustomerXMLNode -> xmlChildrenNode,
							1)) != (xmlChar *) NULL)
						{
							// xmlNodeListGetString NULL means an empty element

							if (xmlStrlen (pxcValue) >=
								CMS_CUSTOMERS_MAXCUSTOMERNAMELENGTH)
							{
								Error err = CMSEngineErrors (
									__FILE__, __LINE__,
									CMS_FIELDTOOLONG, 2, "DirectoryName",
									(unsigned long) xmlStrlen (pxcValue));
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								delete pcCustomer;
								xmlFree (pxcValue);
								xmlFreeDoc (pxdXMLDocument);

								if (_mtCustomers. unLock () != errNoError)
								{
									Error err = PThreadErrors (
										__FILE__, __LINE__,
										THREADLIB_PMUTEX_UNLOCK_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err,
										__FILE__, __LINE__);
								}

								return err;
							}

							pcCustomer -> _bDirectoryName. setBuffer (
								(const char *) pxcValue);

							xmlFree (pxcValue);
						}
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "Key"))
					{
						if ((pxcValue = xmlNodeListGetString (
							pxdXMLDocument,
								pxnCustomerXMLNode -> xmlChildrenNode,
							1)) != (xmlChar *) NULL)
						{
							// xmlNodeListGetString NULL means an empty element

							if (xmlStrlen (pxcValue) >=
								CMS_CUSTOMERS_MAXKEYLENGTH)
							{
								Error err = CMSEngineErrors (
									__FILE__, __LINE__,
									CMS_FIELDTOOLONG, 2, "Key",
									(unsigned long) xmlStrlen (pxcValue));
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								delete pcCustomer;
								xmlFree (pxcValue);
								xmlFreeDoc (pxdXMLDocument);

								if (_mtCustomers. unLock () != errNoError)
								{
									Error err = PThreadErrors (
										__FILE__, __LINE__,
										THREADLIB_PMUTEX_UNLOCK_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err,
										__FILE__, __LINE__);
								}

								return err;
							}

							pcCustomer -> _llCustomerKey	=
								strtoll ((const char *) pxcValue,
								(char **) NULL, 10);

							xmlFree (pxcValue);
						}
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "MaxStorageInGB"))
					{
						if ((pxcValue = xmlNodeListGetString (
							pxdXMLDocument,
								pxnCustomerXMLNode -> xmlChildrenNode,
							1)) != (xmlChar *) NULL)
						{
							// xmlNodeListGetString NULL means an empty element

							if (xmlStrlen (pxcValue) >=
								CMS_CUSTOMERS_MAXSTORAGELENGTH)
							{
								Error err = CMSEngineErrors (
									__FILE__, __LINE__,
									CMS_FIELDTOOLONG, 2, "MaxStorageInGB",
									(unsigned long) xmlStrlen (pxcValue));
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								delete pcCustomer;
								xmlFree (pxcValue);
								xmlFreeDoc (pxdXMLDocument);

								if (_mtCustomers. unLock () != errNoError)
								{
									Error err = PThreadErrors (
										__FILE__, __LINE__,
										THREADLIB_PMUTEX_UNLOCK_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err,
										__FILE__, __LINE__);
								}

								return err;
							}

							pcCustomer -> _ulMaxStorageInGB	=
								strtoul ((const char *) pxcValue,
								(char **) NULL, 10);

							xmlFree (pxcValue);
						}
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "Territory"))
					{
						Buffer_p			pbTerritoryName;
						long long			llTerritoryKey;


						pbTerritoryName		= (Buffer_p) NULL;
						llTerritoryKey		= -1;

						if (pxnCustomerXMLNode == (xmlNodePtr) NULL ||
							(pxnTerritoryXMLNode =
								pxnCustomerXMLNode -> xmlChildrenNode) ==
							(xmlNodePtr) NULL)
						{
							Error err = CMSEngineErrors (__FILE__, __LINE__,
								CMS_XMLWRONG,
								2,
								"getCustomersTerritories",
								(const char *) bHttpGetBodyResponse);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							xmlFreeDoc (pxdXMLDocument);

							if (_mtCustomers. unLock () != errNoError)
							{
								Error err = PThreadErrors (__FILE__, __LINE__,
									THREADLIB_PMUTEX_UNLOCK_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

							return err;
						}

						while (pxnTerritoryXMLNode != (xmlNodePtr) NULL)
						{
							if (!xmlStrcmp (pxnTerritoryXMLNode -> name,
								(const xmlChar *) "Name"))
							{
								if ((pxcValue = xmlNodeListGetString (
									pxdXMLDocument,
										pxnTerritoryXMLNode -> xmlChildrenNode,
									1)) != (xmlChar *) NULL)
								{
									// xmlNodeListGetString NULL means
									// an empty element

									if (xmlStrlen (pxcValue) >=
										CMS_CUSTOMERS_MAXTERRITORYNAMELENGTH)
									{
										Error err = CMSEngineErrors (
											__FILE__, __LINE__,
											CMS_FIELDTOOLONG, 2, "Name",
											(unsigned long)
											xmlStrlen (pxcValue));
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);

										delete pcCustomer;
										xmlFree (pxcValue);
										xmlFreeDoc (pxdXMLDocument);

										if (_mtCustomers. unLock () !=
											errNoError)
										{
											Error err = PThreadErrors (
												__FILE__, __LINE__,
												THREADLIB_PMUTEX_UNLOCK_FAILED);
											_ptSystemTracer -> trace (
												Tracer:: TRACER_LERRR,
												(const char *) err,
												__FILE__, __LINE__);
										}

										return err;
									}

									if ((pbTerritoryName = new Buffer_t) ==
										(Buffer_p) NULL)
									{
										Error err = CMSEngineErrors (
											__FILE__, __LINE__,
											CMS_NEW_FAILED);
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);

										delete pcCustomer;
										xmlFree (pxcValue);
										xmlFreeDoc (pxdXMLDocument);

										if (_mtCustomers. unLock () !=
											errNoError)
										{
											Error err = PThreadErrors (
												__FILE__, __LINE__,
												THREADLIB_PMUTEX_UNLOCK_FAILED);
											_ptSystemTracer -> trace (
												Tracer:: TRACER_LERRR,
												(const char *) err,
												__FILE__, __LINE__);
										}

										return err;
									}

									if (pbTerritoryName -> init (
										(const char *) pxcValue) != errNoError)
									{
										Error err = ToolsErrors (
											__FILE__, __LINE__,
											TOOLS_BUFFER_INIT_FAILED);
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);

										delete pbTerritoryName;

										delete pcCustomer;
										xmlFree (pxcValue);
										xmlFreeDoc (pxdXMLDocument);

										if (_mtCustomers. unLock () !=
											errNoError)
										{
											Error err = PThreadErrors (
												__FILE__, __LINE__,
												THREADLIB_PMUTEX_UNLOCK_FAILED);
											_ptSystemTracer -> trace (
												Tracer:: TRACER_LERRR,
												(const char *) err,
												__FILE__, __LINE__);
										}

										return err;
									}

									xmlFree (pxcValue);
								}
							}
							else if (!xmlStrcmp (pxnTerritoryXMLNode -> name,
								(const xmlChar *) "Key"))
							{
								if ((pxcValue = xmlNodeListGetString (
									pxdXMLDocument,
										pxnTerritoryXMLNode -> xmlChildrenNode,
									1)) != (xmlChar *) NULL)
								{
									// xmlNodeListGetString NULL means
									// an empty element

									if (xmlStrlen (pxcValue) >=
										CMS_CUSTOMERS_MAXKEYLENGTH)
									{
										Error err = CMSEngineErrors (
											__FILE__, __LINE__,
											CMS_FIELDTOOLONG, 2, "Key",
											(unsigned long)
											xmlStrlen (pxcValue));
										_ptSystemTracer -> trace (
											Tracer:: TRACER_LERRR,
											(const char *) err,
											__FILE__, __LINE__);

										delete pcCustomer;
										xmlFree (pxcValue);
										xmlFreeDoc (pxdXMLDocument);

										if (_mtCustomers. unLock () !=
											errNoError)
										{
											Error err = PThreadErrors (
												__FILE__, __LINE__,
												THREADLIB_PMUTEX_UNLOCK_FAILED);
											_ptSystemTracer -> trace (
												Tracer:: TRACER_LERRR,
												(const char *) err,
												__FILE__, __LINE__);
										}

										return err;
									}

									llTerritoryKey	=
										strtoll ((const char *) pxcValue,
										(char **) NULL, 10);

									xmlFree (pxcValue);
								}
							}
							else if (!xmlStrcmp (pxnTerritoryXMLNode -> name,
								(const xmlChar *) "text"))
							{
							}
							else if (!xmlStrcmp (pxnTerritoryXMLNode -> name,
								(const xmlChar *) "comment"))
							{
							}
							else
							{
								Error err = CMSEngineErrors (__FILE__, __LINE__,
									CMS_XMLPARAMETERUNKNOWN,
									2, "getCustomersTerritories",
									(const char *)
									(pxnTerritoryXMLNode -> name));
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);

								delete pcCustomer;
								xmlFreeDoc (pxdXMLDocument);

								if (_mtCustomers. unLock () != errNoError)
								{
									Error err = PThreadErrors (
										__FILE__, __LINE__,
										THREADLIB_PMUTEX_UNLOCK_FAILED);
									_ptSystemTracer -> trace (
										Tracer:: TRACER_LERRR,
										(const char *) err, __FILE__, __LINE__);
								}

								return err;
							}

							if ((pxnTerritoryXMLNode =
								pxnTerritoryXMLNode -> next) ==
								(xmlNodePtr) NULL)
							{
							}
						}

						if (pbTerritoryName == (Buffer_p) NULL ||
							llTerritoryKey == -1)
						{
							Error err = CMSEngineErrors (__FILE__, __LINE__,
								CMS_XMLWRONG,
								2,
								"getCustomersTerritories",
								(const char *) bHttpGetBodyResponse);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							delete pcCustomer;
							xmlFreeDoc (pxdXMLDocument);

							if (_mtCustomers. unLock () != errNoError)
							{
								Error err = PThreadErrors (__FILE__, __LINE__,
									THREADLIB_PMUTEX_UNLOCK_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

							return err;
						}

						(pcCustomer -> _phmTerritories) ->
							InsertWithoutDuplication (llTerritoryKey,
							pbTerritoryName, &iDidInsert);

						if (!iDidInsert)
						{
							Error err = CMSEngineErrors (__FILE__, __LINE__,
								CMS_HASHMAP_INSERTWITHOUTDUPLICATION_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);

							delete pcCustomer;
							xmlFreeDoc (pxdXMLDocument);

							if (_mtCustomers. unLock () != errNoError)
							{
								Error err = PThreadErrors (__FILE__, __LINE__,
									THREADLIB_PMUTEX_UNLOCK_FAILED);
								_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
									(const char *) err, __FILE__, __LINE__);
							}

							return err;
						}
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "text"))
					{
					}
					else if (!xmlStrcmp (pxnCustomerXMLNode -> name,
						(const xmlChar *) "comment"))
					{
					}
					else
					{
						Error err = CMSEngineErrors (__FILE__, __LINE__,
							CMS_XMLPARAMETERUNKNOWN,
							2, "getCustomersTerritories",
							(const char *) (pxnCustomerXMLNode -> name));
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);

						delete pcCustomer;
						xmlFreeDoc (pxdXMLDocument);

						if (_mtCustomers. unLock () != errNoError)
						{
							Error err = PThreadErrors (__FILE__, __LINE__,
								THREADLIB_PMUTEX_UNLOCK_FAILED);
							_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
								(const char *) err, __FILE__, __LINE__);
						}

						return err;
					}

					if ((pxnCustomerXMLNode = pxnCustomerXMLNode -> next) ==
						(xmlNodePtr) NULL)
					{
					}
				}

				_phmCustomers -> InsertWithoutDuplication (
					pcCustomer -> _llCustomerKey,
					pcCustomer, &iDidInsert);

				if (!iDidInsert)
				{
					Error err = CMSEngineErrors (__FILE__, __LINE__,
						CMS_HASHMAP_INSERTWITHOUTDUPLICATION_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);

					delete pcCustomer;
					xmlFreeDoc (pxdXMLDocument);

					if (_mtCustomers. unLock () != errNoError)
					{
						Error err = PThreadErrors (__FILE__, __LINE__,
							THREADLIB_PMUTEX_UNLOCK_FAILED);
						_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
							(const char *) err, __FILE__, __LINE__);
					}

					return err;
				}
			}
			else if (!xmlStrcmp (pxnCustomersXMLNode -> name,
				(const xmlChar *) "Status"))
			{
			}
			else if (!xmlStrcmp (pxnCustomersXMLNode -> name,
				(const xmlChar *) "text"))
			{
			}
			else if (!xmlStrcmp (pxnCustomersXMLNode -> name,
				(const xmlChar *) "comment"))
			{
			}
			else
			{
				Error err = CMSEngineErrors (__FILE__, __LINE__,
					CMS_XMLPARAMETERUNKNOWN,
					2, "getCustomersTerritories",
					(const char *) (pxnCustomersXMLNode -> name));
				_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
					(const char *) err, __FILE__, __LINE__);

				xmlFreeDoc (pxdXMLDocument);

				if (_mtCustomers. unLock () != errNoError)
				{
					Error err = PThreadErrors (__FILE__, __LINE__,
						THREADLIB_PMUTEX_UNLOCK_FAILED);
					_ptSystemTracer -> trace (Tracer:: TRACER_LERRR,
						(const char *) err, __FILE__, __LINE__);
				}

				return err;
			}

			if ((pxnCustomersXMLNode = pxnCustomersXMLNode -> next) ==
				(xmlNodePtr) NULL)
			{
			}
		}

		xmlFreeDoc (pxdXMLDocument);
	}

	if (_mtCustomers. unLock () != errNoError)
	{
		Error err = PThreadErrors (__FILE__, __LINE__,
			THREADLIB_PMUTEX_UNLOCK_FAILED);
		_ptSystemTracer -> trace (Tracer:: TRACER_LERRR, (const char *) err,
			__FILE__, __LINE__);

		return err;
	}


	return errNoError;
*/
}

