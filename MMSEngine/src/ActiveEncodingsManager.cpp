/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.cpp
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#include "ActiveEncodingsManager.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "catralibraries/System.h"
#include <fstream>

ActiveEncodingsManager::ActiveEncodingsManager(
	json configuration, string processorMMS, shared_ptr<MultiEventsSet> multiEventsSet, shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
	shared_ptr<MMSStorage> mmsStorage, shared_ptr<spdlog::logger> logger
)
{
	_logger = logger;
	_configuration = configuration;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_mmsStorage = mmsStorage;

	_encodersLoadBalancer = make_shared<EncodersLoadBalancer>(_mmsEngineDBFacade, _configuration, _logger);

	_hostName = System::getHostName();

	_maxSecondsToWaitUpdateEncodingJobLock = JSONUtils::asInt(_configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 0);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", mms->locks->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
	);

	{
		shared_ptr<long> faceRecognitionNumber = make_shared<long>(0);
		int maxFaceRecognitionNumber = JSONUtils::asInt(_configuration["mms"], "maxFaceRecognitionNumber", 0);
		_logger->info(__FILEREF__ + "Configuration item" + ", mms->maxFaceRecognitionNumber: " + to_string(maxFaceRecognitionNumber));
		maxFaceRecognitionNumber += (MAXHIGHENCODINGSTOBEMANAGED + MAXMEDIUMENCODINGSTOBEMANAGED + MAXLOWENCODINGSTOBEMANAGED);

		int lastProxyIdentifier = 0;

		for (EncodingJob &encodingJob : _lowPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber, _logger
			);
		}

		for (EncodingJob &encodingJob : _mediumPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber, _logger
			);
		}

		for (EncodingJob &encodingJob : _highPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber, _logger
			);
		}
	}

	try
	{
		vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> encodingItems;

		_mmsEngineDBFacade->recoverEncodingsNotCompleted(processorMMS, encodingItems);

		SPDLOG_INFO(
			"recoverEncodingsNotCompleted result"
			", _processorMMS: {}"
			", encodingItems.size: {}",
			processorMMS, encodingItems.size()
		);

		addEncodingItems(encodingItems);

		SPDLOG_INFO(
			"addEncodingItems successful"
			", _processorMMS: {}"
			", encodingItems.size: {}",
			processorMMS, encodingItems.size()
		);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(
			"recoverEncodingsNotCompleted failed"
			", _processorMMS: {}"
			", e.what: {}",
			processorMMS, e.what()
		);

		// è un thread/costruttore
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"recoverEncodingsNotCompleted failed"
			", _processorMMS: {}"
			", e.what: {}",
			processorMMS, e.what()
		);

		// è un thread/costruttore
		// throw e;
	}
}

ActiveEncodingsManager::~ActiveEncodingsManager() {}

bool ActiveEncodingsManager::isProcessorShutdown()
{
	string processorShutdownPathName = "/tmp/processorShutdown.txt";

	ifstream f(processorShutdownPathName.c_str());

	return f.good();
}

