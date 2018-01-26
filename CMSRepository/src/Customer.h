
#ifndef Customer_h
#define Customer_h

#include <string>
#include <unordered_map>

using namespace std;

struct Customer
{   
    using TerritoriesHashMap = unordered_map<long,string>;

    long long               _customerKey;
    string                  _name;
    string                  _directoryName;
    unsigned long           _maxStorageInGB;

    TerritoriesHashMap      _territories;
};

#endif
