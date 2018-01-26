
#ifndef Customers_h
#define Customers_h

#include "../../CMSRepository/src/Customer.h"
#include <unordered_map>
#include <mutex>


class Customers
{
public:
    using CustomerHashMap = unordered_map<long long, shared_ptr<Customer>>;

public:
    Customers (void);

    ~Customers (void);

    shared_ptr<Customer> getCustomer (long long llCustomerKey);

    shared_ptr<Customer> getCustomer (string customerName);

    // Error addNewCustomer (long long llCustomerKey);

    // Error removeCustomer (long long llCustomerKey);

    void lockCustomers (CustomerHashMap::iterator *pItBegin, CustomerHashMap::iterator *pItEnd);

    void unLockCustomers (void);

private:
    mutex                                       _mtCustomers;
    CustomerHashMap                             _customersByKey;
    unordered_map<string, shared_ptr<Customer>> _customersByName;

    void retrieveInfoFromDB (long long llCustomerKey);

};

#endif