void ActiveEncodingsManager::operator()()
{
	bool shutdown = false;

	// thread
	// getEncodingsProgressThread(&ActiveEncodingsManager::getEncodingsProgressThread,
	// this); getEncodingsProgressThread.detach();

	chrono::seconds secondsToBlock(5);

	vector<MMSEngineDBFacade::EncodingPriority> sortedEncodingPriorities = {
		MMSEngineDBFacade::EncodingPriority::High, MMSEngineDBFacade::EncodingPriority::Medium, MMSEngineDBFacade::EncodingPriority::Low
	};

	while (!shutdown)
	{
		try
		{
			if (isProcessorShutdown())
			{
				_logger->info(__FILEREF__ + "ActiveEncodingsManager was shutdown");

				shutdown = true;

				continue;
			}

			unique_lock<mutex> locker(_mtEncodingJobs);

			// _logger->info("Reviewing current Encoding Jobs...");

			_cvAddedEncodingJob.wait_for(locker, secondsToBlock);
			/*
			if (_cvAddedEncodingJob.wait_for(locker, secondsToBlock) ==
			cv_status::timeout)
			{
				// time expired

				continue;
			}
			 */

			chrono::system_clock::time_point startEvent = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Begin checking encodingJobs");

			for (MMSEngineDBFacade::EncodingPriority encodingPriority : sortedEncodingPriorities)
			{
				EncodingJob *encodingJobs;
				int maxEncodingsToBeManaged;

				if (encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
				{
					// _logger->info(__FILEREF__ + "Processing the high encodings...");

					encodingJobs = _highPriorityEncodingJobs;
					maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
				}
				else if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Medium)
				{
					// _logger->info(__FILEREF__ + "Processing the default
					// encodings...");

					encodingJobs = _mediumPriorityEncodingJobs;
					maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
				}
				else // if (encodingPriority ==
					 // MMSEngineDBFacade::EncodingPriority::Low)
				{
					// _logger->info(__FILEREF__ + "Processing the low encodings...");

					encodingJobs = _lowPriorityEncodingJobs;
					maxEncodingsToBeManaged = MAXLOWENCODINGSTOBEMANAGED;
				}

				int freeEncodingJobsNumber = 0;
				int runningEncodingJobsNumber = 0;
				int goingToRunEncodingJobsNumber = 0;
				int toBeRunEncodingJobsNumber = 0;
				for (int encodingJobIndex = 0; encodingJobIndex < maxEncodingsToBeManaged; encodingJobIndex++)
				{
					EncodingJob *encodingJob = &(encodingJobs[encodingJobIndex]);

					if (encodingJob->_status == EncoderProxy::EncodingJobStatus::Free)
					{
						freeEncodingJobsNumber++;

						continue;
					}
					else if (encodingJob->_status == EncoderProxy::EncodingJobStatus::Running ||
							 encodingJob->_status == EncoderProxy::EncodingJobStatus::GoingToRun)
					{
						if (encodingJob->_status == EncoderProxy::EncodingJobStatus::Running)
							runningEncodingJobsNumber++;
						else
							goingToRunEncodingJobsNumber++;

						/*
						// We will start to check the encodingProgress after at least XXX
						seconds.
						// This is because the status is set to EncodingJobStatus::Running
						as soon as it is created
						// the encoderVideoAudioProxyThread thread. Many times the thread
						returns soon because
						// of 'No encoding available' and in this case getEncodingProgress
						will return 'No encoding job key found'

						if (chrono::system_clock::now() - encodingJob->_encodingJobStart >=
						chrono::seconds(secondsToBlock))

						2019-03-31: Above commented because it was introduced the
						GoingToRun status
						*/
						/*
						if (encodingJob->_status ==
						EncoderProxy::EncodingJobStatus::Running)
						{
							try
							{
								int encodingPercentage =
									encodingJob->_encoderProxy
									.getEncodingProgress();

								_logger->info(__FILEREF__ + "updateEncodingJobProgress"
										+ ", encodingJobKey: "
											+
						to_string(encodingJob->_encodingItem->_encodingJobKey)
										+ ", encodingPercentage: " +
						to_string(encodingPercentage)
										);
								_mmsEngineDBFacade->updateEncodingJobProgress
						(encodingJob->_encodingItem->_encodingJobKey, encodingPercentage);
							}
							catch(FFMpegEncodingStatusNotAvailable& e)
							{

							}
							catch(runtime_error& e)
							{
								_logger->error(__FILEREF__ + "getEncodingProgress failed"
									+ ", runtime_error: " + e.what()
								);
							}
							catch(exception& e)
							{
								_logger->error(__FILEREF__ + "getEncodingProgress failed");
							}
						}
						*/

						if (encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::LiveRecorder &&
							encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::LiveProxy &&
							encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::VODProxy &&
							chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart) > chrono::hours(24))
						{
							_logger->error(
								__FILEREF__ + "EncodingJob is not finishing" + ", @MMS statistics@ - elapsed (hours): @" +
								to_string(chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count()
								) +
								"@" + ", workspace: " + encodingJob->_encodingItem->_workspace->_name +
								", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
								", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
								", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType) +
								", encodingJob->_status: " + EncoderProxy::toString(encodingJob->_status)
							);
						}
						else
						{
							_logger->info(
								__FILEREF__ + "EncodingJob still running" + ", elapsed (minutes): " +
								to_string(chrono::duration_cast<chrono::minutes>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count()
								) +
								", workspace: " + encodingJob->_encodingItem->_workspace->_name +
								", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
								", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
								", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType) +
								", encodingJob->_status: " + EncoderProxy::toString(encodingJob->_status)
							);
						}
					}
					else // if (encodingJob._status ==
						 // EncoderProxy::EncodingJobStatus::ToBeRun)
					{
						toBeRunEncodingJobsNumber++;

						chrono::system_clock::time_point processingItemStart = chrono::system_clock::now();

						_logger->info(
							__FILEREF__ + "processEncodingJob begin" + ", workspace: " + encodingJob->_encodingItem->_workspace->_name +
							", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
							", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
							", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
							", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
						);

						try
						{
							processEncodingJob(encodingJob);

							_logger->info(
								__FILEREF__ + "processEncodingJob done" + ", @MMS statistics@ - elapsed (seconds): @" +
								to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@" +
								", workspace: " + encodingJob->_encodingItem->_workspace->_name +
								", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
								", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
								", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
							);
						}
						catch (runtime_error &e)
						{
							_logger->error(
								__FILEREF__ + "processEncodingJob failed" + ", @MMS statistics@ - elapsed (seconds): @" +
								to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@" +
								", workspace: " + encodingJob->_encodingItem->_workspace->_name +
								", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
								", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
								", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType) +
								", runtime_error: " + e.what()
							);
						}
						catch (exception &e)
						{
							_logger->error(
								__FILEREF__ + "processEncodingJob failed" + ", @MMS statistics@ - elapsed (seconds): @" +
								to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@" +
								", workspace: " + encodingJob->_encodingItem->_workspace->_name +
								", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey) +
								", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
								", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority)) +
								", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
							);
						}
					}
				}

				_logger->info(
					__FILEREF__ + "Processing encoding jobs statistics" + ", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority) +
					", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged) + ", freeEncodingJobsNumber: " +
					to_string(freeEncodingJobsNumber) + ", runningEncodingJobsNumber: " + to_string(runningEncodingJobsNumber) +
					", goingToRunEncodingJobsNumber: " + to_string(goingToRunEncodingJobsNumber) +
					", toBeRunEncodingJobsNumber: " + to_string(toBeRunEncodingJobsNumber)
				);

				if (freeEncodingJobsNumber == 0)
					_logger->warn(
						__FILEREF__ + "maxEncodingsToBeManaged should to be increased" + ", encodingPriority: " +
						MMSEngineDBFacade::toString(encodingPriority) + ", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged) +
						", freeEncodingJobsNumber: " + to_string(freeEncodingJobsNumber) + ", runningEncodingJobsNumber: " +
						to_string(runningEncodingJobsNumber) + ", goingToRunEncodingJobsNumber: " + to_string(goingToRunEncodingJobsNumber) +
						", toBeRunEncodingJobsNumber: " + to_string(toBeRunEncodingJobsNumber)
					);
			}

			chrono::system_clock::time_point endEvent = chrono::system_clock::now();
			long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
			_logger->info(
				__FILEREF__ + "End checking encodingJobs" + ", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
			);
		}
		catch (exception &e)
		{
			_logger->info(__FILEREF__ + "ActiveEncodingsManager loop failed");
		}
	}
}

