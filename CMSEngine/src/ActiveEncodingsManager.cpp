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
        CMSEngineDBFacade::EncodingPriority::Medium,
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
                else if (encodingPriority == CMSEngineDBFacade::EncodingPriority::Medium)
                {
                    // _logger->info(__FILEREF__ + "Processing the default encodings...");

                    encodingJobs = _mediumPriorityEncodingJobs;
                    maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
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
                    else if (encodingJob->_status == EncodingJobStatus::Running && 
                            (encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Video
                            || encodingJob->_encodingItem->_contentType == CMSEngineDBFacade::ContentType::Audio))
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
                        catch(runtime_error e)
                        {
                            _logger->error(__FILEREF__ + "_encoderVideoAudioProxy or updateEncodingJobProgress failed"
                                + ", runtime_error: " + e.what()
                            );
                        }
                        catch(exception e)
                        {
                            _logger->error(__FILEREF__ + "_encoderVideoAudioProxy or updateEncodingJobProgress failed");
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

                        try
                        {
                            processEncodingJob(&_mtEncodingJobs, encodingJob);
                            
                            _logger->info(__FILEREF__ + "processEncodingJob done"
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
                        catch(runtime_error e)
                        {
                            _logger->error(__FILEREF__ + "processEncodingJob failed"
                                + ", runtime_error: " + e.what()
                            );
                        }
                        catch(exception e)
                        {
                            _logger->error(__FILEREF__ + "processEncodingJob failed");
                        }
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
            + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
            + ", encodingJob->_encodingItem->_fileName: " + encodingJob->_encodingItem->_fileName
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
        string stagingEncodedAssetPathName;
        try
        {
            stagingEncodedAssetPathName = encodeContentImage(encodingJob->_encodingItem);
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
            processEncodedImage(encodingJob->_encodingItem, stagingEncodedAssetPathName);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "processEncodedImage: " + e.what());

            _logger->error(__FILEREF__ + "Remove"
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            FileIO::remove(stagingEncodedAssetPathName);

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

    string          stagingEncodedAssetPathName;
    
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
        
    string                      newImageFormat;
    int                         newWidth;
    int                         newHeight;
    bool                        newAspectRatio;
    string                      sNewInterlaceType;
    Magick::InterlaceType       newInterlaceType;

    readingImageProfile(encodingItem->_details,
            newImageFormat, newWidth, newHeight, newAspectRatio, sNewInterlaceType, newInterlaceType);

    try
    {
        Magick:: Image      imageToEncode;
        
        imageToEncode. read (cmsSourceAssetPathName.c_str());

        string currentImageFormat = imageToEncode.magick ();
        
	if (currentImageFormat == "jpeg")
            currentImageFormat = "JPG";

        int currentWidth	= imageToEncode. columns ();
	int currentHeight	= imageToEncode. rows ();

        _logger->info(__FILEREF__ + "Image processing"
            + ", encodingItem->_encodingProfileKey: " + to_string(encodingItem->_encodingProfileKey)
            + ", cmsSourceAssetPathName: " + cmsSourceAssetPathName
            + ", currentImageFormat: " + currentImageFormat
            + ", currentWidth: " + to_string(currentWidth)
            + ", currentHeight: " + to_string(currentHeight)
            + ", newImageFormat: " + newImageFormat
            + ", newWidth: " + to_string(newWidth)
            + ", newHeight: " + to_string(newHeight)
            + ", newAspectRatio: " + to_string(newAspectRatio)
            + ", sNewInterlace: " + sNewInterlaceType
        );
        
        if (currentImageFormat == newImageFormat
            && currentWidth == newWidth
            && currentHeight == newHeight)
        {
            // same as the ingested content. Just copy the content
            
            encodedFileName.append(encodingItem->_fileName.substr(extensionIndex));

            bool removeLinuxPathIfExist = true;
            stagingEncodedAssetPathName = _cmsStorage->getStagingAssetPathName(
                encodingItem->_customer->_directoryName,
                encodingItem->_relativePath,
                encodedFileName,
                -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
                -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
                removeLinuxPathIfExist);

            FileIO::copyFile (cmsSourceAssetPathName, stagingEncodedAssetPathName);
        }
        else
        {
            if (newImageFormat == "JPG")
            {
                imageToEncode. magick ("JPEG");                
                encodedFileName.append(".jpg");
            }
            else if (newImageFormat == "GIF")
            {
                imageToEncode. magick ("GIF");                
                encodedFileName.append(".gif");
            }
            else if (newImageFormat == "PNG")
            {
                imageToEncode. magick ("PNG");
                imageToEncode. depth (8);
                encodedFileName.append(".png");
            }

            bool removeLinuxPathIfExist = true;
            stagingEncodedAssetPathName = _cmsStorage->getStagingAssetPathName(
                encodingItem->_customer->_directoryName,
                encodingItem->_relativePath,
                encodedFileName,
                -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
                -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
                removeLinuxPathIfExist);
            
            Magick:: Geometry	newGeometry (newWidth, newHeight);

            // if Aspect is true the proportion are not mantained
            // if Aspect is false the proportion are mantained
            newGeometry. aspect (newAspectRatio);

            // if ulAspect is false, it means the aspect is preserved,
            // the width is fixed and the height will be calculated

            // also 'scale' could be used
            imageToEncode.scale (newGeometry);
            imageToEncode.interlaceType (newInterlaceType);
            imageToEncode.write(stagingEncodedAssetPathName);
        }
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
        
    return stagingEncodedAssetPathName;
}

void ActiveEncodingsManager::processEncodedImage(
        shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem, 
        string stagingEncodedAssetPathName)
{
    string encodedFileName;
    string cmsAssetPathName;
    unsigned long cmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        cmsAssetPathName = _cmsStorage->moveAssetInCMSRepository(
            stagingEncodedAssetPathName,
            encodingItem->_customer->_directoryName,
            encodedFileName,
            encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &cmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            encodingItem->_customer->_territories
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsStorage->moveAssetInCMSRepository failed"
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        throw e;
    }

    try
    {
        unsigned long long cmsAssetSizeInBytes;
        {
            bool inCaseOfLinkHasItToBeRead = false;
            cmsAssetSizeInBytes = FileIO::getFileSizeInBytes(cmsAssetPathName,
                    inCaseOfLinkHasItToBeRead);   
        }

        int64_t encodedPhysicalPathKey = _cmsEngineDBFacade->saveEncodedContentMetadata(
            encodingItem->_customer->_customerKey,
            encodingItem->_mediaItemKey,
            encodedFileName,
            encodingItem->_relativePath,
            cmsPartitionIndexUsed,
            cmsAssetSizeInBytes,
            encodingItem->_encodingProfileKey);
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_cmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        _logger->info(__FILEREF__ + "Remove"
            + ", cmsAssetPathName: " + cmsAssetPathName
        );

        FileIO::remove(cmsAssetPathName);

        throw e;
    }
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
    else if (encodingItem->_encodingPriority == CMSEngineDBFacade::EncodingPriority::Medium)
    {
        encodingJobs = _mediumPriorityEncodingJobs;
        maxEncodingsToBeManaged = MAXMEDIUMENCODINGSTOBEMANAGED;
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

void ActiveEncodingsManager::readingImageProfile(
        string profileDetails,
        string& newFormat,
        int& newWidth,
        int& newHeight,
        bool& newAspectRatio,
        string& sNewInterlaceType,
        Magick::InterlaceType& newInterlaceType
)
{
    string field;
    Json::Value encodingProfileRoot;
    try
    {
        Json::CharReaderBuilder builder;
        Json::CharReader* reader = builder.newCharReader();
        string errors;

        bool parsingSuccessful = reader->parse(profileDetails.c_str(),
                profileDetails.c_str() + profileDetails.size(), 
                &encodingProfileRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "failed to parse the encoder details"
                    + ", details: " + profileDetails;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(...)
    {
        throw runtime_error(string("wrong encoding profile json format")
                + ", profileDetails: " + profileDetails
                );
    }

    // Format
    {
        field = "format";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newFormat = encodingProfileRoot.get(field, "XXX").asString();

        encodingImageFormatValidation(newFormat);
    }

    // Width
    {
        field = "width";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newWidth = encodingProfileRoot.get(field, "XXX").asInt();
    }

    // Height
    {
        field = "height";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newHeight = encodingProfileRoot.get(field, "XXX").asInt();
    }

    // Aspect
    {
        field = "aspectRatio";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newAspectRatio = encodingProfileRoot.get(field, "XXX").asBool();
    }

    // Interlace
    {
        field = "interlaceType";
        if (!_cmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        sNewInterlaceType = encodingProfileRoot.get(field, "XXX").asString();

        newInterlaceType = encodingImageInterlaceTypeValidation(sNewInterlaceType);
    }
}

void ActiveEncodingsManager::encodingImageFormatValidation(string newFormat)
{    
    auto logger = spdlog::get("cmsEngineService");
    if (newFormat != "JPG" 
            && newFormat != "GIF" 
            && newFormat != "PNG" 
            )
    {
        string errorMessage = __FILEREF__ + "newFormat is wrong"
                + ", newFormat: " + newFormat;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
}

Magick::InterlaceType ActiveEncodingsManager::encodingImageInterlaceTypeValidation(string sNewInterlaceType)
{    
    auto logger = spdlog::get("cmsEngineService");
    Magick::InterlaceType       interlaceType;
    
    if (sNewInterlaceType == "NoInterlace")
        interlaceType       = Magick::NoInterlace;
    else if (sNewInterlaceType == "LineInterlace")
        interlaceType       = Magick::LineInterlace;
    else if (sNewInterlaceType == "PlaneInterlace")
        interlaceType       = Magick::PlaneInterlace;
    else if (sNewInterlaceType == "PartitionInterlace")
        interlaceType       = Magick::PartitionInterlace;
    else
    {
        string errorMessage = __FILEREF__ + "sNewInterlaceType is wrong"
                + ", sNewInterlaceType: " + sNewInterlaceType;

        logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return interlaceType;
}
