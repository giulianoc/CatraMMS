#include "HostBandwidthTracker.h"
#include "JSONUtils.h"
#include "spdlog/spdlog.h"
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

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

		hosts.insert(host);

		auto it = _bandwidthMap.find(host);
		if (it == _bandwidthMap.end())
			_bandwidthMap[host] = make_pair(running, 0); // insert
		else
			it->second.first = running; // update
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
		it->second.second += bandwidth;
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
		bool running = bandwidthDetails.first;
		uint64_t bandwidth = bandwidthDetails.second;

		SPDLOG_INFO(
			"bandwidthUsageThread getMinBandwidthHost"
			", host: {}"
			", running: {}"
			", bandwidth: {}",
			host, running, bandwidth
		);
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
		return minHost;
}

void HostBandwidthTracker::updateBandwidth(const string &host, uint64_t bandwidth)
{
	lock_guard<mutex> locker(_trackerMutex);

	auto it = _bandwidthMap.find(host);
	if (it != _bandwidthMap.end())
		it->second.second = bandwidth;
}

void HostBandwidthTracker::addHosts(unordered_set<string> &hosts)
{
	lock_guard<mutex> locker(_trackerMutex);

	for (const auto &[host, bandwidthDetails] : _bandwidthMap)
		hosts.insert(host);
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
