
#include "StatisticTimer.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <numeric>

StatisticTimer::StatisticTimer(string name) { _name = name; }

void StatisticTimer::start(string label)
{
	auto it = _uncompletedTimers.find(label);
	if (it != _uncompletedTimers.end())
		SPDLOG_ERROR("StatisticTimer ({}): start cannot be done because the label ({}) is already present", _name, label);
	else
		_uncompletedTimers.insert(make_pair(label, chrono::system_clock::now()));
}

chrono::system_clock::duration StatisticTimer::stop(string label)
{
	chrono::system_clock::duration d(0);

	auto it = _uncompletedTimers.find(label);
	if (it == _uncompletedTimers.end())
		SPDLOG_ERROR("StatisticTimer ({}): stop cannot be done because the label ({}) is not present", _name, label);
	else
	{
		chrono::system_clock::time_point start = it->second;
		chrono::system_clock::time_point end = chrono::system_clock::now();
		d = end - start;
		_timers.push_back(make_tuple(start, end, label));
		_uncompletedTimers.erase(it);
	}

	return d;
}

string StatisticTimer::toString()
{
	if (_uncompletedTimers.size() > 0)
		SPDLOG_ERROR(
			"StatisticTimer ({}) has {} timers not stopped: {}", _name, _uncompletedTimers.size(),
			accumulate(
				begin(_uncompletedTimers), end(_uncompletedTimers), string(),
				[](const string &s, pair<string, chrono::system_clock::time_point> timer)
				{ return (s == "" ? timer.first : (s + ", " + timer.first)); }
			)
		);

	string log;
	for (tuple<chrono::system_clock::time_point, chrono::system_clock::time_point, string> timer : _timers)
	{
		auto [start, stop, label] = timer;
		if (log != "")
			log += ", ";
		log += fmt::format("{}: {}", label, chrono::duration_cast<chrono::milliseconds>(stop - start).count());
	}
	log = fmt::format("statistics ({}): {}", _name, log);

	return log;
}

json StatisticTimer::toJson()
{
	if (_uncompletedTimers.size() > 0)
		SPDLOG_ERROR(
			"StatisticTimer ({}) has {} timers not stopped: {}", _name, _uncompletedTimers.size(),
			accumulate(
				begin(_uncompletedTimers), end(_uncompletedTimers), string(),
				[](const string &s, pair<string, chrono::system_clock::time_point> timer)
				{ return (s == "" ? timer.first : (s + ", " + timer.first)); }
			)
		);

	json statisticsRoot = json::array();
	for (tuple<chrono::system_clock::time_point, chrono::system_clock::time_point, string> timer : _timers)
	{
		auto [start, stop, label] = timer;

		statisticsRoot.push_back(fmt::format("{}: {} millisecs", label, chrono::duration_cast<chrono::milliseconds>(stop - start).count()));
	}

	json root;
	root[_name] = statisticsRoot;

	return root;
}
