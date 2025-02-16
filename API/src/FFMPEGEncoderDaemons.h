
#ifndef FFMPEGEncoderDaemons_h
#define FFMPEGEncoderDaemons_h

#include "FFMPEGEncoderBase.h"

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "FFMpeg.h"
#include "catralibraries/GetCpuUsage.h"
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
		json configurationRoot, mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,
		mutex *liveProxyMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability, mutex *cpuUsageMutex,
		deque<int> *cpuUsage
	);
	~FFMPEGEncoderDaemons();

	void startMonitorThread();

	void stopMonitorThread();

	void startCPUUsageThread();

	void stopCPUUsageThread();

  private:
	bool _monitorThreadShutdown;
	bool _cpuUsageThreadShutdown;
	int _monitorCheckInSeconds;

	int _maxRealTimeInfoNotChangedToleranceInSeconds;
	int _maxRealTimeInfoTimestampDiscontinuity;
	GetCpuUsage_t _getCpuUsage;

	mutex *_liveRecordingMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *_liveRecordingsCapability;
	mutex *_liveProxyMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *_liveProxiesCapability;
	mutex *_cpuUsageMutex;
	deque<int> *_cpuUsage;

	void termProcess(shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding, int64_t ingestionJobKey, string label, string message, bool kill);
};

#endif
