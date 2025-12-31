#include "HostBandwidthTracker.h"
#include "JSONUtils.h"
#include "spdlog/spdlog.h"
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

using namespace std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

void HostBandwidthTracker::updateHosts(json hostAndRunningRoot)
{
	lock_guard<mutex> locker(_trackerMutex);

	unordered_set<string> hosts;

	// add/update
	for (int index = 0; index < hostAndRunningRoot.size(); index++)
	{
		json hostRoot = hostAndRunningRoot[index];
		string host = JSONUtils::asString(hostRoot, "host");
		bool running = JSONUtils::asBool(hostRoot, "running");
		int64_t bandwidthCorrection = JSONUtils::asInt64(hostRoot, "bandwidthCorrection", 0);

		hosts.insert(host);

		auto it = _bandwidthMap.find(host);
		if (it == _bandwidthMap.end())
			_bandwidthMap[host] = make_tuple(running, 0, bandwidthCorrection); // insert
		else
			get<0>(it->second) = running; // update
										  // it->second.first = running; // update
	}

	// remove if not present anymore
	for (auto it = _bandwidthMap.begin(); it != _bandwidthMap.end();)
	{
		const string &host = it->first;

		// erase(it) restituisce l’iteratore al prossimo elemento valido → non serve fare ++it in quel caso.
		if (hosts.find(host) == hosts.end())
			it = _bandwidthMap.erase(it); // remove
		else
			++it; // Avanza solo se non si cancella
	}
}

void HostBandwidthTracker::addBandwidth(const string &host, uint64_t bandwidth)
{
	lock_guard<mutex> locker(_trackerMutex);
	auto it = _bandwidthMap.find(host);
	if (it != _bandwidthMap.end())
		get<1>(it->second) += bandwidth;
}

optional<string> HostBandwidthTracker::getMinBandwidthHost()
{
	lock_guard<mutex> locker(_trackerMutex);

	if (_bandwidthMap.empty())
		return nullopt;

	uint64_t minBandwidth = numeric_limits<uint64_t>::max();
	string minHost;

	for (const auto &[host, bandwidthDetails] : _bandwidthMap)
	{
		auto [running, bandwidth, bandwidthCorrection] = bandwidthDetails;

		SPDLOG_INFO(
			"getMinBandwidthHost"
			", host: {}"
			", running: {}"
			", bandwidthCorrection: {}"
			", bandwidth: {} ({} Mbps)",
			host, running, bandwidthCorrection, bandwidth, (bandwidth * 8) / 1000000
		);

		bandwidth += bandwidthCorrection;

		if (running && bandwidth < minBandwidth)
		{
			minBandwidth = bandwidth;
			minHost = host;

			if (bandwidth == 0)
				break; // inutile cercare un host con meno banda
		}
	}

	if (minHost.empty())
		return nullopt;
	else
	{
		SPDLOG_INFO(
			"getMinBandwidthHost"
			", minHost: {}"
			", minBandwidth: {} ({} Mbps)",
			minHost, minBandwidth, (minBandwidth * 8) / 1000000
		);
		return minHost;
	}
}

void HostBandwidthTracker::updateBandwidth(const string &host, uint64_t bandwidth)
{
	lock_guard<mutex> locker(_trackerMutex);

	auto it = _bandwidthMap.find(host);
	if (it != _bandwidthMap.end())
		get<1>(it->second) = bandwidth;
}

void HostBandwidthTracker::addRunningHosts(unordered_set<string> &hosts)
{
	lock_guard<mutex> locker(_trackerMutex);

	for (const auto &[host, bandwidthDetails] : _bandwidthMap)
	{
		bool running;
		tie(running, ignore, ignore) = bandwidthDetails;

		if (running)
			hosts.insert(host);
	}
}

/*
uint64_t HostBandwidthTracker::getBandwidth(const string &hostname)
{
	lock_guard<mutex> locker(_trackerMutex);
	auto it = _bandwidthMap.find(hostname);
	return (it != _bandwidthMap.end()) ? it->second.second : 0;
}

// Stampa tutti gli host
void HostBandwidthTracker::logDetails() const
{
	for (const auto &[host, bandwidth] : _bandwidthMap)
		SPDLOG_INFO(
			"HostBandwidthTracker"
			", host: {}"
			", bandwidth (bytes): {}",
			host, bandwidth.second
		);
}
*/
