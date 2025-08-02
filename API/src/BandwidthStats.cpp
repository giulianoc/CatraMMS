
#include "BandwidthStats.h"
#include "Datetime.h"
#include "spdlog/spdlog.h"

void BandwidthStats::addSample(uint64_t bytesUsed, chrono::system_clock::time_point timestamp)
{
	// lock_guard<std::mutex> lock(_mutex);

	time_t timestamp_t = chrono::system_clock::to_time_t(timestamp);

	string day = Datetime::utcToLocalString(timestamp_t, Datetime::Format::YYYY_MM_DD);
	tm local_tm = Datetime::utcSecondsToLocalTime(timestamp_t);

	if (_currentDay.empty())
	{
		_currentDay = day;
		_currentHour = local_tm.tm_hour;
	}
	else if (day != _currentDay)
	{
		logAndReset();
		_currentDay = day;
		_currentHour = local_tm.tm_hour;
	}
	else if (local_tm.tm_hour != _currentHour)
	{
		const auto &samples = _hourlyData[_currentHour];

		_currentHour = local_tm.tm_hour;

		if (!samples.empty())
		{
			uint64_t sum = 0;
			uint64_t peak = 0;
			for (auto v : samples)
			{
				sum += v;
				if (v > peak)
					peak = v;
			}

			double avg = static_cast<double>(sum) / samples.size();
			SPDLOG_INFO(
				"BandwidthStats (Mbps). Day: {}, Hour {}: Peak={} Avg={}", _currentDay, _currentHour, (peak * 8) / 1000000, (avg * 8) / 1000000
			);
		}
	}

	_hourlyData[local_tm.tm_hour].push_back(bytesUsed);
	_dailySamples.emplace_back(timestamp, bytesUsed);

	if (bytesUsed > _dailyPeak)
	{
		_dailyPeak = bytesUsed;
		_dailyPeakTime = timestamp;
	}
}

void BandwidthStats::logAndReset()
{
	for (int h = 0; h < 24; ++h)
	{
		const auto &samples = _hourlyData[h];
		if (samples.empty())
			continue;

		uint64_t sum = 0;
		uint64_t peak = 0;
		for (auto v : samples)
		{
			sum += v;
			if (v > peak)
				peak = v;
		}

		double avg = static_cast<double>(sum) / samples.size();
		SPDLOG_INFO("BandwidthStats (Mbps). Day: {}, Hour {}: Peak={} Avg={}", _currentDay, h, (peak * 8) / 1000000, (avg * 8) / 1000000);
	}

	if (!_dailySamples.empty())
	{
		uint64_t total = 0;
		for (const auto &[_, val] : _dailySamples)
			total += val;
		double avg = static_cast<double>(total) / _dailySamples.size();

		SPDLOG_INFO(
			"BandwidthStats (Mbps). Day: {}: Daily Peak={} at {}, Daily Avg={}", _currentDay, (_dailyPeak * 8) / 1000000,
			Datetime::utcToLocalString(chrono::system_clock::to_time_t(_dailyPeakTime)), (avg * 8) / 1000000
		);
	}

	// Reset
	_hourlyData.clear();
	_dailySamples.clear();
	_dailyPeak = 0;
	_dailyPeakTime = {};
}
