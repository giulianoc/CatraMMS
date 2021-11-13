
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
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic addThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
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
	catch(runtime_error e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
	catch(exception e)
	{
		// _logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
		// 	+ ", exception: " + e.what()
		// );
	}
}

ThreadsStatistic::ThreadsStatistic(
	shared_ptr<spdlog::logger> logger)
{
	_logger = logger;
}

void ThreadsStatistic::addThread(thread::id threadId, ThreadData threadData)
{
	try
	{
		lock_guard<mutex> locker(_runningThreadsMutex);

		map<thread::id, ThreadData>::iterator it = _runningThreads.find(threadId);
		if (it != _runningThreads.end())
		{
			stringstream ss;
			ss << threadId;
			string sThreadId = ss.str();

			string message = __FILEREF__ + "threadsStatistic: thread already added"
				+ ", threadId: " + sThreadId
			;
			_logger->error(message);

			return;
		}

		_runningThreads.insert(make_pair(threadId, threadData));
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic addThread failed"
			+ ", exception: " + e.what()
		);
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic addThread failed"
			+ ", exception: " + e.what()
		);
	}
}

void ThreadsStatistic::removeThread(thread::id threadId)
{
	try
	{
		lock_guard<mutex> locker(_runningThreadsMutex);

		map<thread::id, ThreadData>::iterator it = _runningThreads.find(threadId);
		if (it == _runningThreads.end())
		{
			stringstream ss;
			ss << threadId;
			string sThreadId = ss.str();

			string message = __FILEREF__ + "threadsStatistic: thread not found"
				+ ", threadId: " + sThreadId
			;
			_logger->error(message);

			return;
		}

		ThreadData threadData = (*it).second;
		_runningThreads.erase(it);

		string message = __FILEREF__ + "threadsStatistic"
			+ ", threadName: " + threadData._threadName
			+ ", processorIdentifier: " + to_string(threadData._processorIdentifier)
			+ ", currentThreadsNumber: " + to_string(threadData._currentThreadsNumber)
			+ ", ingestionJobKey: " + to_string(threadData._ingestionJobKey)
			+ ", @MMS statistics@ - threadDuration (secs): @"
				+ to_string(chrono::duration_cast<chrono::seconds>(
				chrono::system_clock::now() - threadData._startThread).count()) + "@"
		;
		_logger->info(message);
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
			+ ", exception: " + e.what()
		);
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic removeThread failed"
			+ ", exception: " + e.what()
		);
	}
}

void ThreadsStatistic::logRunningThreads()
{
	try
	{
		lock_guard<mutex> locker(_runningThreadsMutex);

		string message = to_string(_runningThreads.size()) + ". ";
		bool firstThreadData = true;
		int threadCounter = 1;
		for (map<thread::id, ThreadData>::iterator it = _runningThreads.begin();
			it != _runningThreads.end(); it++)
		{
			stringstream ss;
			ss << (*it).first;
			string sThreadId = ss.str();

			ThreadData threadData = (*it).second;

			if (!firstThreadData)
			{
				message += ", ";

				firstThreadData = false;
			}

			message += (
				// to_string(threadCounter++) + "-"
				to_string(threadData._currentThreadsNumber) +  ". "
				+ sThreadId + "-"
				+ threadData._threadName +  "-"
				+ to_string(chrono::duration_cast<chrono::seconds>(
					chrono::system_clock::now() - threadData._startThread).count())
				)
			;
		}
		_logger->info(__FILEREF__ + "threadsStatistic, running threads: "
			+ message);
	}
	catch(runtime_error e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic logRunningThreads failed"
			+ ", exception: " + e.what()
		);
	}
	catch(exception e)
	{
		_logger->error(__FILEREF__ + "threadsStatistic logRunningThreads failed"
			+ ", exception: " + e.what()
		);
	}
}

