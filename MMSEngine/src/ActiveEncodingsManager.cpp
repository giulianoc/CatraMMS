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
    Json::Value configuration,
    shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
    shared_ptr<MMSStorage> mmsStorage,
    shared_ptr<spdlog::logger> logger) 
{
    _logger = logger;
    _configuration = configuration;
    _mmsEngineDBFacade = mmsEngineDBFacade;
    _mmsStorage = mmsStorage; 
    
    #ifdef __LOCALENCODER__
        _runningEncodingsNumber = 0;
    #endif

    {
        int lastProxyIdentifier = 0;
        
        for (EncodingJob& encodingJob: _lowPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, _mmsEngineDBFacade,
                    _mmsStorage, 
                    #ifdef __LOCALENCODER__
                        &_runningEncodingsNumber,
                    #endif
                    _logger);
        }
        
        for (EncodingJob& encodingJob: _mediumPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, _mmsEngineDBFacade,
                    _mmsStorage, 
                    #ifdef __LOCALENCODER__
                        &_runningEncodingsNumber,
                    #endif
                    _logger);
        }

        for (EncodingJob& encodingJob: _highPriorityEncodingJobs)
        {
            encodingJob._encoderVideoAudioProxy.init(
                lastProxyIdentifier++, &_mtEncodingJobs,
                    _configuration, _mmsEngineDBFacade,
                    _mmsStorage, 
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

void ActiveEncodingsManager::operator()()
{
    bool shutdown = false;
    
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

                    if (encodingJob->_status == EncodingJobStatus::Free)
                        continue;
                    else if (encodingJob->_status == EncodingJobStatus::Running && 
                            (encodingJob->_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video
                            || encodingJob->_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Audio))
                    {
                        try
                        {
                            int encodingPercentage = encodingJob->_encoderVideoAudioProxy
                                .getEncodingProgress(encodingJob->_encodingItem->_encodingJobKey);
                            
                            _mmsEngineDBFacade->updateEncodingJobProgress (encodingJob->_encodingItem->_encodingJobKey, 
                                encodingPercentage);
                        }
                        catch(EncodingStatusNotAvailable e)
                        {

                        }
                        catch(NoEncodingJobKeyFound e)
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
                                    + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                    + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                    + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                    + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                    + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                    + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                    + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                            );
                        }
                    }
                    else // if (encodingJob._status == EncodingJobStatus::ToBeRun)
                    {
                        chrono::system_clock::time_point        processingItemStart;

                        _logger->info(__FILEREF__ + "processEncodingJob"
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
                                + ", _ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
                                + ", _encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                                + ", _encodingPriority: " + to_string(static_cast<int>(encodingJob->_encodingItem->_encodingPriority))
                                + ", _relativePath: " + encodingJob->_encodingItem->_relativePath
                                + ", _fileName: " + encodingJob->_encodingItem->_fileName
                                + ", _encodingProfileKey: " + to_string(encodingJob->_encodingItem->_encodingProfileKey)
                        );

                        try
                        {
                            processEncodingJob(encodingJob);
                            
                            _logger->info(__FILEREF__ + "processEncodingJob done"
                                + ", elapsed (seconds): " + 
                                    to_string(chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - processingItemStart).count())
                                + ", workspace: " + encodingJob->_encodingItem->_workspace->_name
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

void ActiveEncodingsManager::processEncodingJob(EncodingJob* encodingJob)
{
    if (encodingJob->_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video ||
            encodingJob->_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Audio)
    {
        encodingJob->_encoderVideoAudioProxy.setEncodingData(
            &(encodingJob->_status),
            encodingJob->_encodingItem
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
    else if (encodingJob->_encodingItem->_contentType == MMSEngineDBFacade::ContentType::Image)
    {
        string stagingEncodedAssetPathName;
        try
        {
            stagingEncodedAssetPathName = encodeContentImage(encodingJob->_encodingItem);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "encodeContentImage: " + e.what());

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            // PunctualError is used because, in case it always happens, the encoding will never reach a final state
            int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                    encodingJob->_encodingItem->_encodingJobKey, 
                    MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                    encodingJob->_encodingItem->_ingestionJobKey);

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
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

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            // PunctualError is used because, in case it always happens, the encoding will never reach a final state
            int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                    encodingJob->_encodingItem->_encodingJobKey, 
                    MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                    encodingJob->_encodingItem->_ingestionJobKey);

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
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
            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob NoError"
                + ", encodingJob->_encodingItem->_encodingJobKey: " + to_string(encodingJob->_encodingItem->_encodingJobKey)
                + ", encodingJob->_encodingItem->_ingestionJobKey: " + to_string(encodingJob->_encodingItem->_ingestionJobKey)
            );

            _mmsEngineDBFacade->updateEncodingJob (
                encodingJob->_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::NoError, 
                encodingJob->_encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob failed: " + e.what());

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

string ActiveEncodingsManager::encodeContentImage(shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem)
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
    
    string mmsSourceAssetPathName = _mmsStorage->getMMSAssetPathName(
        encodingItem->_mmsPartitionNumber,
        encodingItem->_workspace->_directoryName,
        encodingItem->_relativePath,
        encodingItem->_fileName);

    string          stagingEncodedAssetPathName;
    
    // added the check of the file size is zero because in this case the
    // magick library cause the crash of the xmms engine
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long ulFileSize = FileIO::getFileSizeInBytes (
            mmsSourceAssetPathName, inCaseOfLinkHasItToBeRead);
        if (ulFileSize == 0)
        {
            string errorMessage = __FILEREF__ + "source image file size is zero"
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
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

    readingImageProfile(encodingItem->_jsonProfile,
            newImageFormat, newWidth, newHeight, newAspectRatio, sNewInterlaceType, newInterlaceType);

    try
    {
        Magick:: Image      imageToEncode;
        
        imageToEncode. read (mmsSourceAssetPathName.c_str());

        string currentImageFormat = imageToEncode.magick ();
        
	if (currentImageFormat == "jpeg")
            currentImageFormat = "JPG";

        int currentWidth	= imageToEncode. columns ();
	int currentHeight	= imageToEncode. rows ();

        _logger->info(__FILEREF__ + "Image processing"
            + ", encodingItem->_encodingProfileKey: " + to_string(encodingItem->_encodingProfileKey)
            + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
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
            stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
                encodingItem->_workspace->_directoryName,
                encodingItem->_relativePath,
                encodedFileName,
                -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
                -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
                removeLinuxPathIfExist);

            FileIO::copyFile (mmsSourceAssetPathName, stagingEncodedAssetPathName);
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
            stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
                encodingItem->_workspace->_directoryName,
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
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem, 
        string stagingEncodedAssetPathName)
{
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
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

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            encodingItem->_workspace->_directoryName,
            encodedFileName,
            encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            encodingItem->_workspace->_territories
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            bool inCaseOfLinkHasItToBeRead = false;
            mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                    inCaseOfLinkHasItToBeRead);   
        }

        int64_t encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            encodingItem->_workspace->_workspaceKey,
            encodingItem->_mediaItemKey,
            encodedFileName,
            encodingItem->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
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
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
            + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        _logger->info(__FILEREF__ + "Remove"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        FileIO::remove(mmsAssetPathName);

        throw e;
    }
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

    for (shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem: vEncodingItems)
    {
        _logger->info(__FILEREF__ + "Adding Encoding Item"
            + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
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
                + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                + ", encodingItem->_fileName: " + encodingItem->_fileName
            );
            
            _mmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "addEncodingItem failed"
                + ", encodingItem->_workspace->_name: " + encodingItem->_workspace->_name
                + ", encodingItem->_ingestionJobKey: " + to_string(encodingItem->_ingestionJobKey)
                + ", encodingItem->_encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                + ", encodingItem->_encodingPriority: " + to_string(static_cast<int>(encodingItem->_encodingPriority))
                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                + ", encodingItem->_fileName: " + encodingItem->_fileName
            );
            _mmsEngineDBFacade->updateEncodingJob (encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::ErrorBeforeEncoding, encodingItem->_ingestionJobKey);
        }        
    }
    
    if (ulEncodingsNumberAdded > 0)
    {
        _cvAddedEncodingJob.notify_one();
    }

    return ulEncodingsNumberAdded;
}

void ActiveEncodingsManager::readingImageProfile(
        string jsonProfile,
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

        bool parsingSuccessful = reader->parse(jsonProfile.c_str(),
                jsonProfile.c_str() + jsonProfile.size(), 
                &encodingProfileRoot, &errors);
        delete reader;

        if (!parsingSuccessful)
        {
            string errorMessage = __FILEREF__ + "failed to parse the encoder details"
                    + ", errors: " + errors
                    + ", jsonProfile: " + jsonProfile
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch(...)
    {
        throw runtime_error(string("wrong encoding profile json format")
                + ", jsonProfile: " + jsonProfile
                );
    }

    // Format
    {
        field = "format";
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
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
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
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
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
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
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
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
        if (!_mmsEngineDBFacade->isMetadataPresent(encodingProfileRoot, field))
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
    auto logger = spdlog::get("mmsEngineService");
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
    auto logger = spdlog::get("mmsEngineService");
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
