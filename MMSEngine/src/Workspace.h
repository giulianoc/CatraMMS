
#ifndef Workspace_h
#define Workspace_h

#include "nlohmann/json.hpp"
#include <string>
#include <unordered_map>

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

struct Workspace
{
	using TerritoriesHashMap = unordered_map<long, string>;

	long long _workspaceKey;
	string _name;
	string _directoryName;
	int _maxEncodingPriority;
	string _notes;
	json _externalDeliveriesRoot = nullptr;
	json _preferences = nullptr;

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
