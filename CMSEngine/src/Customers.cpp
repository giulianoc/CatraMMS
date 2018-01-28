
#include "Customers.h"


Customers:: Customers (shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade)
{
    _cmsEngineDBFacade  = cmsEngineDBFacade;
    
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

Customers::CustomerHashMap Customers:: getCustomers ()
{

    lock_guard<mutex>           locker(_mtCustomers);

    return _customersByKey; // by copy
}

void Customers:: retrieveInfoFromDB (long long llCustomerKey)
{

    vector<shared_ptr<Customer>> customers = _cmsEngineDBFacade->getCustomers();

    lock_guard<mutex>           locker(_mtCustomers);
    
    for (shared_ptr<Customer> customer: customers)
    {
        _customersByKey.insert(make_pair(customer->_customerKey, customer));
        _customersByName.insert(make_pair(customer->_name, customer));
    }
}

