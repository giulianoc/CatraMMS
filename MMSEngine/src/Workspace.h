
#ifndef Workspace_h
#define Workspace_h

#include <string>
#include <unordered_map>

using namespace std;

struct Workspace
{
	using TerritoriesHashMap = unordered_map<long,string>;

	long long				_workspaceKey;
	string					_name;
	string					_directoryName;
	unsigned long			_maxStorageInMB;
	int						_maxEncodingPriority;

	unsigned long			_maxStorageInGB;
	unsigned long			_currentCostForStorage;
	unsigned long			_dedicatedEncoder_power_1;
	unsigned long			_currentCostForDedicatedEncoder_power_1;
	unsigned long			_dedicatedEncoder_power_2;
	unsigned long			_currentCostForDedicatedEncoder_power_2;
	unsigned long			_dedicatedEncoder_power_3;
	unsigned long			_currentCostForDedicatedEncoder_power_3;
	bool					_support24x7;
	unsigned long			_currentCostForSupport24x7;

    TerritoriesHashMap		_territories;
};

#endif
