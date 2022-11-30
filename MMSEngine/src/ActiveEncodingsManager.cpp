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

#include <fstream>
#include "JSONUtils.h"
#include "ActiveEncodingsManager.h"
#include "catralibraries/System.h"
#include "FFMpeg.h"

ActiveEncodingsManager::ActiveEncodingsManager(
    Json::Value configuration,
    shared_ptr<MultiEventsSet> multiEventsSet,
    shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
    shared_ptr<MMSStorage> mmsStorage,
    shared_ptr<spdlog::logger> logger) 
{
    _logger = logger;
    _configuration = configuration;
    _mmsEngineDBFacade = mmsEngineDBFacade;
    _mmsStorage = mmsStorage; 

    _encodersLoadBalancer = make_shared<EncodersLoadBalancer>(_mmsEngineDBFacade, _configuration, _logger);

	_hostName				= System::getHostName();

	_maxSecondsToWaitUpdateEncodingJobLock  = JSONUtils::asInt(_configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 0);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", mms->locks->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
	);

    #ifdef __LOCALENCODER__
        _runningEncodingsNumber = 0;
    #endif

    {
		shared_ptr<long> faceRecognitionNumber = make_shared<long>(0);
		int maxFaceRecognitionNumber = JSONUtils::asInt(_configuration["mms"], "maxFaceRecognitionNumber", 0);
		_logger->info(__FILEREF__ + "Configuration item"
			+ ", mms->maxFaceRecognitionNumber: " + to_string(maxFaceRecognitionNumber)
		);
		maxFaceRecognitionNumber +=
			(MAXHIGHENCODINGSTOBEMANAGED + MAXMEDIUMENCODINGSTOBEMANAGED + MAXLOWENCODINGSTOBEMANAGED);
        
        int lastProxyIdentifier = 0;

        for (EncodingJob& encodingJob: _lowPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, multiEventsSet, _mmsEngineDBFacade,
                    _mmsStorage, _encodersLoadBalancer,
					faceRecognitionNumber, maxFaceRecognitionNumber,
                    #ifdef __LOCALENCODER__
                        &_runningEncodingsNumber,
                    #endif
                    _logger);
        }
        
        for (EncodingJob& encodingJob: _mediumPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, multiEventsSet, _mmsEngineDBFacade,
                    _mmsStorage, _encodersLoadBalancer,
					faceRecognitionNumber, maxFaceRecognitionNumber,
                    #ifdef __LOCALENCODER__
                        &_runningEncodingsNumber,
                    #endif
                    _logger);
        }

        for (EncodingJob& encodingJob: _highPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, multiEventsSet, _mmsEngineDBFacade,
                    _mmsStorage, _encodersLoadBalancer,
					faceRecognitionNumber, maxFaceRecognitionNumber,
                    #ifdef __LOCALENCODER__
                        &_runningEncodingsNumber,
                    #endif
                    _logger);
        }
    }                        
}

ActiveEncodingsManager::~ActiveEncodingsManager()
{
}

bool ActiveEncodingsManager::isProcessorShutdown()
{
	string processorShutdownPathName = "/tmp/processorShutdown.txt";

	ifstream f(processorShutdownPathName.c_str());

    return f.good();
}