void ActiveEncodingsManager::processEncodingJob(EncodingJob *encodingJob)
{
	chrono::system_clock::time_point startEvent = chrono::system_clock::now();

	switch (encodingJob->_encodingItem->_encodingType)
	{
	case MMSEngineDBFacade::EncodingType::EncodeImage:
	case MMSEngineDBFacade::EncodingType::EncodeVideoAudio:
	case MMSEngineDBFacade::EncodingType::OverlayImageOnVideo:
	case MMSEngineDBFacade::EncodingType::OverlayTextOnVideo:
	case MMSEngineDBFacade::EncodingType::GenerateFrames:
	case MMSEngineDBFacade::EncodingType::SlideShow:
	case MMSEngineDBFacade::EncodingType::FaceRecognition:
	case MMSEngineDBFacade::EncodingType::FaceIdentification:
	case MMSEngineDBFacade::EncodingType::LiveRecorder:
	case MMSEngineDBFacade::EncodingType::VideoSpeed:
	case MMSEngineDBFacade::EncodingType::PictureInPicture:
	case MMSEngineDBFacade::EncodingType::IntroOutroOverlay:
	case MMSEngineDBFacade::EncodingType::CutFrameAccurate:
	case MMSEngineDBFacade::EncodingType::LiveProxy:
	case MMSEngineDBFacade::EncodingType::VODProxy:
	case MMSEngineDBFacade::EncodingType::Countdown:
	case MMSEngineDBFacade::EncodingType::LiveGrid:
	case MMSEngineDBFacade::EncodingType::AddSilentAudio:
	{
		encodingJob->_encoderProxy.setEncodingData(&(encodingJob->_status), encodingJob->_encodingItem);

		_logger->info(
			__FILEREF__ + "Creating encoderVideoAudioProxy thread" +
			", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey) +
			", encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
		);
		thread encoderVideoAudioProxyThread(ref(encodingJob->_encoderProxy));
		encoderVideoAudioProxyThread.detach();

		// the lock guarantees us that the _ejsStatus is not updated
		// before the below _ejsStatus setting
		encodingJob->_encodingJobStart = chrono::system_clock::now();
		encodingJob->_status = EncoderProxy::EncodingJobStatus::GoingToRun;

		break;
	}
	default:
	{
		string errorMessage = __FILEREF__ + "Encoding not managed for the EncodingType" + ", encodingJob->_encodingItem->_encodingType: " +
							  MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType);
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
	}

	chrono::system_clock::time_point endEvent = chrono::system_clock::now();
	long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
	_logger->warn(
		__FILEREF__ + "processEncodingJob" +
		", encodingJob->_encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType) +
		", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
	);
}

