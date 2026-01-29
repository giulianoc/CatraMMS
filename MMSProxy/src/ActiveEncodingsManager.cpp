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
#include "JSONUtils.h"
#include "System.h"
#include "spdlog/spdlog.h"
#include <fstream>

using namespace std;
using json = nlohmann::json;

ActiveEncodingsManager::ActiveEncodingsManager(
	const json& configuration, string processorMMS, const shared_ptr<MultiEventsSet>& multiEventsSet,
	const shared_ptr<MMSEngineDBFacade>& mmsEngineDBFacade, const shared_ptr<MMSStorage>& mmsStorage
): _configuration(configuration), _mmsEngineDBFacade(mmsEngineDBFacade), _mmsStorage(mmsStorage)
{
	_encodersLoadBalancer = make_shared<EncodersLoadBalancer>(_mmsEngineDBFacade, _configuration);

	_hostName = System::hostName();

	_maxSecondsToWaitUpdateEncodingJobLock = JSONUtils::as<int32_t>(_configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 0);
	LOG_INFO(
		"Configuration item"
		", mms->locks->maxSecondsToWaitUpdateEncodingJobLock: {}",
		_maxSecondsToWaitUpdateEncodingJobLock
	);

	{
		shared_ptr<long> faceRecognitionNumber = make_shared<long>(0);
		int maxFaceRecognitionNumber = JSONUtils::as<int32_t>(_configuration["mms"], "maxFaceRecognitionNumber", 0);
		LOG_INFO(
			"Configuration item"
			", mms->maxFaceRecognitionNumber: {}",
			maxFaceRecognitionNumber
		);
		maxFaceRecognitionNumber += (MAXHIGHENCODINGSTOBEMANAGED + MAXMEDIUMENCODINGSTOBEMANAGED + MAXLOWENCODINGSTOBEMANAGED);

		int lastProxyIdentifier = 0;

		for (EncodingJob &encodingJob : _lowPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber
			);
		}

		for (EncodingJob &encodingJob : _mediumPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber
			);
		}

		for (EncodingJob &encodingJob : _highPriorityEncodingJobs)
		{
			encodingJob._encoderProxy.init(
				lastProxyIdentifier++, &_mtEncodingJobs, _configuration, multiEventsSet, _mmsEngineDBFacade, _mmsStorage, _encodersLoadBalancer,
				faceRecognitionNumber, maxFaceRecognitionNumber
			);
		}
	}

	try
	{
		vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> encodingItems;

		_mmsEngineDBFacade->recoverEncodingsNotCompleted(processorMMS, encodingItems);

		LOG_INFO(
			"recoverEncodingsNotCompleted result"
			", _processorMMS: {}"
			", encodingItems.size: {}",
			processorMMS, encodingItems.size()
		);

		addEncodingItems(encodingItems);

		LOG_INFO(
			"addEncodingItems successful"
			", _processorMMS: {}"
			", encodingItems.size: {}",
			processorMMS, encodingItems.size()
		);
	}
	catch (exception &e)
	{
		LOG_ERROR(
			"recoverEncodingsNotCompleted failed"
			", _processorMMS: {}"
			", e.what: {}",
			processorMMS, e.what()
		);

		// è un thread/costruttore
		// throw e;
	}
}

ActiveEncodingsManager::~ActiveEncodingsManager() = default;

