
#ifndef Customers_h
#define Customers_h

#include <unordered_map>
#include <mutex>
#include <memory>
#include "../../CMSRepository/src/Customer.h"
#include "CMSEngineDBFacade.h"

class Customers
{
public:
    using CustomerHashMap = unordered_map<long long, shared_ptr<Customer>>;

private:
    shared_ptr<CMSEngineDBFacade>               _cmsEngineDBFacade;
    mutex                                       _mtCustomers;
    CustomerHashMap                             _customersByKey;
    unordered_map<string, shared_ptr<Customer>> _customersByName;

    void retrieveInfoFromDB (long long llCustomerKey);
    
public:
    Customers (shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade);

    ~Customers (void);

    shared_ptr<Customer> getCustomer (long long llCustomerKey);

    shared_ptr<Customer> getCustomer (string customerName);

    // Error addNewCustomer (long long llCustomerKey);

    // Error removeCustomer (long long llCustomerKey);

    Customers::CustomerHashMap getCustomers ();
};

#endif

