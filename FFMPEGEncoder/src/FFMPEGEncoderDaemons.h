
#pragma once

#include "FFMPEGEncoderBase.h"

#include <shared_mutex>

#include "FFMpegWrapper.h"
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
		std::mutex *liveProxyMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability
	);
	~FFMPEGEncoderDaemons();

	void startMonitorThread();

	void stopMonitorThread();

	static void termProcess(const std::shared_ptr<FFMPEGEncoderBase::Encoding> &selectedEncoding, int64_t ingestionJobKey, std::string label, std::string message, bool kill);

private:
	bool _monitorThreadShutdown;
	int _monitorCheckInSeconds;

	int _maxRealTimeInfoNotChangedToleranceInSeconds;
	int _maxRealTimeInfoTimestampDiscontinuitiesInTimeWindow;

	std::mutex *_liveRecordingMutex;
	std::vector<std::shared_ptr<LiveRecording>> *_liveRecordingsCapability;
	std::mutex *_liveProxyMutex;
	std::vector<std::shared_ptr<LiveProxyAndGrid>> *_liveProxiesCapability;

	static bool exists(const std::string& pathName, int retries = 3, int waitInSeconds = 1);
};