void ActiveEncodingsManager::operator()()
{
    bool shutdown = false;

	// thread getEncodingsProgressThread(&ActiveEncodingsManager::getEncodingsProgressThread, this);
	// getEncodingsProgressThread.detach();

    chrono::seconds secondsToBlock(5);

    vector<MMSEngineDBFacade::EncodingPriority> sortedEncodingPriorities = { 
        MMSEngineDBFacade::EncodingPriority::High,
        MMSEngineDBFacade::EncodingPriority::Medium,
        MMSEngineDBFacade::EncodingPriority::Low
    };
        
    while (!shutdown)
    {
        try
        {
			if (isProcessorShutdown())
			{
				_logger->info(__FILEREF__ + "ActiveEncodingsManager was shutdown"
				);

				shutdown = true;

				continue;
			}

            unique_lock<mutex>  locker(_mtEncodingJobs);

            // _logger->info("Reviewing current Encoding Jobs...");

            _cvAddedEncodingJob.wait_for(locker, secondsToBlock);
            /*
            if (_cvAddedEncodingJob.wait_for(locker, secondsToBlock) == cv_status::timeout)
            {
                // time expired

                continue;
            }
             */

			chrono::system_clock::time_point startEvent = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Begin checking encodingJobs");

            for (MMSEngineDBFacade::EncodingPriority encodingPriority: sortedEncodingPriorities)
            {
                EncodingJob*    encodingJobs;
                int             maxEncodingsToBeManaged;

                if (encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
                {
                    // _logger->info(__FILEREF__ + "Processing the high encodings...");

                    encodingJobs            = _highPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
                }
                else if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Medium)
                {
                    // _logger->info(__FILEREF__ + "Processing the default encodings...");

                    encodingJobs = _mediumPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
                }
                else // if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Low)
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
                    EncodingJob* encodingJob = &(encodingJobs[encodingJobIndex]);

                    if (encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::Free)
					{
						freeEncodingJobsNumber++;

                        continue;
					}
                    else if (encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::Running
						|| encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::GoingToRun)
                    {
						if (encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::Running)
							runningEncodingJobsNumber++;
						else
							goingToRunEncodingJobsNumber++;

						/*
						// We will start to check the encodingProgress after at least XXX seconds.
						// This is because the status is set to EncodingJobStatus::Running as soon as it is created
						// the encoderVideoAudioProxyThread thread. Many times the thread returns soon because
						// of 'No encoding available' and in this case getEncodingProgress will return 'No encoding job key found'

                        if (chrono::system_clock::now() - encodingJob->_encodingJobStart >= chrono::seconds(secondsToBlock))

						2019-03-31: Above commented because it was introduced the GoingToRun status
						*/
						/*
						if (encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::Running)
                        {
                            try
                            {
                                int encodingPercentage =
									encodingJob->_encoderVideoAudioProxy
									.getEncodingProgress();

								_logger->info(__FILEREF__ + "updateEncodingJobProgress"
										+ ", encodingJobKey: "
											+ to_string(encodingJob->_encodingItem->_encodingJobKey)
										+ ", encodingPercentage: " + to_string(encodingPercentage)
										);
                                _mmsEngineDBFacade->updateEncodingJobProgress (encodingJob->_encodingItem->_encodingJobKey, 
                                    encodingPercentage);
                            }
                            catch(FFMpegEncodingStatusNotAvailable e)
                            {

                            }
                            catch(runtime_error e)
                            {
                                _logger->error(__FILEREF__ + "getEncodingProgress failed"
                                    + ", runtime_error: " + e.what()
                                );
                            }
                            catch(exception e)
                            {
                                _logger->error(__FILEREF__ + "getEncodingProgress failed");
                            }
                        }
						*/

						if (encodingJob->_encodingItem->_encodingType !=
							MMSEngineDBFacade::EncodingType::LiveRecorder
							&& encodingJob->_encodingItem->_encodingType !=
								MMSEngineDBFacade::EncodingType::LiveProxy
							&& encodingJob->_encodingItem->_encodingType !=
								MMSEngineDBFacade::EncodingType::VODProxy
							&& chrono::duration_cast<chrono::hours>(
								chrono::system_clock::now() - encodingJob->_encodingJobStart) >
								chrono::hours(24))
						{
							_logger->error(__FILEREF__ + "EncodingJob is not finishing"
								+ ", @MMS statistics@ - elapsed (hours): @" + to_string(chrono::duration_cast<chrono::hours>(
									chrono::system_clock::now() - encodingJob->_encodingJobStart).count()) + "@"
								+ ", workspace: " + encodingJob->_encodingItem->_workspace->_name
								+ ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
								+ ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
								+ ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
								+ ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
								+ ", encodingJob->_status: " + EncoderVideoAudioProxy::toString(encodingJob->_status)
							);
						}
						else
						{
							_logger->info(__FILEREF__ + "EncodingJob still running"
								+ ", elapsed (minutes): " + 
									to_string(chrono::duration_cast<chrono::minutes>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count())
								+ ", workspace: " + encodingJob->_encodingItem->_workspace->_name
								+ ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
								+ ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
								+ ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
								+ ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
								+ ", encodingJob->_status: " + EncoderVideoAudioProxy::toString(encodingJob->_status)
							);
						}
                    }
                    else // if (encodingJob._status == EncoderVideoAudioProxy::EncodingJobStatus::ToBeRun)
                    {
						toBeRunEncodingJobsNumber++;

                        chrono::system_clock::time_point        processingItemStart = chrono::system_clock::now();

                        _logger->info(__FILEREF__ + "processEncodingJob begin"
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
                                + ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
                        );

                        try
                        {
                            processEncodingJob(encodingJob);
                            
                            _logger->info(__FILEREF__ + "processEncodingJob done"
                                + ", @MMS statistics@ - elapsed (seconds): @" + 
                                    to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@"
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
                                + ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
                            );
                        }
                        catch(runtime_error e)
                        {
                            _logger->error(__FILEREF__ + "processEncodingJob failed"
                                + ", @MMS statistics@ - elapsed (seconds): @" + 
                                    to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@"
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
                                + ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
                                + ", runtime_error: " + e.what()
                            );
                        }
                        catch(exception e)
                        {
                            _logger->error(__FILEREF__ + "processEncodingJob failed"
                                + ", @MMS statistics@ - elapsed (seconds): @" + 
                                    to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count()) + "@"
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
                                + ", _encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
							);
                        }
                    }
                }

				_logger->info(__FILEREF__ + "Processing encoding jobs statistics"
					+ ", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority)
					+ ", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged)
					+ ", freeEncodingJobsNumber: " + to_string(freeEncodingJobsNumber)
					+ ", runningEncodingJobsNumber: " + to_string(runningEncodingJobsNumber)
					+ ", goingToRunEncodingJobsNumber: " + to_string(goingToRunEncodingJobsNumber)
					+ ", toBeRunEncodingJobsNumber: " + to_string(toBeRunEncodingJobsNumber)
				);

				if (freeEncodingJobsNumber == 0)
					_logger->warn(__FILEREF__ + "maxEncodingsToBeManaged should to be increased"
						+ ", encodingPriority: " + MMSEngineDBFacade::toString(encodingPriority)
						+ ", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged)
						+ ", freeEncodingJobsNumber: " + to_string(freeEncodingJobsNumber)
						+ ", runningEncodingJobsNumber: " + to_string(runningEncodingJobsNumber)
						+ ", goingToRunEncodingJobsNumber: " + to_string(goingToRunEncodingJobsNumber)
						+ ", toBeRunEncodingJobsNumber: " + to_string(toBeRunEncodingJobsNumber)
					);
			}

			chrono::system_clock::time_point endEvent = chrono::system_clock::now();
			long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
			_logger->info(__FILEREF__ + "End checking encodingJobs"
				+ ", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
			);
        }
        catch(exception e)
        {
            _logger->info(__FILEREF__ + "ActiveEncodingsManager loop failed");
        }
    }
}

