
#ifndef Workspace_h
#define Workspace_h

#include "nlohmann/json.hpp"
#include <string>
#include <unordered_map>

struct Workspace
{
	using TerritoriesHashMap = std::unordered_map<long, std::string>;

	long long _workspaceKey;
	std::string _name;
	std::string _directoryName;
	int _maxEncodingPriority;
	std::string _notes;
	nlohmann::json _externalDeliveriesRoot = nullptr;
	nlohmann::json _preferences = nullptr;

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
