
#include "MMSEngineProcessor.h"
/*
#include <stdio.h>

#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "FFMpeg.h"
#include "GEOInfoTimes.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "PersistenceLock.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

void MMSEngineProcessor::handleContentRetentionEventThread(shared_ptr<long> processorsThreadsNumber)
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleContentRetentionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey
	);

	SPDLOG_INFO(
		string() + "handleContentRetentionEventThread" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count())
	);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	{
		vector<tuple<shared_ptr<Workspace>, int64_t, int64_t>> mediaItemKeyOrPhysicalPathKeyToBeRemoved;
		bool moreRemoveToBeDone = true;

		while (moreRemoveToBeDone)
		{
			try
			{
				int maxMediaItemKeysNumber = 100;

				mediaItemKeyOrPhysicalPathKeyToBeRemoved.clear();
				_mmsEngineDBFacade->getExpiredMediaItemKeysCheckingDependencies(
					_processorMMS, mediaItemKeyOrPhysicalPathKeyToBeRemoved, maxMediaItemKeysNumber
				);

				if (mediaItemKeyOrPhysicalPathKeyToBeRemoved.size() == 0)
					moreRemoveToBeDone = false;
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "getExpiredMediaItemKeysCheckingDependencies failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
				break;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "getExpiredMediaItemKeysCheckingDependencies failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
				break;
			}

			for (tuple<shared_ptr<Workspace>, int64_t, int64_t> workspaceMediaItemKeyOrPhysicalPathKey : mediaItemKeyOrPhysicalPathKeyToBeRemoved)
			{
				shared_ptr<Workspace> workspace;
				int64_t mediaItemKey;
				int64_t physicalPathKey;

				tie(workspace, mediaItemKey, physicalPathKey) = workspaceMediaItemKeyOrPhysicalPathKey;

				SPDLOG_INFO(
					string() + "Removing because of ContentRetention" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
					", mediaItemKey: " + to_string(mediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
				);

				try
				{
					if (physicalPathKey == -1)
						_mmsStorage->removeMediaItem(mediaItemKey);
					else
						_mmsStorage->removePhysicalPath(physicalPathKey);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						string() + "_mmsStorage->removeMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
						", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
						", exception: " + e.what()
					);

					try
					{
						string processorMMSForRetention = "";
						_mmsEngineDBFacade->updateMediaItem(mediaItemKey, processorMMSForRetention);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}

					// one remove failed, procedure has to go ahead to try all
					// the other removes moreRemoveToBeDone = false; break;

					continue;
					// no throw since it is running in a detached thread
					// throw e;
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						string() + "_mmsStorage->removeMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
						", workspace->_workspaceKey: " + to_string(workspace->_workspaceKey) + ", workspace->_name: " + workspace->_name +
						", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey)
					);

					try
					{
						string processorMMSForRetention = "";
						_mmsEngineDBFacade->updateMediaItem(mediaItemKey, processorMMSForRetention);
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							string() + "updateMediaItem failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							", mediaItemKeyToBeRemoved: " + to_string(mediaItemKey) + ", physicalPathKeyToBeRemoved: " + to_string(physicalPathKey) +
							", exception: " + e.what()
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
		SPDLOG_INFO(
			string() + "Content retention finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
		);
	}

	/* Already done by the crontab script
	{
		SPDLOG_INFO(string() + "Staging Retention started"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
			+ ", _mmsStorage->getStagingRootRepository(): " +
_mmsStorage->getStagingRootRepository()
		);

		try
		{
			chrono::system_clock::time_point tpNow =
chrono::system_clock::now();

			FileIO::DirectoryEntryType_t detDirectoryEntryType;
			shared_ptr<FileIO::Directory> directory = FileIO::openDirectory
(_mmsStorage->getStagingRootRepository());

			bool scanDirectoryFinished = false;
			while (!scanDirectoryFinished)
			{
				string directoryEntry;
				try
				{
					string directoryEntry = FileIO::readDirectory (directory,
						&detDirectoryEntryType);

//                    if (detDirectoryEntryType !=
FileIO::TOOLS_FILEIO_REGULARFILE)
//                        continue;

					string pathName = _mmsStorage->getStagingRootRepository()
							+ directoryEntry;
					chrono::system_clock::time_point tpLastModification =
							FileIO:: getFileTime (pathName);

					int elapsedInHours =
chrono::duration_cast<chrono::hours>(tpNow - tpLastModification).count(); double
elapsedInDays =  elapsedInHours / 24; if (elapsedInDays >=
_stagingRetentionInDays)
					{
						if (detDirectoryEntryType == FileIO::
TOOLS_FILEIO_DIRECTORY)
						{
							SPDLOG_INFO(string() + "Removing staging
directory because of Retention"
								+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
								+ ", pathName: " + pathName
								+ ", elapsedInDays: " + to_string(elapsedInDays)
								+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
							);

							try
							{
								bool removeRecursively = true;

								FileIO::removeDirectory(pathName,
removeRecursively);
							}
							catch(runtime_error& e)
							{
								_logger->warn(string() + "Error removing
staging directory because of Retention"
									+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
									+ ", pathName: " + pathName
									+ ", elapsedInDays: " +
to_string(elapsedInDays)
									+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception& e)
							{
								_logger->warn(string() + "Error removing
staging directory because of Retention"
									+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
									+ ", pathName: " + pathName
									+ ", elapsedInDays: " +
to_string(elapsedInDays)
									+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
									+ ", e.what(): " + e.what()
								);
							}
						}
						else
						{
							SPDLOG_INFO(string() + "Removing staging file
because of Retention"
								+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
								+ ", pathName: " + pathName
								+ ", elapsedInDays: " + to_string(elapsedInDays)
								+ ", _stagingRetentionInDays: " +
to_string(_stagingRetentionInDays)
							);

							bool exceptionInCaseOfError = false;

							FileIO::remove(pathName, exceptionInCaseOfError);
						}
					}
				}
				catch(DirectoryListFinished& e)
				{
					scanDirectoryFinished = true;
				}
				catch(runtime_error& e)
				{
					string errorMessage = string() + "listing directory
failed"
						+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
						   + ", e.what(): " + e.what()
					;
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
				catch(exception& e)
				{
					string errorMessage = string() + "listing directory
failed"
						+ ", _processorIdentifier: " +
to_string(_processorIdentifier)
						   + ", e.what(): " + e.what()
					;
					SPDLOG_ERROR(errorMessage);

					throw e;
				}
			}

			FileIO::closeDirectory (directory);
		}
		catch(runtime_error& e)
		{
			SPDLOG_ERROR(string() + "removeHavingPrefixFileName failed"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
				+ ", e.what(): " + e.what()
			);
		}
		catch(exception& e)
		{
			SPDLOG_ERROR(string() + "removeHavingPrefixFileName failed"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}

		SPDLOG_INFO(string() + "Staging Retention finished"
				+ ", _processorIdentifier: " + to_string(_processorIdentifier)
		);
	}
	*/
}