void ActiveEncodingsManager::addEncodingItem(shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem)
{

	EncodingJob *encodingJobs;
	int maxEncodingsToBeManaged;

	if (encodingItem->_encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
	{
		encodingJobs = _highPriorityEncodingJobs;
		maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
	}
	else if (encodingItem->_encodingPriority == MMSEngineDBFacade::EncodingPriority::Medium)
	{
		encodingJobs = _mediumPriorityEncodingJobs;
		maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
	}
	else // if (encodingItem->_encodingPriority ==
		 // MMSEngineDBFacade::EncodingPriority::Low)
	{
		encodingJobs = _lowPriorityEncodingJobs;
		maxEncodingsToBeManaged = MAXLOWENCODINGSTOBEMANAGED;
	}

	int encodingJobIndex;
	for (encodingJobIndex = 0; encodingJobIndex < maxEncodingsToBeManaged; encodingJobIndex++)
	{
		if ((encodingJobs[encodingJobIndex])._status == EncoderProxy::EncodingJobStatus::Free)
		{
			(encodingJobs[encodingJobIndex])._status = EncoderProxy::EncodingJobStatus::ToBeRun;
			(encodingJobs[encodingJobIndex])._encodingJobStart = chrono::system_clock::now();
			(encodingJobs[encodingJobIndex])._encodingItem = encodingItem;
			// (encodingJobs [encodingJobIndex])._petEncodingThread			= (void
			// *) NULL;

			break;
		}
	}

	if (encodingJobIndex == maxEncodingsToBeManaged)
	{
		_logger->warn(
			__FILEREF__ + "Max Encodings Manager capacity reached" + ", workspace->_name: " + encodingItem->_workspace->_name +
			", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey) + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
			", encodingPriority: " + MMSEngineDBFacade::toString(encodingItem->_encodingPriority) +
			", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged)
		);

		throw MaxEncodingsManagerCapacityReached();
	}

	_logger->info(
		__FILEREF__ + "Encoding Job Key added" + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name +
		", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey) +
		", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
	);
}

unsigned long ActiveEncodingsManager::addEncodingItems(std::vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &vEncodingItems)
{
	unsigned long ulEncodingsNumberAdded = 0;
	lock_guard<mutex> locker(_mtEncodingJobs);

	int encodingItemIndex = 0;
	int encodingItemsNumber = vEncodingItems.size();
	for (shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem : vEncodingItems)
	{
		_logger->info(
			__FILEREF__ + "Adding Encoding Item " + to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber) +
			", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name + ", encodingItem->_ingestionJobKey: " +
			to_string(encodingItem->_ingestionJobKey) + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
			", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority)) +
			", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
		);

		try
		{
			addEncodingItem(encodingItem);
			ulEncodingsNumberAdded++;
		}
		catch (MaxEncodingsManagerCapacityReached &e)
		{
			_logger->info(
				__FILEREF__ + "Max Encodings Manager Capacity reached " + to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber) +
				", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name + ", encodingItem->_ingestionJobKey: " +
				to_string(encodingItem->_ingestionJobKey) + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
				", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority)) +
				", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
			);

			_mmsEngineDBFacade->updateEncodingJob(
				encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::MaxCapacityReached,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				encodingItem->_ingestionJobKey
			);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "addEncodingItem failed " + to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber) +
				", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name + ", encodingItem->_ingestionJobKey: " +
				to_string(encodingItem->_ingestionJobKey) + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey) +
				", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority)) +
				", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
			);
			_mmsEngineDBFacade->updateEncodingJob(
				encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::ErrorBeforeEncoding,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				encodingItem->_ingestionJobKey, "addEncodingItem failed"
			);
		}

		encodingItemIndex++;
	}

	if (ulEncodingsNumberAdded > 0)
	{
		_cvAddedEncodingJob.notify_one();
	}

	return ulEncodingsNumberAdded;
}