/*
void ActiveEncodingsManager::getEncodingsProgressThread()
{
    bool shutdown = false;

	chrono::seconds secondsToSleep(5);
  
    vector<MMSEngineDBFacade::EncodingPriority> sortedEncodingPriorities = { 
        MMSEngineDBFacade::EncodingPriority::High,
        MMSEngineDBFacade::EncodingPriority::Medium,
        MMSEngineDBFacade::EncodingPriority::Low
    };
        
    while (!shutdown)
    {
        try
        {
			chrono::system_clock::time_point startEvent = chrono::system_clock::now();

			_logger->info(__FILEREF__ + "Begin getEncodingsProgressThread");

            for (MMSEngineDBFacade::EncodingPriority encodingPriority: sortedEncodingPriorities)
            {
                EncodingJob*    encodingJobs;
                int             maxEncodingsToBeManaged;

                if (encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
                {
                    // _logger->info(__FILEREF__ + "Processing the high encodings...");

                    encodingJobs            = _highPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
                }
                else if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Medium)
                {
                    // _logger->info(__FILEREF__ + "Processing the default encodings...");

                    encodingJobs = _mediumPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
                }
                else // if (encodingPriority == MMSEngineDBFacade::EncodingPriority::Low)
                {
                    // _logger->info(__FILEREF__ + "Processing the low encodings...");

                    encodingJobs = _lowPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXLOWENCODINGSTOBEMANAGED;
                }

                for (int encodingJobIndex = 0; encodingJobIndex < maxEncodingsToBeManaged; encodingJobIndex++)
                {
                    EncodingJob* encodingJob = &(encodingJobs[encodingJobIndex]);

                    if (encodingJob->_status == EncoderVideoAudioProxy::EncodingJobStatus::Running)
                    {
						// We will start to check the encodingProgress after at least XXX seconds.
						// This is because the status is set to EncodingJobStatus::Running as soon as it is created
						// the encoderVideoAudioProxyThread thread. Many times the thread returns soon because
						// of 'No encoding available' and in this case getEncodingProgress will return 'No encoding job key found'

                        // if (chrono::system_clock::now() - encodingJob->_encodingJobStart >= chrono::seconds(secondsToBlock))

						// 2019-03-31: Above commented because it was introduced the GoingToRun status
						try
						{
							int encodingPercentage =
								encodingJob->_encoderVideoAudioProxy.getEncodingProgress();

							_logger->info(__FILEREF__ + "updateEncodingJobProgress"
								+ ", encodingJobKey: "
								+ to_string(encodingJob->_encodingItem->_encodingJobKey)
								+ ", encodingPercentage: " + to_string(encodingPercentage)
							);
							_mmsEngineDBFacade->updateEncodingJobProgress (
								encodingJob->_encodingItem->_encodingJobKey, encodingPercentage);
						}
						catch(FFMpegEncodingStatusNotAvailable e)
						{

						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "getEncodingProgress failed"
								+ ", runtime_error: " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "getEncodingProgress failed");
						}
                    }
                }
			}

			chrono::system_clock::time_point endEvent = chrono::system_clock::now();
			long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
			_logger->info(__FILEREF__ + "End getEncodingsProgressThread"
				+ ", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
			);

			this_thread::sleep_for(secondsToSleep);
        }
        catch(exception e)
        {
            _logger->info(__FILEREF__ + "getEncodingsProgressThread loop failed");
        }
    }
}
*/

