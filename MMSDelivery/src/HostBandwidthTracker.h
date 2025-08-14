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

	// Aggiorna (sovrascrive) la banda per un dato host
	void updateBandwidth(const string &host, uint64_t bandwidth);

	// Aggiunge banda (cumulativamente)
	void addBandwidth(const string &host, uint64_t bandwidth);

	optional<string> getMinBandwidthHost();

	void addHosts(unordered_set<string> &hosts);

  private:
	mutex _trackerMutex;
	// associa ad ogni host le seguenti informazioni: running, bandwidthUsage
	// e bandwidthCorrection (se il server di delivery fa anche da storage potrebbe essere utile banda fittizia perchè sia servito di meno)
	unordered_map<string, tuple<bool, uint64_t, int64_t>> _bandwidthMap;
};