bool ActiveEncodingsManager::isProcessorShutdown()
{
	const string processorShutdownPathName = "/tmp/processorShutdown.txt";

	const ifstream f(processorShutdownPathName.c_str());

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
				LOG_INFO("ActiveEncodingsManager was shutdown");

				shutdown = true;

				continue;
			}

			unique_lock<mutex> locker(_mtEncodingJobs);

			// info("Reviewing current Encoding Jobs...");

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

			LOG_INFO("Begin checking encodingJobs");

			for (MMSEngineDBFacade::EncodingPriority encodingPriority : sortedEncodingPriorities)
			{
				EncodingJob *encodingJobs;
				int maxEncodingsToBeManaged;

				if (encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
				{
					// info(__FILEREF__ + "Processing the high encodings...");

					encodingJobs = _highPriorityEncodingJobs;
					maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
				}
				else if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Medium)
				{
					// info(__FILEREF__ + "Processing the default
					// encodings...");

					encodingJobs = _mediumPriorityEncodingJobs;
					maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
				}
				else // if (encodingPriority ==
					 // MMSEngineDBFacade::EncodingPriority::Low)
				{
					// info(__FILEREF__ + "Processing the low encodings...");

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
					if (encodingJob->_status == EncoderProxy::EncodingJobStatus::Running ||
						encodingJob->_status == EncoderProxy::EncodingJobStatus::GoingToRun)
					{
						if (encodingJob->_status == EncoderProxy::EncodingJobStatus::Running)
							runningEncodingJobsNumber++;
						else
							goingToRunEncodingJobsNumber++;

						if (encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::LiveRecorder &&
							encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::LiveProxy &&
							encodingJob->_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::VODProxy &&
							chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart) > chrono::hours(24))
						{
							LOG_ERROR(
								"EncodingJob is not finishing"
								", @MMS statistics@ - elapsed (hours): @{}@"
								", workspace: {}"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", _encodingPriority: {}"
								", _encodingType: {}"
								", encodingJob->_status: {}",
								chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count(),
								encodingJob->_encodingItem->_workspace->_name, encodingJob->_encodingItem->_ingestionJobKey,
								encodingJob->_encodingItem->_encodingJobKey, static_cast<int>(encodingJob->_encodingItem->_encodingPriority),
								MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType), EncoderProxy::toString(encodingJob->_status)
							);
						}
						else
						{
							LOG_INFO(
								"EncodingJob still running"
								", @MMS statistics@ - elapsed (hours): @{}@"
								", workspace: {}"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", _encodingPriority: {}"
								", _encodingType: {}"
								", encodingJob->_status: {}",
								chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count(),
								encodingJob->_encodingItem->_workspace->_name, encodingJob->_encodingItem->_ingestionJobKey,
								encodingJob->_encodingItem->_encodingJobKey, static_cast<int>(encodingJob->_encodingItem->_encodingPriority),
								MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType), EncoderProxy::toString(encodingJob->_status)
							);
						}
					}
					else // if (encodingJob._status == EncoderProxy::EncodingJobStatus::ToBeRun)
					{
						toBeRunEncodingJobsNumber++;

						chrono::system_clock::time_point processingItemStart = chrono::system_clock::now();

						LOG_INFO(
							"processEncodingJob begin"
							", workspace: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", _encodingPriority: {}"
							", _encodingType: {}",
							encodingJob->_encodingItem->_workspace->_name, encodingJob->_encodingItem->_ingestionJobKey,
							encodingJob->_encodingItem->_encodingJobKey, static_cast<int>(encodingJob->_encodingItem->_encodingPriority),
							MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
						);

						try
						{
							processEncodingJob(encodingJob);

							LOG_INFO(
								"processEncodingJob done"
								", @MMS statistics@ - elapsed (seconds): @{}@"
								", workspace: {}"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", _encodingPriority: {}"
								", _encodingType: {}",
								chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count(),
								encodingJob->_encodingItem->_workspace->_name, encodingJob->_encodingItem->_ingestionJobKey,
								encodingJob->_encodingItem->_encodingJobKey, static_cast<int>(encodingJob->_encodingItem->_encodingPriority),
								MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
							);
						}
						catch (exception &e)
						{
							LOG_ERROR(
								"processEncodingJob failed"
								", @MMS statistics@ - elapsed (seconds): @{}@"
								", workspace: {}"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", _encodingPriority: {}"
								", _encodingType: {}",
								chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count(),
								encodingJob->_encodingItem->_workspace->_name, encodingJob->_encodingItem->_ingestionJobKey,
								encodingJob->_encodingItem->_encodingJobKey, static_cast<int>(encodingJob->_encodingItem->_encodingPriority),
								MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
							);
						}
					}
				}

				LOG_INFO(
					"Processing encoding jobs statistics"
					", encodingPriority: {}"
					", maxEncodingsToBeManaged: {}"
					", freeEncodingJobsNumber: {}"
					", runningEncodingJobsNumber: {}"
					", goingToRunEncodingJobsNumber: {}"
					", toBeRunEncodingJobsNumber: {}",
					MMSEngineDBFacade::toString(encodingPriority), maxEncodingsToBeManaged, freeEncodingJobsNumber, runningEncodingJobsNumber,
					goingToRunEncodingJobsNumber, toBeRunEncodingJobsNumber
				);

				if (freeEncodingJobsNumber == 0)
					LOG_WARN(
						"maxEncodingsToBeManaged should to be increased"
						", encodingPriority: {}"
						", maxEncodingsToBeManaged: {}"
						", freeEncodingJobsNumber: {}"
						", runningEncodingJobsNumber: {}"
						", goingToRunEncodingJobsNumber: {}"
						", toBeRunEncodingJobsNumber: {}",
						MMSEngineDBFacade::toString(encodingPriority), maxEncodingsToBeManaged, freeEncodingJobsNumber, runningEncodingJobsNumber,
						goingToRunEncodingJobsNumber, toBeRunEncodingJobsNumber
					);
			}

			chrono::system_clock::time_point endEvent = chrono::system_clock::now();
			long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
			LOG_INFO(
				"End checking encodingJobs"
				", @MMS statistics@ - elapsed in seconds: @{}@",
				elapsedInSeconds
			);
		}
		catch (exception &e)
		{
			LOG_INFO("ActiveEncodingsManager loop failed"
				", exception: {}", e.what()
				);
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

		LOG_INFO(
			"Creating encoderVideoAudioProxy thread"
			", encodingJob->_encodingItem->_encodingJobKey: {}"
			", encodingType: {}",
			encodingJob->_encodingItem->_encodingJobKey, MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
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
		string errorMessage = std::format(
			"Encoding not managed for the EncodingType"
			", encodingJob->_encodingItem->_encodingType: {}",
			MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
		);
		LOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
	}

	chrono::system_clock::time_point endEvent = chrono::system_clock::now();
	long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
	LOG_WARN(
		"processEncodingJob"
		", encodingJob->_encodingItem->_encodingType: {}"
		", @MMS statistics@ - elapsed in seconds: @{}@",
		MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType), elapsedInSeconds
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
	else // if (encodingItem->_encodingPriority == MMSEngineDBFacade::EncodingPriority::Low)
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
			// (encodingJobs [encodingJobIndex])._petEncodingThread			= (void *) NULL;

			break;
		}
	}

	if (encodingJobIndex == maxEncodingsToBeManaged)
	{
		LOG_WARN(
			"Max Encodings Manager capacity reached"
			", workspace->_name: {}"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", encodingPriority: {}"
			", maxEncodingsToBeManaged: {}",
			encodingItem->_workspace->_name, encodingItem->_ingestionJobKey, encodingItem->_encodingJobKey,
			MMSEngineDBFacade::toString(encodingItem->_encodingPriority), maxEncodingsToBeManaged
		);

		throw MaxEncodingsManagerCapacityReached();
	}

	LOG_INFO(
		"Encoding Job Key added"
		", encodingItem->_workspace->_name: {}"
		", encodingItem->_ingestionJobKey: {}"
		", encodingItem->_encodingJobKey: {}",
		encodingItem->_workspace->_name, encodingItem->_ingestionJobKey, encodingItem->_encodingJobKey
	);
}

