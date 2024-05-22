
#ifndef StatisticTimer_h
#define StatisticTimer_h

#include <string>
#include <chrono>
#include <vector>
#include <map>

using namespace std;                                                                                          

class StatisticTimer {

public:
	StatisticTimer(string name);
	void start(string label);
	void stop(string label);
	string toString();

private:
	string _name;

	map<string, chrono::system_clock::time_point> _uncompletedTimers;

	vector<tuple<chrono::system_clock::time_point, chrono::system_clock::time_point, string>> _timers;
};

#endif

