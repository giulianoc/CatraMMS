
#include "MMSEngineProcessor.h"
#include "StatisticTimer.h"

using namespace std;
using json = nlohmann::json;

void MMSEngineProcessor::handleContentRetentionEventThread(const shared_ptr<long>& processorsThreadsNumber) const
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleContentRetentionEventThread", _processorIdentifier,
		_processorsThreadsNumber.use_count(), -1 // ingestionJobKey
	);

	LOG_INFO("handleContentRetentionEventThread"
		", _processorIdentifier: {}"
		", _processorsThreadsNumber.use_count(): {}", _processorIdentifier, _processorsThreadsNumber.use_count()
	);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	{
		vector<tuple<shared_ptr<Workspace>, int64_t, int64_t>> mediaItemKeyOrPhysicalPathKeyToBeRemoved;
		bool moreRemoveToBeDone = true;

		while (moreRemoveToBeDone)
		{
			try
			{
				const int maxMediaItemKeysNumber = 100;

				mediaItemKeyOrPhysicalPathKeyToBeRemoved.clear();
				_mmsEngineDBFacade->getExpiredMediaItemKeysCheckingDependencies(
					_processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved, maxMediaItemKeysNumber
				);

				if (mediaItemKeyOrPhysicalPathKeyToBeRemoved.empty())
					moreRemoveToBeDone = false;
			}
			catch (exception &e)
			{
				LOG_ERROR("getExpiredMediaItemKeysCheckingDependencies failed"
					", _processorIdentifier: {}"
					", exception: {}", _processorIdentifier, e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
				break;
			}

			for (const auto& [workspace, mediaItemKey, physicalPathKey] :
				mediaItemKeyOrPhysicalPathKeyToBeRemoved)
			{
				LOG_INFO("Removing because of ContentRetention"
					", mediaItemKey: {}"
					", physicalPathKey: {}"
					", _processorIdentifier: {}"
					", workspaceKey: {}"
					", workspace->_name: {}", mediaItemKey, physicalPathKey, _processorIdentifier, workspace->_workspaceKey,
					workspace->_name
				);

				try
				{
					if (physicalPathKey == -1)
						_mmsStorage->removeMediaItem(mediaItemKey);
					else
						_mmsStorage->removePhysicalPath(physicalPathKey);
				}
				catch (exception &e)
				{
					LOG_ERROR("_mmsStorage->removeMediaItem failed"
						", mediaItemKey: {}"
						", physicalPathKey: {}"
						", _processorIdentifier: {}"
						", workspaceKey: {}"
						", workspace->_name: {}"
						", mediaItemKeyToBeRemoved: {}"
						", physicalPathKeyToBeRemoved: {}", mediaItemKey, physicalPathKey, _processorIdentifier, workspace->_workspaceKey,
						workspace->_name, mediaItemKey, physicalPathKey
					);

					try
					{
						_mmsEngineDBFacade->updateMediaItem(mediaItemKey, "");
					}
					catch (exception &e)
					{
						LOG_ERROR("updateMediaItem failed"
							", mediaItemKey: {}"
							", physicalPathKey: {}"
							", _processorIdentifier: {}"
							", mediaItemKeyToBeRemoved: {}"
							", physicalPathKeyToBeRemoved: {}"
							", exception: {}", mediaItemKey, physicalPathKey, _processorIdentifier, mediaItemKey, physicalPathKey, e.what()
						);
					}

					// one remove failed, procedure has to go ahead to try all
					// the other removes moreRemoveToBeDone = false; break;

					continue;
					// no throw since it is running in a detached thread
					// throw e;
				}
			}
		}

		chrono::system_clock::time_point end = chrono::system_clock::now();
		LOG_INFO("Content retention finished"
			", _processorIdentifier: {}"
			", @MMS statistics@ - duration (secs): @{}@",
			_processorIdentifier, chrono::duration_cast<chrono::seconds>(end - start).count()
		);
	}
}

