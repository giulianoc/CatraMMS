
#include "StatisticTimer.h"
#include "spdlog/spdlog.h"
#include <numeric>


StatisticTimer::StatisticTimer(string name)
{
	_name = name;
}

void StatisticTimer::start(string label)
{
	auto it = _uncompletedTimers.find(label);
    if (it != _uncompletedTimers.end())
		SPDLOG_ERROR("StatisticTimer ({}): start cannot be done because the label ({}) is already present",
			_name, label);
    else
		_uncompletedTimers.insert(make_pair(label, chrono::system_clock::now()));
}

void StatisticTimer::stop(string label)
{
	auto it = _uncompletedTimers.find(label);
    if (it == _uncompletedTimers.end())
		SPDLOG_ERROR("StatisticTimer ({}): stop cannot be done because the label ({}) is not present",
			_name, label);
    else
	{
		_timers.push_back(make_tuple(it->second, chrono::system_clock::now(), label));
		_uncompletedTimers.erase(it);
	}
}

string StatisticTimer::toString()
{
	if (_uncompletedTimers.size() > 0)
		SPDLOG_ERROR("StatisticTimer ({}) has {} timers not stopped: {}",
			_name, _uncompletedTimers.size(),
			accumulate(begin(_uncompletedTimers), end(_uncompletedTimers), string(),
				[](const string&s, pair<string, chrono::system_clock::time_point> timer)
				{
					return (s == "" ? timer.first : (s + ", " + timer.first));
				})
		);

	string log;
	for(tuple<chrono::system_clock::time_point, chrono::system_clock::time_point, string> timer: _timers)
	{
		auto [start, stop, label] = timer;
		if (log != "")
			log += ", ";
		log += fmt::format("{}: {}", label, chrono::duration_cast<chrono::milliseconds>(stop - start).count());
	}
	log = fmt::format("statistics ({}): {}", _name, log);

	return log;
}

