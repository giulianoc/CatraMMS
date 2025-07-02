
#ifndef Workspace_h
#define Workspace_h

#include <string>
#include <unordered_map>

using namespace std;

struct Workspace
{
	using TerritoriesHashMap = unordered_map<long, string>;

	long long _workspaceKey;
	string _name;
	string _directoryName;
	int _maxEncodingPriority;
	string _anotes;
	string _externalDeliveries;

	unsigned long _maxStorageInGB;
	unsigned long _currentCostForStorage;
	unsigned long _dedicatedEncoder_power_1;
	unsigned long _currentCostForDedicatedEncoder_power_1;
	unsigned long _dedicatedEncoder_power_2;
	unsigned long _currentCostForDedicatedEncoder_power_2;
	unsigned long _dedicatedEncoder_power_3;
	unsigned long _currentCostForDedicatedEncoder_power_3;
	unsigned long _CDN_type_1;
	unsigned long _currentCostForCDN_type_1;
	bool _support_type_1;
	unsigned long _currentCostForSupport_type_1;

	TerritoriesHashMap _territories;
};

#endif