void MMSEngineProcessor::handleDBDataRetentionEventThread()
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleDBDataRetentionEventThread", _processorIdentifier,
		_processorsThreadsNumber.use_count(), -1 // ingestionJobKey,
	);

	bool alreadyExecuted = true;

	StatisticTimer statisticTimer("handleDBDataRetentionEventThread");

	try
	{
		LOG_INFO("DBDataRetention: onceExecution"
			", _processorIdentifier: {}",
			_processorIdentifier
		);

		alreadyExecuted = _mmsEngineDBFacade->onceExecution(MMSEngineDBFacade::OnceType::DBDataRetention);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"DBDataRetention: onceExecution failed"
			", _processorIdentifier: {}"
			", exception: {}",
			_processorIdentifier, e.what()
		);

		// no throw since it is running in a detached thread
		// throw e;
	}

	if (!alreadyExecuted)
	{
		try
		{
			LOG_INFO(
				"DBDataRetention: retentionOfIngestionData"
				", _processorIdentifier: {}",
				_processorIdentifier
			);
			statisticTimer.start("retentionOfIngestionData");
			_mmsEngineDBFacade->retentionOfIngestionData();
			statisticTimer.stop("retentionOfIngestionData");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"DBDataRetention: retentionOfIngestionData failed"
				", _processorIdentifier: {}"
				", exception: {}",
				_processorIdentifier, e.what()
			);

			statisticTimer.stop("retentionOfIngestionData");

			// no throw since it is running in a detached thread
			// throw e;
		}

		try
		{
			LOG_INFO(
				"DBDataRetention: retentionOfDeliveryAuthorization"
				", _processorIdentifier: {}",
				_processorIdentifier
			);
			statisticTimer.start("retentionOfDeliveryAuthorization");
			_mmsEngineDBFacade->retentionOfDeliveryAuthorization();
			statisticTimer.stop("retentionOfDeliveryAuthorization");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"DBDataRetention: retentionOfDeliveryAuthorization failed"
				", _processorIdentifier: {}"
				", exception: {}",
				_processorIdentifier, e.what()
			);

			statisticTimer.stop("retentionOfDeliveryAuthorization");

			// no throw since it is running in a detached thread
			// throw e;
		}

		try
		{
			LOG_INFO(
				"DBDataRetention: retentionOfStatisticData"
				", _processorIdentifier: {}",
				_processorIdentifier
			);
			statisticTimer.start("retentionOfStatisticData");
			_mmsEngineDBFacade->retentionOfStatisticData();
			statisticTimer.stop("retentionOfStatisticData");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"DBDataRetention: retentionOfStatisticData failed"
				", _processorIdentifier: {}"
				", exception: {}",
				_processorIdentifier, e.what()
			);

			statisticTimer.stop("retentionOfStatisticData");

			// no throw since it is running in a detached thread
			// throw e;
		}

		try
		{
			// Scenarios: IngestionJob in final status but EncodingJob not
			// in final status
			LOG_INFO(
				"DBDataRetention: fixEncodingJobsHavingWrongStatus"
				", _processorIdentifier: {}",
				_processorIdentifier
			);
			statisticTimer.start("fixEncodingJobsHavingWrongStatus");
			_mmsEngineDBFacade->fixEncodingJobsHavingWrongStatus();
			statisticTimer.stop("fixEncodingJobsHavingWrongStatus");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"DBDataRetention: fixEncodingJobsHavingWrongStatus failed"
				", _processorIdentifier: {}"
				", exception: {}",
				_processorIdentifier, e.what()
			);

			statisticTimer.stop("fixEncodingJobsHavingWrongStatus");

			// no throw since it is running in a detached thread
			// throw e;
		}

		try
		{
			LOG_INFO(
				"DBDataRetention: fixIngestionJobsHavingWrongStatus"
				", _processorIdentifier: {}",
				_processorIdentifier
			);
			// Scenarios: EncodingJob in final status but IngestionJob not
			// in final status
			//		even it it was passed long time
			statisticTimer.start("fixIngestionJobsHavingWrongStatus");
			_mmsEngineDBFacade->fixIngestionJobsHavingWrongStatus();
			statisticTimer.stop("fixIngestionJobsHavingWrongStatus");
		}
		catch (exception &e)
		{
			LOG_ERROR(
				"DBDataRetention: fixIngestionJobsHavingWrongStatus failed"
				", _processorIdentifier: {}"
				", exception: {}",
				_processorIdentifier, e.what()
			);

			statisticTimer.stop("fixIngestionJobsHavingWrongStatus");

			// no throw since it is running in a detached thread
			// throw e;
		}
	}
	LOG_INFO(
		"DBDataRetention"
		", _processorIdentifier: {}"
		", alreadyExecuted: {}"
		", statisticTimer: {}",
		_processorIdentifier, alreadyExecuted, statisticTimer.toString()
	);
}
