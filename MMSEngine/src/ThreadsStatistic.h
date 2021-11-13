
#include "spdlog/spdlog.h"
#include <string>
#include <chrono>
#include <thread>
#include <map>

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;

struct ThreadData
{
	string							_threadName;
	int								_processorIdentifier;
	int								_currentThreadsNumber;
	int64_t							_ingestionJobKey;
	chrono::system_clock::time_point	_startThread;
};

class ThreadsStatistic {

	public:
	class ThreadStatistic
	{
		shared_ptr<ThreadsStatistic>	_mmsThreadsStatistic;
		thread::id						_threadId;

		public:
		ThreadStatistic(
			shared_ptr<ThreadsStatistic> mmsThreadsStatistic,
			string threadName,
			int processorIdentifier,
			int currentThreadsNumber,
			int64_t ingestionJobKey);

		~ThreadStatistic();
	};

	public:
		ThreadsStatistic(shared_ptr<spdlog::logger> logger);

		void addThread(thread::id threadId, ThreadData threadData);
		void removeThread(thread::id threadId);

		void logRunningThreads();

	private:
		mutex							_runningThreadsMutex;
		map<thread::id, ThreadData>		_runningThreads;
		shared_ptr<spdlog::logger>		_logger;
};

