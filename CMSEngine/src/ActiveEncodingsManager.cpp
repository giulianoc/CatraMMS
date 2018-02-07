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

ActiveEncodingsManager::ActiveEncodingsManager(
    shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
    shared_ptr<CMSStorage> cmsStorage,
    shared_ptr<spdlog::logger> logger) 
{
    _logger = logger;
    _cmsEngineDBFacade = cmsEngineDBFacade;
    _cmsStorage = cmsStorage;    
}

ActiveEncodingsManager::~ActiveEncodingsManager()
{
}

void ActiveEncodingsManager::operator()()
{
    bool shutdown = false;
    
    chrono::seconds secondsToBlock(5);
    
    while (!shutdown)
    {
        unique_lock<mutex>  locker(_mtEncodingJobs);

        // _logger->info("Reviewing current Encoding Jobs...");

        if (_cvAddedEncodingJob.wait_for(locker, secondsToBlock) == cv_status::timeout)
        {
            // time expired

            continue;
        }
                        
        for (int encodingPriority = 0; encodingPriority < 3; encodingPriority++)
        {
            EncodingJob*    encodingJobs;
            int             maxEncodingsToBeManaged;
            
            if (encodingPriority == 0)
            {
                encodingJobs            = _highPriorityEncodingJobs;
                maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
            }
            else if (encodingPriority == 1)
            {
                encodingJobs = _defaultPriorityEncodingJobs;
                maxEncodingsToBeManaged = MAXDEFAULTENCODINGSTOBEMANAGED;
            }
            else // if (encodingPriority == 2)
            {
                encodingJobs = _lowPriorityEncodingJobs;
                maxEncodingsToBeManaged = MAXLOWENCODINGSTOBEMANAGED;
            }
            
            for (int encodingJobIndex = 0; encodingJobIndex < maxEncodingsToBeManaged; encodingJobIndex++)
            {
                EncodingJob* encodingJob = &(encodingJobs[encodingJobIndex]);
                
                if (encodingJob->_status == EncodingJobStatus::Free)
                    continue;
                else if (encodingJob->_status == EncodingJobStatus::Running)
                {
                    if (chrono::duration_cast<chrono::hours>(
                            chrono::system_clock::now() - encodingJob->_encodingJobStart) >
                            chrono::hours(24))
                    {
                        _logger->error(string("EncodingJob is not finishing")
                                + ", elapsed (hours): " + 
                                    to_string(chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count())
                        );
                    }
                    else
                    {
                        _logger->info(string("EncodingJob still running")
                                + ", elapsed (minutes): " + 
                                    to_string(chrono::duration_cast<chrono::minutes>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count())
                                + ", customer: " + encodingJob->_encodingItem->_customer->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(encodingJob->_encodingItem->_encodingPriority)
                                + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                        );
                    }
                }
                else // if (encodingJob._status == EncodingJobStatus::ToBeRun)
                {
                    chrono::system_clock::time_point        processingItemStart;

                    processEncodingJob(&_mtEncodingJobs, encodingJob);

                    _logger->info(string("processEncodingJob finished")
                            + ", elapsed (seconds): " + 
                                to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count())
                            + ", customer: " + encodingJob->_encodingItem->_customer->_name
                            + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                            + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                            + ", _encodingPriority: " + to_string(encodingJob->_encodingItem->_encodingPriority)
                            + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                            + ", _fileName: " + encodingJob->_encodingItem->_fileName
                            + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                    );

                }
            }
        }
    }
}

void ActiveEncodingsManager::processEncodingJob(mutex* mtEncodingJobs, EncodingJob* encodingJob)
{
    if (encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Video ||
            encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Audio)
    {
        EncoderVideoAudioProxy encoderVideoAudioProxy(
            mtEncodingJobs,
            &(encodingJob->_status),
            _cmsEngineDBFacade,
            _cmsStorage,
            encodingJob->_encodingItem,
            _logger
        );
        
        thread encoderVideoAudioProxyThread(encoderVideoAudioProxy);
        encoderVideoAudioProxyThread.detach();
        
        // the lock guarantees us that the _ejsStatus is not updated
        // before the below _ejsStatus setting
        encodingJob->_encodingJobStart		= chrono::system_clock::now();
        encodingJob->_status			= EncodingJobStatus::Running;
    }
}

void ActiveEncodingsManager::addEncodingItem(shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem)
{
    
    EncodingJob*    encodingJobs;
    int             maxEncodingsToBeManaged;

    if (encodingItem->_encodingPriority == 0)
    {
        encodingJobs            = _highPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
    }
    else if (encodingItem->_encodingPriority == 1)
    {
        encodingJobs = _defaultPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXDEFAULTENCODINGSTOBEMANAGED;
    }
    else // if (encodingItem->_encodingPriority == 2)
    {
        encodingJobs = _lowPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXLOWENCODINGSTOBEMANAGED;
    }
    
    int encodingJobIndex;
    for (encodingJobIndex = 0; encodingJobIndex < maxEncodingsToBeManaged; encodingJobIndex++)
    {
        if ((encodingJobs [encodingJobIndex])._status == EncodingJobStatus::Free)
        {
            (encodingJobs [encodingJobIndex])._status	= EncodingJobStatus::ToBeRun;
            (encodingJobs [encodingJobIndex])._encodingJobStart	= chrono::system_clock::now();
            (encodingJobs [encodingJobIndex])._encodingItem		= encodingItem;
            // (encodingJobs [encodingJobIndex])._petEncodingThread			= (void *) NULL;

            break;
        }
    }
    
    if (encodingJobIndex == maxEncodingsToBeManaged)
    {
        _logger->error("Max Encodings Manager capacity reached");
        
        throw MaxEncodingsManagerCapacityReached();
    }
    
    _logger->info(string("Encoding Job Key added")
        + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
        + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
        + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
    );
}

unsigned long ActiveEncodingsManager:: addEncodingItems (
	std:: vector<shared_ptr<CMSEngineDBFacade::EncodingItem>> *vEncodingItems)
{
    unsigned long       ulEncodingsNumberAdded = 0;
    lock_guard<mutex>   locker(_mtEncodingJobs);

    for (shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem: *vEncodingItems)
    {
        _logger->info(string("Adding Encoding Item")
            + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_encodingPriority: " + to_string(encodingItem->_encodingPriority)
            + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", encodingItem->_fileName: " + encodingItem->_fileName
        );

        try
        {
            addEncodingItem (encodingItem);
            ulEncodingsNumberAdded++;
        }
        catch(MaxEncodingsManagerCapacityReached e)
        {
            _logger->info(string("Max Encodings Manager Capacity reached")
                + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(encodingItem->_encodingPriority)
                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                + ", encodingItem->_fileName: " + encodingItem->_fileName
            );
            
            _cmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::MaxCapacityReached, encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(string("addEncodingItem failed")
                + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(encodingItem->_encodingPriority)
                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                + ", encodingItem->_fileName: " + encodingItem->_fileName
            );
            _cmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::ErrorBeforeEncoding, encodingItem->_ingestionJobKey);
        }        
    }
    
    if (ulEncodingsNumberAdded > 0)
    {
        _cvAddedEncodingJob.notify_one();
    }

    return ulEncodingsNumberAdded;
}