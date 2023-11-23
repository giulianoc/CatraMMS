
#include "ThreadsStatistic.h"
#include <sstream>

ThreadsStatistic::ThreadStatistic::ThreadStatistic(
	shared_ptr<ThreadsStatistic> mmsThreadsStatistic,
	string threadName,
	int processorIdentifier,
	int currentThreadsNumber,
	int64_t ingestionJobKey)
{
	try
	{
		_mmsThreadsStatistic = mmsThreadsStatistic;
		_threadId = this_thread::get_id();

		ThreadData threadData;
		threadData._threadName = threadName;
		threadData._processorIdentifier = processorIdentifier;
		threadData._currentThreadsNumber = currentThreadsNumber;
		threadData._ingestionJobKey = ingestionJobKey;
		threadData._startThread = chrono::system_clock::now();

		_mmsThreadsStatistic->addThread(_threadId, threadData);
	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

ThreadsStatistic::ThreadStatistic::~ThreadStatistic()
{
	try
	{
		_mmsThreadsStatistic->removeThread(_threadId);
	}
	catch(runtime_error& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception& e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

ThreadsStatistic::ThreadsStatistic(
	// shared_ptr<spdlog::logger> logger
)
{
	// _logger = logger;
}

void ThreadsStatistic::addThread(thread::id threadId, ThreadData threadData)
{
	try
	{
		stringstream ss;
		ss << threadId;
		string sThreadId = ss.str();

		lock_guard<mutex> locker(_runningThreadsMutex);

		map<string, ThreadData>::iterator it = _runningThreads.find(sThreadId);
		if (it != _runningThreads.end())
		{
			SPDLOG_ERROR("threadsStatistic: thread already added"
				", threadId: {}", sThreadId);

			logRunningThreads(true);

			return;
		}

		_runningThreads.insert(make_pair(sThreadId, threadData));
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("threadsStatistic addThread failed"
			", exception: {}", e.what());
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("threadsStatistic addThread failed"
			", exception: {}", e.what());
	}
}

void ThreadsStatistic::removeThread(thread::id threadId)
{
	try
	{
		stringstream ss;
		ss << threadId;
		string sThreadId = ss.str();

		lock_guard<mutex> locker(_runningThreadsMutex);

		map<string, ThreadData>::iterator it = _runningThreads.find(sThreadId);
		if (it == _runningThreads.end())
		{
			SPDLOG_ERROR("threadsStatistic: thread not found"
				", threadId: {}", sThreadId);

			logRunningThreads(true);

			return;
		}

		ThreadData threadData = (*it).second;
		_runningThreads.erase(it);

		SPDLOG_INFO("threadsStatistic"
			", threadName: {}"
			", processorIdentifier: {}"
			", currentThreadsNumber: {}"
			", ingestionJobKey: {}"
			", @MMS statistics@ - threadDuration (secs): @{}@",
			threadData._threadName, threadData._processorIdentifier, threadData._currentThreadsNumber,
			threadData._ingestionJobKey, chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - threadData._startThread).count()
		);
	}
	catch(runtime_error& e)
	{
		SPDLOG_ERROR("threadsStatistic removeThread failed"
			", exception: {}", e.what());
	}
	catch(exception& e)
	{
		SPDLOG_ERROR("threadsStatistic removeThread failed"
			", exception: ", e.what());
	}
}

void ThreadsStatistic::logRunningThreads(bool asError)
{
	try
	{
		lock_guard<mutex> locker(_runningThreadsMutex);

		string message = to_string(_runningThreads.size()) + ". ";
		bool firstThreadData = true;
		int threadCounter = 1;
		for (map<string, ThreadData>::iterator it = _runningThreads.begin();
			it != _runningThreads.end(); it++)
		{
			string sThreadId = it->first;
			// stringstream ss;
			// ss << (*it).first;
			// string sThreadId = ss.str();

			ThreadData threadData = it->second;

			if (!firstThreadData)
			{
				message += ", ";

				firstThreadData = false;
			}

			message += fmt::format(
				// to_string(threadCounter++) + "-"
				"{}. {}-{}-{}",
				threadData._currentThreadsNumber,
				sThreadId,
				threadData._threadName,
				chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - threadData._startThread).count()
				);
		}
		if (asError)
			SPDLOG_ERROR("threadsStatistic, running threads: {}", message);
		else
			SPDLOG_INFO("threadsStatistic, running threads: {}", message);
	}
	catch(runtime_error& e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic logRunningThreads failed"
			+ ", exception: " + e.what()
		);
	}
	catch(exception& e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic logRunningThreads failed"
			+ ", exception: " + e.what()
		);
	}
}

