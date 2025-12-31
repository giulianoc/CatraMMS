
#pragma once

#include "FFMPEGEncoderBase.h"

#include <shared_mutex>

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "FFMpegWrapper.h"
#include "GetCpuUsage.h"
#include "spdlog/spdlog.h"
#include <chrono>
#include <deque>
#include <string>

#ifndef __FILEREF__
#ifdef __APPLE__
#define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
#else
#define __FILEREF__ string("[") + basename((char *)__FILE__) + ":" + to_string(__LINE__) + "] "
#endif
#endif

class FFMPEGEncoderDaemons : public FFMPEGEncoderBase
{

  public:
	FFMPEGEncoderDaemons(
		nlohmann::json configurationRoot, std::mutex *liveRecordingMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,
		std::mutex *liveProxyMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability, std::shared_mutex *cpuUsageMutex,
		std::deque<int> *cpuUsage
	);
	~FFMPEGEncoderDaemons();

	void startMonitorThread();

	void stopMonitorThread();

	void startCPUUsageThread();

	void stopCPUUsageThread();

	static void termProcess(const std::shared_ptr<FFMPEGEncoderBase::Encoding> &selectedEncoding, int64_t ingestionJobKey, std::string label, std::string message, bool kill);

private:
	bool _monitorThreadShutdown;
	bool _cpuUsageThreadShutdown;
	int _monitorCheckInSeconds;

	int _maxRealTimeInfoNotChangedToleranceInSeconds;
	int _maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow;
	GetCpuUsage _getCpuUsage;

	std::mutex *_liveRecordingMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *_liveRecordingsCapability;
	std::mutex *_liveProxyMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *_liveProxiesCapability;
	std::shared_mutex *_cpuUsageMutex;
	std::deque<int> *_cpuUsage;

	static bool exists(const std::string& pathName, int retries = 3, int waitInSeconds = 1);
};