void ActiveEncodingsManager::processEncodingJob(EncodingJob* encodingJob)
{
	chrono::system_clock::time_point startEvent = chrono::system_clock::now();

	if (encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::EncodeImage
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::EncodeVideoAudio
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::OverlayImageOnVideo
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::OverlayTextOnVideo
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::GenerateFrames
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::SlideShow
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::FaceRecognition
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::FaceIdentification
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::LiveRecorder
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::VideoSpeed
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::PictureInPicture
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::IntroOutroOverlay
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::CutFrameAccurate
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::LiveProxy
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::VODProxy
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::Countdown
		|| encodingJob->_encodingItem->_encodingType ==
			MMSEngineDBFacade::EncodingType::LiveGrid
	)
    {
        encodingJob->_encoderVideoAudioProxy.setEncodingData(
            &(encodingJob->_status),
            encodingJob->_encodingItem
        );
        
        _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
            + ", encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
            + ", encodingParameters: " + encodingJob->_encodingItem->_encodingParameters
        );
        thread encoderVideoAudioProxyThread(ref(encodingJob->_encoderVideoAudioProxy));
        encoderVideoAudioProxyThread.detach();
        
        // the lock guarantees us that the _ejsStatus is not updated
        // before the below _ejsStatus setting
        encodingJob->_encodingJobStart		= chrono::system_clock::now();
        encodingJob->_status			= EncoderVideoAudioProxy::EncodingJobStatus::GoingToRun;
    }
    else
    {
        string errorMessage = __FILEREF__ + "Encoding not managed for the EncodingType"
                + ", encodingJob->_encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }

	chrono::system_clock::time_point endEvent = chrono::system_clock::now();
	long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();
	_logger->warn(__FILEREF__ + "processEncodingJob"
		+ ", encodingJob->_encodingItem->_encodingType: "
			+ MMSEngineDBFacade::toString(encodingJob->_encodingItem->_encodingType)
		+ ", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
	);
}

