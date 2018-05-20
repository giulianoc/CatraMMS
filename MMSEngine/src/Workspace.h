
#ifndef Workspace_h
#define Workspace_h

#include <string>
#include <unordered_map>

using namespace std;

struct Workspace
{   
    using TerritoriesHashMap = unordered_map<long,string>;

    long long               _workspaceKey;
    string                  _name;
    string                  _directoryName;
    unsigned long           _maxStorageInMB;
    int                     _maxEncodingPriority;

    TerritoriesHashMap      _territories;
};

#endif
