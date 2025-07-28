// #include <cstdint>
#include "nlohmann/json.hpp"
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

using namespace std;

using json = nlohmann::json;
using orderd_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

class HostBandwidthTracker
{
  public:
	void updateHosts(json hostAndRunningRoot);

	// Aggiorna (sovrascrive) la banda per un dato hostname
	void updateBandwidth(const string &hostname, uint64_t bandwidth);

	// Aggiunge banda (cumulativamente)
	void addBandwidth(const string &hostname, uint64_t bandwidth);

	optional<string> getMinBandwidthHost();

	void addHosts(unordered_set<string> &hosts);

  private:
	mutex _trackerMutex;
	// associa ad ogni hostname due informazioni: running e bandwidthUsage
	unordered_map<string, pair<bool, uint64_t>> _bandwidthMap;
};
