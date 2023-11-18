
#ifndef FFMPEGEncoderDaemons_h
#define FFMPEGEncoderDaemons_h

#include "FFMPEGEncoderBase.h"

#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "FFMpeg.h"
#include "catralibraries/GetCpuUsage.h"
#include <string>
#include <chrono>

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif


class FFMPEGEncoderDaemons: public FFMPEGEncoderBase {

	public:
		FFMPEGEncoderDaemons(
			Json::Value configuration,
			mutex* liveRecordingMutex,                                                                            
			vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* liveRecordingsCapability,
			mutex* liveProxyMutex,
			vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>* liveProxiesCapability,
			mutex* cpuUsageMutex,
			deque<int>* cpuUsage,
			shared_ptr<spdlog::logger> logger);
		~FFMPEGEncoderDaemons();

		void startMonitorThread();

		void stopMonitorThread();

		void startCPUUsageThread();

		void stopCPUUsageThread();

	private:
		bool		_monitorThreadShutdown;
		bool		_cpuUsageThreadShutdown;
		int			_monitorCheckInSeconds;

		GetCpuUsage_t	_getCpuUsage;

		mutex*		_liveRecordingMutex;
		vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>* _liveRecordingsCapability;
		mutex*		_liveProxyMutex;
		vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>*	_liveProxiesCapability;
		mutex*			_cpuUsageMutex;
		deque<int>*		_cpuUsage;
};

#endif