unsigned long ActiveEncodingsManager::addEncodingItems(std::vector<shared_ptr<MMSEngineDBFacade::EncodingItem>> &vEncodingItems)
{
	unsigned long ulEncodingsNumberAdded = 0;
	lock_guard locker(_mtEncodingJobs);

	int encodingItemIndex = 0;
	int encodingItemsNumber = vEncodingItems.size();
	for (const shared_ptr<MMSEngineDBFacade::EncodingItem>& encodingItem : vEncodingItems)
	{
		LOG_INFO(
			"Adding Encoding Item {}/{}"
			", encodingItem->_workspace->_name: {}"
			", encodingItem->_ingestionJobKey: {}"
			", encodingItem->_encodingJobKey: {}"
			", encodingItem->_encodingPriority: {}"
			", encodingItem->_encodingType: {}",
			encodingItemIndex, encodingItemsNumber, encodingItem->_workspace->_name, encodingItem->_ingestionJobKey, encodingItem->_encodingJobKey,
			static_cast<int>(encodingItem->_encodingPriority), MMSEngineDBFacade::toString(encodingItem->_encodingType)
		);

		try
		{
			addEncodingItem(encodingItem);
			ulEncodingsNumberAdded++;
		}
		catch (MaxEncodingsManagerCapacityReached &e)
		{
			LOG_INFO(
				"Max Encodings Manager Capacity reached {}/{}"
				", encodingItem->_workspace->_name: {}"
				", encodingItem->_ingestionJobKey: {}"
				", encodingItem->_encodingJobKey: {}"
				", encodingItem->_encodingPriority: {}"
				", encodingItem->_encodingType: {}",
				encodingItemIndex, encodingItemsNumber, encodingItem->_workspace->_name, encodingItem->_ingestionJobKey,
				encodingItem->_encodingJobKey, static_cast<int>(encodingItem->_encodingPriority),
				MMSEngineDBFacade::toString(encodingItem->_encodingType)
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
			LOG_ERROR(
				"addEncodingItem failed {}/{}"
				", encodingItem->_workspace->_name: {}"
				", encodingItem->_ingestionJobKey: {}"
				", encodingItem->_encodingJobKey: {}"
				", encodingItem->_encodingPriority: {}"
				", encodingItem->_encodingType: {}"
				", exception: {}",
				encodingItemIndex, encodingItemsNumber, encodingItem->_workspace->_name, encodingItem->_ingestionJobKey,
				encodingItem->_encodingJobKey, static_cast<int>(encodingItem->_encodingPriority),
				MMSEngineDBFacade::toString(encodingItem->_encodingType), e.what()
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
		_cvAddedEncodingJob.notify_one();

	return ulEncodingsNumberAdded;
}
