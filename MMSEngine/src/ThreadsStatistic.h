
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include <string>
#include <chrono>
#include <thread>
#include <map>

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ std::string("[") + std::string(__FILE__).substr(std::string(__FILE__).find_last_of("/") + 1) + ":" + to_std::string(__LINE__) + "] "
    #else
        #define __FILEREF__ std::string("[") + basename((char *) __FILE__) + ":" + to_std::string(__LINE__) + "] "
    #endif
#endif

// using namespace std;

struct ThreadData
{
	std::string							_threadName;
	int								_processorIdentifier;
	int								_currentThreadsNumber;
	int64_t							_ingestionJobKey;
	std::chrono::system_clock::time_point	_startThread;
};

class ThreadsStatistic {

	public:
	class ThreadStatistic
	{
		std::shared_ptr<ThreadsStatistic>	_mmsThreadsStatistic;
		std::thread::id						_threadId;

		public:
		ThreadStatistic(
			std::shared_ptr<ThreadsStatistic> mmsThreadsStatistic,
			std::string threadName,
			int processorIdentifier,
			int currentThreadsNumber,
			int64_t ingestionJobKey);

		~ThreadStatistic();
	};

	public:
		ThreadsStatistic(std::shared_ptr<spdlog::logger> logger);

		void addThread(std::thread::id threadId, ThreadData threadData);
		void removeThread(std::thread::id threadId);

		void logRunningThreads(bool asError = false);

	private:
		std::mutex							_runningThreadsMutex;
		std::map<std::string, ThreadData>			_runningThreads;
		std::shared_ptr<spdlog::logger>		_logger;
};

