
#pragma once

#include <unordered_map>
#include <deque>
#include <vector>
#include <mutex>
#include <chrono>

// Questa classe implementa un rate limiter/checking per workspace, con una logica di sharding per ridurre la contesa sui mutex.
// L'idea è di avere più "shard" (mappe separate) e associare ogni workspace ad uno shard in base ad una funzione di hash.
// Ogni shard ha la sua mappa e il suo mutex, quindi le richieste per workspace diversi che cadono
// su shard diversi possono essere processate in parallelo senza contesa.
// La funzione allowRequest controlla se una richiesta per un workspace è consentita in base al numero di richieste recenti
// (dentro una finestra temporale).
// La funzione cleanupEmpty permette di rimuovere i workspace che non hanno richieste recenti, per evitare che la mappa cresca indefinitamente.
// Questa classe è contenuta in MMSDeliveryAuthorization che viene usato da tutti i thread APIs
class WorkspaceRateChecking
{
public:
	// Analogia per capire lo shard: immagina una banca:
	// - Un solo sportello → coda lunga
	// - 32 sportelli → le persone si distribuiscono
	// Lo shard è uno sportello.
	explicit WorkspaceRateChecking(const size_t shards = 16, const size_t maxRequests = 5,
		const std::chrono::seconds window = std::chrono::seconds(15))
		: _shards(shards), _maxRequests(maxRequests), _window(window), _maps(shards), _mutexes(shards)
	{
	}

	bool burstOfRequests(const int64_t workspaceKey)
	{
		const auto now = Clock::now();
		const size_t shardIndex = shard(workspaceKey);

		std::lock_guard lock(_mutexes[shardIndex]);

		auto& map = _maps[shardIndex];
		const auto it = map.find(workspaceKey);
		if (it == map.end())
		{
		  	// Se è la prima richiesta per questo workspace, creo la coda e aggiungo il timestamp
			map[workspaceKey].push_back(now);
			return false;
		}
		auto& dq = it->second;

		// Rimuove richieste fuori finestra
		while (!dq.empty() && now - dq.front() > _window)
			dq.pop_front();

		if (dq.size() >= _maxRequests)
			return true;

		dq.push_back(now);
		return false;
	}

private:
	using Clock = std::chrono::steady_clock;
	using TimePoint = Clock::time_point;

	[[nodiscard]] size_t shard(const int64_t workspaceKey) const
	{
		return std::hash<int64_t>{}(workspaceKey) % _shards;
	}

	size_t _shards;
	size_t _maxRequests;
	std::chrono::seconds _window;

	std::vector<std::unordered_map<int64_t, std::deque<TimePoint>>> _maps; // sportelli (leggi analogia sopra)
	std::vector<std::mutex> _mutexes; // mutex su ogni sportello
};