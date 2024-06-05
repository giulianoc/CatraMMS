
#ifndef StatisticTimer_h
#define StatisticTimer_h

#include <chrono>
#include <map>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
using namespace nlohmann::literals;

using namespace std;

class StatisticTimer
{

  public:
	StatisticTimer(string name);
	void start(string label);
	chrono::system_clock::duration stop(string label);
	string toString();
	json toJson();

  private:
	string _name;

	map<string, chrono::system_clock::time_point> _uncompletedTimers;

	vector<tuple<chrono::system_clock::time_point, chrono::system_clock::time_point, string>> _timers;
};

#endif