void ActiveEncodingsManager::addEncodingItem(shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem)
{

    EncodingJob*    encodingJobs;
    int             maxEncodingsToBeManaged;

    if (encodingItem->_encodingPriority == MMSEngineDBFacade::EncodingPriority::High)
    {
        encodingJobs            = _highPriorityEncodingJobs;
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
        if ((encodingJobs [encodingJobIndex])._status == EncoderVideoAudioProxy::EncodingJobStatus::Free)
        {
            (encodingJobs [encodingJobIndex])._status	= EncoderVideoAudioProxy::EncodingJobStatus::ToBeRun;
            (encodingJobs [encodingJobIndex])._encodingJobStart	= chrono::system_clock::now();
            (encodingJobs [encodingJobIndex])._encodingItem		= encodingItem;
            // (encodingJobs [encodingJobIndex])._petEncodingThread			= (void *) NULL;

            break;
        }
    }

    if (encodingJobIndex == maxEncodingsToBeManaged)
    {
		_logger->warn(__FILEREF__ + "Max Encodings Manager capacity reached"
			+ ", workspace->_name: " + encodingItem->_workspace->_name
			+ ", ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
			+ ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
			+ ", encodingPriority: " + MMSEngineDBFacade::toString(encodingItem->_encodingPriority)
			+ ", maxEncodingsToBeManaged: " + to_string(maxEncodingsToBeManaged)
		);
        
        throw MaxEncodingsManagerCapacityReached();
    }

    _logger->info(__FILEREF__ + "Encoding Job Key added"
        + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
        + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
        + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
    );
}

unsigned long ActiveEncodingsManager:: addEncodingItems (
	std:: vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& vEncodingItems)
{
    unsigned long       ulEncodingsNumberAdded = 0;
    lock_guard<mutex>   locker(_mtEncodingJobs);

	int encodingItemIndex = 0;
	int encodingItemsNumber = vEncodingItems.size();
    for (shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem: vEncodingItems)
    {
        _logger->info(__FILEREF__ + "Adding Encoding Item "
				+ to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber)
            + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
            + ", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
            + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
        );

        try
        {
            addEncodingItem (encodingItem);
            ulEncodingsNumberAdded++;
        }
        catch(MaxEncodingsManagerCapacityReached e)
        {
            _logger->info(__FILEREF__ + "Max Encodings Manager Capacity reached "
				+ to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber)
                + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
                + ", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
            );
            
            _mmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey,
                    MMSEngineDBFacade::EncodingError::MaxCapacityReached, 
                    false,  // isIngestionJobFinished: this field is not used by updateEncodingJob
                    encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "addEncodingItem failed "
				+ to_string(encodingItemIndex) + "/" + to_string(encodingItemsNumber)
                + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
                + ", encodingItem->_encodingType: " + MMSEngineDBFacade::toString(encodingItem->_encodingType)
                + ", encodingItem->_encodingParameters: " + encodingItem->_encodingParameters
            );
            _mmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::ErrorBeforeEncoding,
                false,  // isIngestionJobFinished: this field is not used by updateEncodingJob
				encodingItem->_ingestionJobKey, "addEncodingItem failed");
        }

		encodingItemIndex++;
    }
    
    if (ulEncodingsNumberAdded > 0)
    {
        _cvAddedEncodingJob.notify_one();
    }

    return ulEncodingsNumberAdded;
}

