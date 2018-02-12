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
#include "Magick++.h"

ActiveEncodingsManager::ActiveEncodingsManager(
    shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
    shared_ptr<CMSStorage> cmsStorage,
    shared_ptr<spdlog::logger> logger) 
{
    _logger = logger;
    _cmsEngineDBFacade = cmsEngineDBFacade;
    _cmsStorage = cmsStorage; 
    
    #ifdef __FFMPEGLOCALENCODER__
        _ffmpegEncoderRunning = 0;
    #endif
}

ActiveEncodingsManager::~ActiveEncodingsManager()
{
}

void ActiveEncodingsManager::operator()()
{
    bool shutdown = false;
    
    chrono::seconds secondsToBlock(5);
    
    vector<CMSEngineDBFacade::EncodingPriority> sortedEncodingPriorities = { 
        CMSEngineDBFacade::EncodingPriority::High,
        CMSEngineDBFacade::EncodingPriority::Default,
        CMSEngineDBFacade::EncodingPriority::Low
    };
        
    while (!shutdown)
    {
        try
        {
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

            for (CMSEngineDBFacade::EncodingPriority encodingPriority: sortedEncodingPriorities)
            {
                EncodingJob*    encodingJobs;
                int             maxEncodingsToBeManaged;

                if (encodingPriority == CMSEngineDBFacade::EncodingPriority::High)
                {
                    // _logger->info(__FILEREF__ + "Processing the high encodings...");

                    encodingJobs            = _highPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
                }
                else if (encodingPriority == CMSEngineDBFacade::EncodingPriority::Default)
                {
                    // _logger->info(__FILEREF__ + "Processing the default encodings...");

                    encodingJobs = _defaultPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXDEFAULTENCODINGSTOBEMANAGED;
                }
                else // if (encodingPriority == CMSEngineDBFacade::EncodingPriority::Low)
                {
                    // _logger->info(__FILEREF__ + "Processing the low encodings...");

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
                        try
                        {
                            int encodingPercentage = encodingJob->_encoderVideoAudioProxy.getEncodingProgress();
                            
                            _cmsEngineDBFacade->updateEncodingJobProgress (encodingJob->_encodingItem->_encodingJobKey, 
                                encodingPercentage);
                        }
                        catch(EncodingStatusNotAvailable e)
                        {

                        }

                        if (chrono::duration_cast<chrono::hours>(
                                chrono::system_clock::now() - encodingJob->_encodingJobStart) >
                                chrono::hours(24))
                        {
                            _logger->error(__FILEREF__ + "EncodingJob is not finishing"
                                    + ", elapsed (hours): " + 
                                        to_string(chrono::duration_cast<chrono::hours>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count())
                            );
                        }
                        else
                        {
                            _logger->info(__FILEREF__ + "EncodingJob still running"
                                    + ", elapsed (minutes): " + 
                                        to_string(chrono::duration_cast<chrono::minutes>(chrono::system_clock::now() - encodingJob->_encodingJobStart).count())
                                    + ", customer: " + encodingJob->_encodingItem->_customer->_name
                                    + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                    + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                    + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                    + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                    + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                    + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                                    + ", _outputFfmpegPathFileName: " + encodingJob->_encoderVideoAudioProxy._outputFfmpegPathFileName
                            );
                        }
                    }
                    else // if (encodingJob._status == EncodingJobStatus::ToBeRun)
                    {
                        chrono::system_clock::time_point        processingItemStart;

                        _logger->info(__FILEREF__ + "processEncodingJob"
                                + ", customer: " + encodingJob->_encodingItem->_customer->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                        );

                        processEncodingJob(&_mtEncodingJobs, encodingJob);

                        _logger->info(__FILEREF__ + "processEncodingJob finished"
                                + ", elapsed (seconds): " + 
                                    to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count())
                                + ", customer: " + encodingJob->_encodingItem->_customer->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                        );
                    }
                }
            }
        }
        catch(exception e)
        {
            _logger->info(__FILEREF__ + "ActiveEncodingsManager loop failed");
        }
    }
}

