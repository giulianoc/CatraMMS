/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   API.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#pragma once

#include <chrono>
#include <unordered_map>

using namespace std;

class BandwidthStats
{
  public:
	void addSample(uint64_t bytesUsed, chrono::system_clock::time_point timestamp);

  private:
	// il mutex non serve visto che un solo thread gestisce la bandwidthUsage
	// mutex _mutex;

	string _currentDay;
	int _currentHour;

	unordered_map<int, vector<uint64_t>> _hourlyData; // hour → samples
	uint64_t _dailyPeak = 0;
	chrono::system_clock::time_point _dailyPeakTime;
	vector<pair<chrono::system_clock::time_point, uint64_t>> _dailySamples;

	void logAndReset();
};