void MMSEngineProcessor::handleDBDataRetentionEventThread()
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleDBDataRetentionEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey,
	);

	bool alreadyExecuted = true;

	try
	{
		SPDLOG_INFO(string() + "DBDataRetention: oncePerDayExecution" + ", _processorIdentifier: " + to_string(_processorIdentifier));

		alreadyExecuted = _mmsEngineDBFacade->oncePerDayExecution(MMSEngineDBFacade::OncePerDayType::DBDataRetention);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", exception: " + e.what()
		);

		// no throw since it is running in a detached thread
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", exception: " + e.what()
		);

		// no throw since it is running in a detached thread
		// throw e;
	}

	if (!alreadyExecuted)
	{
		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Ingestion Data started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfIngestionData();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Ingestion Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Ingestion Data finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Delivery Autorization started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfDeliveryAuthorization();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Delivery Autorization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Delivery Autorization failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Delivery Autorization finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(string() + "DBDataRetention: Statistic Data started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			try
			{
				_mmsEngineDBFacade->retentionOfStatisticData();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Statistic Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "DBDataRetention: Statistic Data failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() + "DBDataRetention: Statistic Data finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of EncodingJobs having wrong status "
				"started" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				// Scenarios: IngestionJob in final status but EncodingJob not
				// in final status
				_mmsEngineDBFacade->fixEncodingJobsHavingWrongStatus();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of EncodingJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of EncodingJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of EncodingJobs having wrong status "
				"finished" +
				", _processorIdentifier: " + to_string(_processorIdentifier) + ", @MMS statistics@ - duration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}

		{
			chrono::system_clock::time_point start = chrono::system_clock::now();

			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of IngestionJobs having wrong status "
				"started" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				// Scenarios: EncodingJob in final status but IngestionJob not
				// in final status
				//		even it it was passed long time
				_mmsEngineDBFacade->fixIngestionJobsHavingWrongStatus();
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of IngestionJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() +
					"DBDataRetention: Fix of IngestionJobs having wrong status "
					"failed" +
					", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
				);

				// no throw since it is running in a detached thread
				// throw e;
			}

			chrono::system_clock::time_point end = chrono::system_clock::now();
			SPDLOG_INFO(
				string() +
				"DBDataRetention: Fix of IngestionJobs having wrong status "
				"finished" +
				", _processorIdentifier: " + to_string(_processorIdentifier) + ", @MMS statistics@ - duration (secs): @" +
				to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
			);
		}
	}
}