void ActiveEncodingsManager::processEncodingJob(mutex* mtEncodingJobs, EncodingJob* encodingJob)
{
    if (encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Video ||
            encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Audio)
    {
        encodingJob->_encoderVideoAudioProxy.setData(
            mtEncodingJobs,
            &(encodingJob->_status),
            _cmsEngineDBFacade,
            _cmsStorage,
            encodingJob->_encodingItem,
            #ifdef __FFMPEGLOCALENCODER__
                &_ffmpegEncoderRunning,
            #endif
            _logger
        );
        
        _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", encodingJob->_encodingItem->_encodingJobKey" + to_string(encodingJob->_encodingItem->_encodingJobKey)
            + ", encodingJob->_encodingItem->_fileName" + encodingJob->_encodingItem->_fileName
        );
        thread encoderVideoAudioProxyThread(ref(encodingJob->_encoderVideoAudioProxy));
        encoderVideoAudioProxyThread.detach();
        
        // the lock guarantees us that the _ejsStatus is not updated
        // before the below _ejsStatus setting
        encodingJob->_encodingJobStart		= chrono::system_clock::now();
        encodingJob->_status			= EncodingJobStatus::Running;
    }
    else if (encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Image)
    {
        string stagingEncodedImagePathName;
        try
        {
            stagingEncodedImagePathName = encodeContentImage(encodingJob->_encodingItem);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "encodeContentImage: " + e.what());

            _logger->info(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            // PunctualError is used because, in case it always happens, the encoding will never reach a final state
            int encodingFailureNumber = _cmsEngineDBFacade->updateEncodingJob (
                    encodingJob->_encodingItem->_encodingJobKey, 
                    CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                    encodingJob->_encodingItem->_ingestionJobKey);

            _logger->info(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
            );

            encodingJob->_status = EncodingJobStatus::Free;

            // throw e;
            return;
        }

        try
        {
            processEncodedImage(stagingEncodedImagePathName);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "processEncodedImage: " + e.what());

            _logger->error(__FILEREF__ + "Remove"
                + ", stagingEncodedImagePathName: " + stagingEncodedImagePathName
            );

            FileIO::remove(stagingEncodedImagePathName);

            _logger->info(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            // PunctualError is used because, in case it always happens, the encoding will never reach a final state
            int encodingFailureNumber = _cmsEngineDBFacade->updateEncodingJob (
                    encodingJob->_encodingItem->_encodingJobKey, 
                    CMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                    encodingJob->_encodingItem->_ingestionJobKey);

            _logger->info(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
            );

            encodingJob->_status = EncodingJobStatus::Free;

            // throw e;
            return;
        }

        try
        {
            _logger->info(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob NoError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            _cmsEngineDBFacade->updateEncodingJob (
                encodingJob->_encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::NoError, 
                encodingJob->_encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_cmsEngineDBFacade->updateEncodingJob failed: " + e.what());

            encodingJob->_status = EncodingJobStatus::Free;

            // throw e;
            return;
        }

        encodingJob->_status = EncodingJobStatus::Free;
    }
    else
    {
        string errorMessage = __FILEREF__ + "Encoding not managed for the ContentType"
                + ", encodingJob->_encodingItem->_contentType: " + to_string(static_cast<int>(encodingJob->_encodingItem->_contentType))
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

string ActiveEncodingsManager::encodeContentImage(shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem)
{
    size_t extensionIndex = encodingItem->_fileName.find_last_of(".");
    if (extensionIndex == string::npos)
    {
        string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", encodingItem->_fileName: " + encodingItem->_fileName;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);
    }
    string encodedFileName =
            encodingItem->_fileName.substr(0, extensionIndex)
            + "_" 
            + to_string(encodingItem->_encodingProfileKey);
    
    string cmsSourceAssetPathName = _cmsStorage->getCMSAssetPathName(
        encodingItem->_cmsPartitionNumber,
        encodingItem->_customer->_directoryName,
        encodingItem->_relativePath,
        encodingItem->_fileName);

    try
    {
        // added the check of the file size is zero because in this case the
        // magick library cause the crash of the xcms engine
        {
            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long ulFileSize = FileIO::getFileSizeInBytes (
                cmsSourceAssetPathName, inCaseOfLinkHasItToBeRead);
            if (ulFileSize == 0)
            {
                string errorMessage = __FILEREF__ + "source image file size is zero"
                    + ", cmsSourceAssetPathName: " + cmsSourceAssetPathName
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        
        Magick:: Image      iImageToEncode;
        
        iImageToEncode. read (cmsSourceAssetPathName.c_str());

        string currentImageFormat = iImageToEncode.magick ();
        
	if (currentImageFormat == "jpeg")
            currentImageFormat = "JPG";

        int currentWidth	= iImageToEncode. columns ();
	int currentHeight	= iImageToEncode. rows ();

        {
            string field;
            Json::Value encodingProfileRoot;
            try
            {
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(encodingItem->_details.c_str(),
                        encodingItem->_details.c_str() + encodingItem->_details.size(), 
                        &encodingProfileRoot, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the encoder details"
                            + ", details: " + encodingItem->_details;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
            }
            catch(...)
            {
                throw runtime_error(string("wrong encoding profile json format")
                        + ", encodingItem->_details: " + encodingItem->_details
                        );
            }
        }

        Magick::InterlaceType			itInterlaceType;
    }
    catch (Magick::Error &e)
    {
        _logger->info(__FILEREF__ + "ImageMagick exception"
            + ", e.what(): " + e.what()
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
        );
        
        throw e;
    }
    catch (exception e)
    {
        _logger->info(__FILEREF__ + "ImageMagick exception"
            + ", e.what(): " + e.what()
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
        );
        
        throw e;
    }
    
    
    
    
    
    string stagingEncodedImagePathName;
    
    
    return stagingEncodedImagePathName;
}

void ActiveEncodingsManager::processEncodedImage(string stagingEncodedImagePathName)
{
    
}


void ActiveEncodingsManager::addEncodingItem(shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem)
{
    
    EncodingJob*    encodingJobs;
    int             maxEncodingsToBeManaged;

    if (encodingItem->_encodingPriority == CMSEngineDBFacade::EncodingPriority::High)
    {
        encodingJobs            = _highPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXHIGHENCODINGSTOBEMANAGED;
    }
    else if (encodingItem->_encodingPriority == CMSEngineDBFacade::EncodingPriority::Default)
    {
        encodingJobs = _defaultPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXDEFAULTENCODINGSTOBEMANAGED;
    }
    else // if (encodingItem->_encodingPriority == CMSEngineDBFacade::EncodingPriority::Low)
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
        _logger->error(__FILEREF__ + "Max Encodings Manager capacity reached");
        
        throw MaxEncodingsManagerCapacityReached();
    }
    
    _logger->info(__FILEREF__ + "Encoding Job Key added"
        + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
        + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
        + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
    );
}

unsigned long ActiveEncodingsManager:: addEncodingItems (
	std:: vector<shared_ptr<CMSEngineDBFacade::EncodingItem>>& vEncodingItems)
{
    unsigned long       ulEncodingsNumberAdded = 0;
    lock_guard<mutex>   locker(_mtEncodingJobs);

    for (shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem: vEncodingItems)
    {
        _logger->info(__FILEREF__ + "Adding Encoding Item"
            + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
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
            _logger->info(__FILEREF__ + "Max Encodings Manager Capacity reached"
                + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                + ", encodingItem->_fileName: " + encodingItem->_fileName
            );
            
            _cmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                CMSEngineDBFacade::EncodingError::MaxCapacityReached, encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "addEncodingItem failed"
                + ", encodingItem->_customer->_name: " + encodingItem->_customer->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
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