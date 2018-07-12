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
#include <sstream>
#ifdef __LOCALENCODER__
#else
    #include <curlpp/cURLpp.hpp>
    #include <curlpp/Easy.hpp>
    #include <curlpp/Options.hpp>
    #include <curlpp/Exception.hpp>
    #include <curlpp/Infos.hpp>
#endif
#include "catralibraries/ProcessUtility.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "catralibraries/Convert.h"
#include "Validator.h"
#include "FFMpeg.h"
#include "EncoderVideoAudioProxy.h"


EncoderVideoAudioProxy::EncoderVideoAudioProxy()
{
}

EncoderVideoAudioProxy::~EncoderVideoAudioProxy() 
{
}

void EncoderVideoAudioProxy::init(
        int proxyIdentifier,
        mutex* mtEncodingJobs,
        Json::Value configuration,
        shared_ptr<MultiEventsSet> multiEventsSet,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        shared_ptr<EncodersLoadBalancer> encodersLoadBalancer,
        #ifdef __LOCALENCODER__
            int* pRunningEncodingsNumber,
        #endif
        shared_ptr<spdlog::logger> logger
)
{
    _proxyIdentifier        = proxyIdentifier;
    
    _mtEncodingJobs         = mtEncodingJobs;
    
    _logger                 = logger;
    _configuration          = configuration;
    
    _multiEventsSet         = multiEventsSet;
    _mmsEngineDBFacade      = mmsEngineDBFacade;
    _mmsStorage             = mmsStorage;
    _encodersLoadBalancer   = encodersLoadBalancer;
    
    _mp4Encoder             = _configuration["encoding"].get("mp4Encoder", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->mp4Encoder: " + _mp4Encoder
    );
    _mpeg2TSEncoder         = _configuration["encoding"].get("mpeg2TSEncoder", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->mpeg2TSEncoder: " + _mpeg2TSEncoder
    );
    
    _intervalInSecondsToCheckEncodingFinished         = _configuration["encoding"].get("intervalInSecondsToCheckEncodingFinished", "").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
    );        
    
    _ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
    );
    _ffmpegEncoderPort = _configuration["ffmpeg"].get("encoderPort", "").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
    );
    _ffmpegEncoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
    );
    _ffmpegEncoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPassword: " + "..."
    );
    _ffmpegEncoderProgressURI = _configuration["ffmpeg"].get("encoderProgressURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProgressURI: " + _ffmpegEncoderProgressURI
    );
    _ffmpegEncoderStatusURI = _configuration["ffmpeg"].get("encoderStatusURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI
    );
    _ffmpegEncodeURI = _configuration["ffmpeg"].get("encodeURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encodeURI: " + _ffmpegEncodeURI
    );
    _ffmpegOverlayImageOnVideoURI = _configuration["ffmpeg"].get("overlayImageOnVideoURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->overlayImageOnVideoURI: " + _ffmpegOverlayImageOnVideoURI
    );
    _ffmpegOverlayTextOnVideoURI = _configuration["ffmpeg"].get("overlayTextOnVideoURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->overlayTextOnVideoURI: " + _ffmpegOverlayTextOnVideoURI
    );
    _ffmpegGenerateFramesURI = _configuration["ffmpeg"].get("generateFramesURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->generateFramesURI: " + _ffmpegGenerateFramesURI
    );
            
    #ifdef __LOCALENCODER__
        _ffmpegMaxCapacity      = 1;
        
        _pRunningEncodingsNumber  = pRunningEncodingsNumber;
        
        _ffmpeg = make_shared<FFMpeg>(configuration, logger);
    #endif
}

void EncoderVideoAudioProxy::setEncodingData(
        EncodingJobStatus* status,
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem
)
{
    _status                 = status;
    
    _encodingItem           = encodingItem;            
}

void EncoderVideoAudioProxy::operator()()
{
    
    _logger->info(__FILEREF__ + "Running EncoderVideoAudioProxy..."
        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
        + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
    );

    string stagingEncodedAssetPathName;
    try
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            stagingEncodedAssetPathName = encodeContentVideoAudio();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            stagingEncodedAssetPathName = overlayImageOnVideo();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            stagingEncodedAssetPathName = overlayTextOnVideo();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            generateFrames();
        }
        else
        {
            string errorMessage = string("Wrong EncodingType")
                    + ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                    ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(MaxConcurrentJobsReached e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->warn(__FILEREF__ + "encodeContentVideoAudio: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->warn(__FILEREF__ + "overlayImageOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->warn(__FILEREF__ + "overlayTextOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->warn(__FILEREF__ + "generateFrames: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob MaxCapacityReached"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        int64_t encodedPhysicalPathKey = -1;
        _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(EncoderError e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayImageOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayTextOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->error(__FILEREF__ + "generateFrames: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        int64_t encodedPhysicalPathKey = -1;
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(runtime_error e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayImageOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayTextOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->error(__FILEREF__ + "generateFrames: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        int64_t encodedPhysicalPathKey = -1;
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(exception e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->error(__FILEREF__ + "encodeContentVideoAudio: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayImageOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->error(__FILEREF__ + "overlayTextOnVideo: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->error(__FILEREF__ + "generateFrames: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        int64_t encodedPhysicalPathKey = -1;
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }

    int64_t mediaItemKey;
    int64_t encodedPhysicalPathKey;

    try
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            mediaItemKey = _encodingItem->_encodeData->_mediaItemKey;

            encodedPhysicalPathKey = processEncodedContentVideoAudio(
                stagingEncodedAssetPathName);
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey = processOverlayedImageOnVideo(
                stagingEncodedAssetPathName);
            
            mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;
            encodedPhysicalPathKey = mediaItemKeyAndPhysicalPathKey.second;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey = processOverlayedTextOnVideo(
                stagingEncodedAssetPathName);
            
            mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;
            encodedPhysicalPathKey = mediaItemKeyAndPhysicalPathKey.second;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            processGeneratedFrames();            
        }
        else
        {
            string errorMessage = string("Wrong EncodingType")
                    + ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                    ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->error(__FILEREF__ + "processEncodedContentVideoAudio failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->error(__FILEREF__ + "processOverlayedTextOnVideo failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->error(__FILEREF__ + "processGeneratedFrames failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        if (FileIO::fileExisting(stagingEncodedAssetPathName) || 
                FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->error(__FILEREF__ + "Remove"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        encodedPhysicalPathKey = -1;
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(exception e)
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
            _logger->error(__FILEREF__ + "processEncodedContentVideoAudio failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            _logger->error(__FILEREF__ + "processOverlayedTextOnVideo failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            _logger->error(__FILEREF__ + "processGeneratedFrames failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );
        }

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->error(__FILEREF__ + "Remove"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        int64_t mediaItemKey = -1;
        int64_t encodedPhysicalPathKey = -1;
        // PunctualError is used because, in case it always happens, the encoding will never reach a final state
        int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob PunctualError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }

    if (_encodingItem->_encodingType != MMSEngineDBFacade::EncodingType::GenerateFrames)
    {
        // the updateEncodingJob in case of GenerateFrames it is done into the processGeneratedFrames
        //  because it generates a log of output media items and the manage of updateEncodingJob is different
        
        try
        {
            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob NoError"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            );

            _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::NoError,
                mediaItemKey, encodedPhysicalPathKey,
                _encodingItem->_ingestionJobKey);
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->updateEncodingJob failed: " + e.what()
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );

            {
                lock_guard<mutex> locker(*_mtEncodingJobs);

                *_status = EncodingJobStatus::Free;
            }

            _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            );

            // throw e;
            return;
        }
    }
    
    {
        lock_guard<mutex> locker(*_mtEncodingJobs);

        *_status = EncodingJobStatus::Free;
    }        
    
    _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        + ", _encodingItem->_encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
        + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
    );
        
}

string EncoderVideoAudioProxy::encodeContentVideoAudio()
{
    string stagingEncodedAssetPathName;
    
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingItem->_encodeData->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodeData->_encodingProfileTechnology))
        + ", _mp4Encoder: " + _mp4Encoder
    );

    if (
        (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4 &&
            _mp4Encoder == "FFMPEG") ||
        (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
        stagingEncodedAssetPathName = encodeContent_VideoAudio_through_ffmpeg();
    }
    else if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return stagingEncodedAssetPathName;
}

string EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
{
    
    int64_t sourcePhysicalPathKey;
    int64_t encodingProfileKey;    

    {
        string field = "sourcePhysicalPathKey";
        sourcePhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "encodingProfileKey";
        encodingProfileKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();
    }
    
    string stagingEncodedAssetPathName;
    string encodedFileName;
    string mmsSourceAssetPathName;

    
    #ifdef __LOCALENCODER__
        if (*_pRunningEncodingsNumber > _ffmpegMaxCapacity)
        {
            _logger->info("Max ffmpeg encoder capacity is reached"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );

            throw MaxConcurrentJobsReached();
        }
    #endif

    // stagingEncodedAssetPathName preparation
    {        
        mmsSourceAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_encodeData->_mmsPartitionNumber,
            _encodingItem->_workspace->_directoryName,
            _encodingItem->_encodeData->_relativePath,
            _encodingItem->_encodeData->_fileName);

        size_t extensionIndex = _encodingItem->_encodeData->_fileName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingItem->_encodeData->_fileName: " + _encodingItem->_encodeData->_fileName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName =
                to_string(_encodingItem->_ingestionJobKey)
                + "_"
                + to_string(_encodingItem->_encodingJobKey)
                + "_" 
                + to_string(encodingProfileKey);
        if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4)
            encodedFileName.append(".mp4");
        else if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
                _encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
            ;
        else if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM)
            encodedFileName.append(".webm");
        else
        {
            string errorMessage = __FILEREF__ + "Unknown technology"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        bool removeLinuxPathIfExist = true;
        stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
            _encodingItem->_workspace->_directoryName,
            "/",    // _encodingItem->_relativePath,
            encodedFileName,
            -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
            -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
            removeLinuxPathIfExist);

        if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
            _encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
        {
            // In this case, the path is a directory where to place the segments

            if (!FileIO::directoryExisting(stagingEncodedAssetPathName)) 
            {
                _logger->info(__FILEREF__ + "Create directory"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );

                bool noErrorIfExists = true;
                bool recursive = true;
                FileIO::createDirectory(
                        stagingEncodedAssetPathName,
                        S_IRUSR | S_IWUSR | S_IXUSR |
                        S_IRGRP | S_IXGRP |
                        S_IROTH | S_IXOTH, noErrorIfExists, recursive);
            }        
        }
        
    }

    #ifdef __LOCALENCODER__
        (*_pRunningEncodingsNumber)++;

        try
        {
            _ffmpeg->encodeContent(
                mmsSourceAssetPathName,
                _encodingItem->_encodeData->_durationInMilliSeconds,
                encodedFileName,
                stagingEncodedAssetPathName,
                _encodingItem->_details,
                _encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video,
                _encodingItem->_physicalPathKey,
                _encodingItem->_workspace->_directoryName,
                _encodingItem->_relativePath,
                _encodingItem->_encodingJobKey,
                _encodingItem->_ingestionJobKey);

            (*_pRunningEncodingsNumber)++;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + _ffmpegEncodeURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
                Json::Value encodingMedatada;
                
                encodingMedatada["mmsSourceAssetPathName"] = mmsSourceAssetPathName;
                encodingMedatada["durationInMilliSeconds"] = (Json::LargestUInt) (_encodingItem->_encodeData->_durationInMilliSeconds);
                encodingMedatada["encodedFileName"] = encodedFileName;
                encodingMedatada["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                Json::Value encodingDetails;
                {
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(_encodingItem->_encodeData->_jsonProfile.c_str(),
                                _encodingItem->_encodeData->_jsonProfile.c_str() + _encodingItem->_encodeData->_jsonProfile.size(), 
                                &encodingDetails, &errors);
                        delete reader;

                        if (!parsingSuccessful)
                        {
                            string errorMessage = __FILEREF__ + "failed to parse the _encodingItem->_jsonProfile"
                                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                    + ", errors: " + errors
                                    + ", _encodingItem->_encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                    catch(...)
                    {
                        string errorMessage = string("_encodingItem->_jsonProfile json is not well format")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", _encodingItem->_encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                encodingMedatada["encodingProfileDetails"] = encodingDetails;
                encodingMedatada["contentType"] = MMSEngineDBFacade::toString(_encodingItem->_encodeData->_contentType);
                encodingMedatada["physicalPathKey"] = (Json::LargestUInt) (sourcePhysicalPathKey);
                encodingMedatada["workspaceDirectoryName"] = _encodingItem->_workspace->_directoryName;
                encodingMedatada["relativePath"] = _encodingItem->_encodeData->_relativePath;
                encodingMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                encodingMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, encodingMedatada);
                }
            }
            
            list<string> header;

            header.push_back("Content-Type: application/json");
            {
                string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            if (_ffmpegEncoderProtocol == "https")
            {
                /*
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                    typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                    typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                    typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                 */
                                                                                                  
                
                /*
                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                request.setOpt(sslCert);
                 */

                /*
                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                if(pPassphrase)
                  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
                */
                
                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);
                
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);
                
                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));
            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Encoding media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value encodeContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &encodeContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }               
            }
            catch(runtime_error e)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw e;
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                // same string declared in FFMPEGEncoder.cpp
                string noEncodingAvailableMessage("__NO-ENCODING-AVAILABLE__");
            
                string field = "error";
                if (Validator::isMetadataPresent(encodeContentResponse, field))
                {
                    string error = encodeContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(noEncodingAvailableMessage) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }                        
                }
                /*
                else
                {
                    string field = "ffmpegEncoderHost";
                    if (Validator::isMetadataPresent(encodeContentResponse, field))
                    {
                        _currentUsedFFMpegEncoderHost = encodeContentResponse.get("ffmpegEncoderHost", "XXX").asString();
                        
                        _logger->info(__FILEREF__ + "Retrieving ffmpegEncoderHost"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
            
            // loop waiting the end of the encoding
            bool encodingFinished = false;
            int maxEncodingStatusFailures = 1;
            int encodingStatusFailures = 0;
            while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
                try
                {
                    encodingFinished = getEncodingStatus(_encodingItem->_encodingJobKey);
                }
                catch(...)
                {
                    _logger->error(__FILEREF__ + "getEncodingStatus failed");
                    
                    encodingStatusFailures++;
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
            _logger->info(__FILEREF__ + "Encoded media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
                    + ", sResponse: " + sResponse
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
            );
        }
        catch(MaxConcurrentJobsReached e)
        {
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", response.str(): " + response.str()
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

            throw e;
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return stagingEncodedAssetPathName;
}

int64_t EncoderVideoAudioProxy::processEncodedContentVideoAudio(string stagingEncodedAssetPathName)
{
    int64_t sourcePhysicalPathKey;
    int64_t encodingProfileKey;    

    {
        string field = "sourcePhysicalPathKey";
        sourcePhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "encodingProfileKey";
        encodingProfileKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();
    }
    
    int64_t durationInMilliSeconds = -1;
    long bitRate = -1;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate = -1;
    string audioCodecName;
    long audioSampleRate = -1;
    int audioChannels = -1;
    long audioBitRate = -1;

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality = -1;
    try
    {
        FFMpeg ffmpeg (_configuration, _logger);
        tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
            ffmpeg.getMediaInfo(stagingEncodedAssetPathName);

        tie(durationInMilliSeconds, bitRate, 
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
        );

        throw e;
    }        
    
    
    int64_t encodedPhysicalPathKey;
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        /*
        encodedFileName = _encodingItem->_fileName
                + "_" 
                + to_string(_encodingItem->_encodingProfileKey);
        if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4)
            encodedFileName.append(".mp4");
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS ||
                _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe)
            ;
        else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM)
            encodedFileName.append(".webm");
        else
        {
            string errorMessage = __FILEREF__ + "Unknown technology"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    ;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        */        

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            _encodingItem->_encodeData->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            _encodingItem->_workspace->_territories
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            FileIO::DirectoryEntryType_t detSourceFileType = 
                    FileIO::getDirectoryEntryType(mmsAssetPathName);

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                    detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                string errorMessage = __FILEREF__ + "Wrong directory entry type"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        ;

                _logger->error(errorMessage);
                throw runtime_error(errorMessage);
            }

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                mmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(mmsAssetPathName);   
            }
            else
            {
                bool inCaseOfLinkHasItToBeRead = false;
                mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                        inCaseOfLinkHasItToBeRead);   
            }
        }


        encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            _encodingItem->_workspace->_workspaceKey,
            _encodingItem->_encodeData->_mediaItemKey,
            encodedFileName,
            _encodingItem->_encodeData->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            encodingProfileKey,
                
            durationInMilliSeconds,
            bitRate,
            videoCodecName,
            videoProfile,
            videoWidth,
            videoHeight,
            videoAvgFrameRate,
            videoBitRate,
            audioCodecName,
            audioSampleRate,
            audioChannels,
            audioBitRate,

            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
                );
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        if (FileIO::fileExisting(mmsAssetPathName)
                || FileIO::directoryExisting(mmsAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

            _logger->info(__FILEREF__ + "Remove"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", mmsAssetPathName: " + mmsAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                FileIO::remove(mmsAssetPathName);
            }
        }

        throw e;
    }
    
    return encodedPhysicalPathKey;
}

string EncoderVideoAudioProxy::overlayImageOnVideo()
{
    string stagingEncodedAssetPathName;
    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingItem->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
        + ", _mp4Encoder: " + _mp4Encoder
    );

    if (
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4 &&
            _mp4Encoder == "FFMPEG") ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
    */
        stagingEncodedAssetPathName = overlayImageOnVideo_through_ffmpeg();
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    return stagingEncodedAssetPathName;
}

string EncoderVideoAudioProxy::overlayImageOnVideo_through_ffmpeg()
{
    
    int64_t sourceVideoPhysicalPathKey;
    int64_t sourceImagePhysicalPathKey;  
    string imagePosition_X_InPixel;
    string imagePosition_Y_InPixel;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "sourceImagePhysicalPathKey";
        sourceImagePhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "imagePosition_X_InPixel";
        imagePosition_X_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "imagePosition_Y_InPixel";
        imagePosition_Y_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();
    }
    
    string stagingEncodedAssetPathName;
    string encodedFileName;
    string mmsSourceVideoAssetPathName;
    string mmsSourceImageAssetPathName;

    
    #ifdef __LOCALENCODER__
        if (*_pRunningEncodingsNumber > _ffmpegMaxCapacity)
        {
            _logger->info("Max ffmpeg encoder capacity is reached"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );

            throw MaxConcurrentJobsReached();
        }
    #endif

    // stagingEncodedAssetPathName preparation
    {        
        mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_overlayImageOnVideoData->_mmsVideoPartitionNumber,
            _encodingItem->_workspace->_directoryName,
            _encodingItem->_overlayImageOnVideoData->_videoRelativePath,
            _encodingItem->_overlayImageOnVideoData->_videoFileName);

        mmsSourceImageAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_overlayImageOnVideoData->_mmsImagePartitionNumber,
            _encodingItem->_workspace->_directoryName,
            _encodingItem->_overlayImageOnVideoData->_imageRelativePath,
            _encodingItem->_overlayImageOnVideoData->_imageFileName);

        size_t extensionIndex = _encodingItem->_overlayImageOnVideoData->_videoFileName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingItem->_overlayImageOnVideoData->_videoFileName: " + _encodingItem->_overlayImageOnVideoData->_videoFileName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName =
                to_string(_encodingItem->_ingestionJobKey)
                + "_"
                + to_string(_encodingItem->_encodingJobKey)
                + _encodingItem->_overlayImageOnVideoData->_videoFileName.substr(extensionIndex)
                ;

        bool removeLinuxPathIfExist = true;
        stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
            _encodingItem->_workspace->_directoryName,
            "/",    // _encodingItem->_relativePath,
            encodedFileName,
            -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
            -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
            removeLinuxPathIfExist);        
    }

    #ifdef __LOCALENCODER__
        (*_pRunningEncodingsNumber)++;

        /*
        try
        {
            _ffmpeg->encodeContent(
                mmsSourceAssetPathName,
                _encodingItem->_durationInMilliSeconds,
                encodedFileName,
                stagingEncodedAssetPathName,
                _encodingItem->_details,
                _encodingItem->_contentType == MMSEngineDBFacade::ContentType::Video,
                _encodingItem->_physicalPathKey,
                _encodingItem->_workspace->_directoryName,
                _encodingItem->_relativePath,
                _encodingItem->_encodingJobKey,
                _encodingItem->_ingestionJobKey);

            (*_pRunningEncodingsNumber)++;
        }
        catch(runtime_error e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->encodeContent failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
                + ", encodedFileName: " + encodedFileName
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _encodingItem->_details: " + _encodingItem->_details
                + ", _encodingItem->_contentType: " + MMSEngineDBFacade::toString(_encodingItem->_contentType)
                + ", _encodingItem->_physicalPathKey: " + to_string(_encodingItem->_physicalPathKey)
                + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            );
            
            (*_pRunningEncodingsNumber)++;
            
            throw e;
        }
        */
    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + _ffmpegOverlayImageOnVideoURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
                Json::Value overlayMedatada;
                
                overlayMedatada["mmsSourceVideoAssetPathName"] = mmsSourceVideoAssetPathName;
                overlayMedatada["videoDurationInMilliSeconds"] = (Json::LargestUInt) (_encodingItem->_overlayImageOnVideoData->_videoDurationInMilliSeconds);
                overlayMedatada["mmsSourceImageAssetPathName"] = mmsSourceImageAssetPathName;
                overlayMedatada["imagePosition_X_InPixel"] = imagePosition_X_InPixel;
                overlayMedatada["imagePosition_Y_InPixel"] = imagePosition_Y_InPixel;
                overlayMedatada["encodedFileName"] = encodedFileName;
                overlayMedatada["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                overlayMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                overlayMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, overlayMedatada);
                }
            }
            
            list<string> header;

            header.push_back("Content-Type: application/json");
            {
                string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            if (_ffmpegEncoderProtocol == "https")
            {
                /*
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                    typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                    typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                    typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                 */
                                                                                                  
                
                /*
                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                request.setOpt(sslCert);
                 */

                /*
                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                if(pPassphrase)
                  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
                */
                
                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);
                
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);
                
                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));
            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Overlaying media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value overlayContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &overlayContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }               
            }
            catch(runtime_error e)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw e;
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                // same string declared in FFMPEGEncoder.cpp
                string noEncodingAvailableMessage("__NO-ENCODING-AVAILABLE__");
            
                string field = "error";
                if (Validator::isMetadataPresent(overlayContentResponse, field))
                {
                    string error = overlayContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(noEncodingAvailableMessage) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }                        
                }
                /*
                else
                {
                    string field = "ffmpegEncoderHost";
                    if (Validator::isMetadataPresent(encodeContentResponse, field))
                    {
                        _currentUsedFFMpegEncoderHost = encodeContentResponse.get("ffmpegEncoderHost", "XXX").asString();
                        
                        _logger->info(__FILEREF__ + "Retrieving ffmpegEncoderHost"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
            
            // loop waiting the end of the encoding
            bool encodingFinished = false;
            int maxEncodingStatusFailures = 1;
            int encodingStatusFailures = 0;
            while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
                try
                {
                    encodingFinished = getEncodingStatus(_encodingItem->_encodingJobKey);
                }
                catch(...)
                {
                    _logger->error(__FILEREF__ + "getEncodingStatus failed");
                    
                    encodingStatusFailures++;
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
            _logger->info(__FILEREF__ + "Overlayed media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
                    + ", sResponse: " + sResponse
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
            );
        }
        catch(MaxConcurrentJobsReached e)
        {
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", response.str(): " + response.str()
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

            throw e;
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return stagingEncodedAssetPathName;
}

pair<int64_t,int64_t> EncoderVideoAudioProxy::processOverlayedImageOnVideo(string stagingEncodedAssetPathName)
{
    pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey;
    
    string encodedFileName;
    string relativePathToBeUsed;
    unsigned long mmsPartitionIndexUsed;
    string mmsAssetPathName;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        relativePathToBeUsed = _mmsEngineDBFacade->nextRelativePathToBeUsed (
                _encodingItem->_workspace->_workspaceKey);
        
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            relativePathToBeUsed,
            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed,
            deliveryRepositoriesToo,
            _encodingItem->_workspace->_territories
            );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }

    MMSEngineDBFacade::ContentType contentType;
    
    int64_t durationInMilliSeconds = -1;
    long bitRate = -1;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate = -1;
    string audioCodecName;
    long audioSampleRate = -1;
    int audioChannels = -1;
    long audioBitRate = -1;

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality = -1;
    try
    {
        FFMpeg ffmpeg (_configuration, _logger);
        tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
            ffmpeg.getMediaInfo(mmsAssetPathName);

        tie(durationInMilliSeconds, bitRate, 
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;

        contentType = MMSEngineDBFacade::ContentType::Video;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }        

    try
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                inCaseOfLinkHasItToBeRead);   

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata..."
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
            + ", relativePathToBeUsed: " + relativePathToBeUsed
            + ", encodedFileName: " + encodedFileName
            + ", mmsPartitionIndexUsed: " + to_string(mmsPartitionIndexUsed)
            + ", sizeInBytes: " + to_string(sizeInBytes)

            + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            + ", bitRate: " + to_string(bitRate)
            + ", videoCodecName: " + videoCodecName
            + ", videoProfile: " + videoProfile
            + ", videoWidth: " + to_string(videoWidth)
            + ", videoHeight: " + to_string(videoHeight)
            + ", videoAvgFrameRate: " + videoAvgFrameRate
            + ", videoBitRate: " + to_string(videoBitRate)
            + ", audioCodecName: " + audioCodecName
            + ", audioSampleRate: " + to_string(audioSampleRate)
            + ", audioChannels: " + to_string(audioChannels)
            + ", audioBitRate: " + to_string(audioBitRate)

            + ", imageWidth: " + to_string(imageWidth)
            + ", imageHeight: " + to_string(imageHeight)
            + ", imageFormat: " + imageFormat
            + ", imageQuality: " + to_string(imageQuality)
        );

        mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveIngestedContentMetadata (
                    _encodingItem->_workspace,
                    _encodingItem->_ingestionJobKey,
                    true, // ingestionRowToBeUpdatedAsSuccess
                    contentType,
                    _encodingItem->_overlayImageOnVideoData->_overlayParametersRoot,
                    relativePathToBeUsed,
                    encodedFileName,
                    mmsPartitionIndexUsed,
                    sizeInBytes,
                
                    // video-audio
                    durationInMilliSeconds,
                    bitRate,
                    videoCodecName,
                    videoProfile,
                    videoWidth,
                    videoHeight,
                    videoAvgFrameRate,
                    videoBitRate,
                    audioCodecName,
                    audioSampleRate,
                    audioChannels,
                    audioBitRate,

                    // image
                    imageWidth,
                    imageHeight,
                    imageFormat,
                    imageQuality
        );

        _logger->info(__FILEREF__ + "Added a new ingested content"
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first)
            + ", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }    

    /*
    int64_t encodedPhysicalPathKey;
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            _encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            _encodingItem->_workspace->_territories
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_relativePath: " + _encodingItem->_relativePath
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_relativePath: " + _encodingItem->_relativePath
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            FileIO::DirectoryEntryType_t detSourceFileType = 
                    FileIO::getDirectoryEntryType(mmsAssetPathName);

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                    detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                string errorMessage = __FILEREF__ + "Wrong directory entry type"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        ;

                _logger->error(errorMessage);
                throw runtime_error(errorMessage);
            }

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                mmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(mmsAssetPathName);   
            }
            else
            {
                bool inCaseOfLinkHasItToBeRead = false;
                mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                        inCaseOfLinkHasItToBeRead);   
            }
        }


        encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            _encodingItem->_workspace->_workspaceKey,
            _encodingItem->_mediaItemKey,
            encodedFileName,
            _encodingItem->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            encodingProfileKey);
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(mmsAssetPathName);
        }

        throw e;
    }
     */
    
    return mediaItemKeyAndPhysicalPathKey;
}

string EncoderVideoAudioProxy::overlayTextOnVideo()
{
    string stagingEncodedAssetPathName;
    
    stagingEncodedAssetPathName = overlayTextOnVideo_through_ffmpeg();
    
    return stagingEncodedAssetPathName;
}

string EncoderVideoAudioProxy::overlayTextOnVideo_through_ffmpeg()
{
    
    int64_t sourceVideoPhysicalPathKey;
    string text;
    string textPosition_X_InPixel;
    string textPosition_Y_InPixel;
    string fontType;
    int fontSize;
    string fontColor;
    int textPercentageOpacity;
    bool boxEnable;
    string boxColor;
    int boxPercentageOpacity;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayTextOnVideoJob
    {
        string field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "text";
        text = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "textPosition_X_InPixel";
        textPosition_X_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "textPosition_Y_InPixel";
        textPosition_Y_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "fontType";
        fontType = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "fontSize";
        fontSize = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "fontColor";
        fontColor = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "textPercentageOpacity";
        textPercentageOpacity = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "boxEnable";
        boxEnable = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "boxColor";
        boxColor = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "boxPercentageOpacity";
        boxPercentageOpacity = _encodingItem->_parametersRoot.get(field, 0).asInt();
    }
    
    string stagingEncodedAssetPathName;
    string encodedFileName;
    string mmsSourceVideoAssetPathName;

    
    #ifdef __LOCALENCODER__
        if (*_pRunningEncodingsNumber > _ffmpegMaxCapacity)
        {
            _logger->info("Max ffmpeg encoder capacity is reached"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );

            throw MaxConcurrentJobsReached();
        }
    #endif

    // stagingEncodedAssetPathName preparation
    {        
        mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_overlayTextOnVideoData->_mmsVideoPartitionNumber,
            _encodingItem->_workspace->_directoryName,
            _encodingItem->_overlayTextOnVideoData->_videoRelativePath,
            _encodingItem->_overlayTextOnVideoData->_videoFileName);

        size_t extensionIndex = _encodingItem->_overlayTextOnVideoData->_videoFileName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extension find in the asset file name"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingItem->_overlayTextOnVideoData->_videoFileName: " + _encodingItem->_overlayTextOnVideoData->_videoFileName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName =
                to_string(_encodingItem->_ingestionJobKey)
                + "_"
                + to_string(_encodingItem->_encodingJobKey)
                + _encodingItem->_overlayTextOnVideoData->_videoFileName.substr(extensionIndex)
                ;

        bool removeLinuxPathIfExist = true;
        stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
            _encodingItem->_workspace->_directoryName,
            "/",    // _encodingItem->_relativePath,
            encodedFileName,
            -1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
            -1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
            removeLinuxPathIfExist);        
    }

    #ifdef __LOCALENCODER__
        (*_pRunningEncodingsNumber)++;

    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + _ffmpegOverlayTextOnVideoURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
                Json::Value overlayTextMedatada;
                
                overlayTextMedatada["mmsSourceVideoAssetPathName"] = mmsSourceVideoAssetPathName;
                overlayTextMedatada["videoDurationInMilliSeconds"] = (Json::LargestUInt) (_encodingItem->_overlayTextOnVideoData->_videoDurationInMilliSeconds);

                overlayTextMedatada["text"] = text;
                overlayTextMedatada["textPosition_X_InPixel"] = textPosition_X_InPixel;
                overlayTextMedatada["textPosition_Y_InPixel"] = textPosition_Y_InPixel;
                overlayTextMedatada["fontType"] = fontType;
                overlayTextMedatada["fontSize"] = fontSize;
                overlayTextMedatada["fontColor"] = fontColor;
                overlayTextMedatada["textPercentageOpacity"] = textPercentageOpacity;
                overlayTextMedatada["boxEnable"] = boxEnable;
                overlayTextMedatada["boxColor"] = boxColor;
                overlayTextMedatada["boxPercentageOpacity"] = boxPercentageOpacity;
                
                overlayTextMedatada["encodedFileName"] = encodedFileName;
                overlayTextMedatada["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                overlayTextMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                overlayTextMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, overlayTextMedatada);
                }
            }
            
            list<string> header;

            header.push_back("Content-Type: application/json");
            {
                string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            if (_ffmpegEncoderProtocol == "https")
            {
                /*
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                    typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                    typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                    typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                 */
                                                                                                  
                
                /*
                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                request.setOpt(sslCert);
                 */

                /*
                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                if(pPassphrase)
                  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
                */
                
                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);
                
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);
                
                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));
            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "OverlayText media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value overlayTextContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &overlayTextContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }               
            }
            catch(runtime_error e)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw e;
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                // same string declared in FFMPEGEncoder.cpp
                string noEncodingAvailableMessage("__NO-ENCODING-AVAILABLE__");
            
                string field = "error";
                if (Validator::isMetadataPresent(overlayTextContentResponse, field))
                {
                    string error = overlayTextContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(noEncodingAvailableMessage) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }                        
                }
                /*
                else
                {
                    string field = "ffmpegEncoderHost";
                    if (Validator::isMetadataPresent(encodeContentResponse, field))
                    {
                        _currentUsedFFMpegEncoderHost = encodeContentResponse.get("ffmpegEncoderHost", "XXX").asString();
                        
                        _logger->info(__FILEREF__ + "Retrieving ffmpegEncoderHost"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
            
            // loop waiting the end of the encoding
            bool encodingFinished = false;
            int maxEncodingStatusFailures = 1;
            int encodingStatusFailures = 0;
            while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
                try
                {
                    encodingFinished = getEncodingStatus(_encodingItem->_encodingJobKey);
                }
                catch(...)
                {                    
                    encodingStatusFailures++;
                    
                    _logger->error(__FILEREF__ + "getEncodingStatus failed"
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    );
                }
            }
            
            // here we do not know if the encoding was successful or not
            // we can just check the encoded file because we know the ffmpeg methods
            // will remove the encoded file in case of failure
            if (!FileIO::fileExisting(stagingEncodedAssetPathName)
                && !FileIO::directoryExisting(stagingEncodedAssetPathName))
            {
                string errorMessage = string("Encoded file was not generated!!!")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                        + ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
            _logger->info(__FILEREF__ + "OverlayedText media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
                    + ", sResponse: " + sResponse
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
            );
        }
        catch(MaxConcurrentJobsReached e)
        {
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", response.str(): " + response.str()
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

            throw e;
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return stagingEncodedAssetPathName;
}

pair<int64_t,int64_t> EncoderVideoAudioProxy::processOverlayedTextOnVideo(string stagingEncodedAssetPathName)
{
    pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey;
    
    string encodedFileName;
    string relativePathToBeUsed;
    unsigned long mmsPartitionIndexUsed;
    string mmsAssetPathName;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        relativePathToBeUsed = _mmsEngineDBFacade->nextRelativePathToBeUsed (
                _encodingItem->_workspace->_workspaceKey);
        
        bool partitionIndexToBeCalculated   = true;
        bool deliveryRepositoriesToo        = true;
        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            relativePathToBeUsed,
            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed,
            deliveryRepositoriesToo,
            _encodingItem->_workspace->_territories
            );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }

    MMSEngineDBFacade::ContentType contentType;
    
    int64_t durationInMilliSeconds = -1;
    long bitRate = -1;
    string videoCodecName;
    string videoProfile;
    int videoWidth = -1;
    int videoHeight = -1;
    string videoAvgFrameRate;
    long videoBitRate = -1;
    string audioCodecName;
    long audioSampleRate = -1;
    int audioChannels = -1;
    long audioBitRate = -1;

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality = -1;
    try
    {
        FFMpeg ffmpeg (_configuration, _logger);
        tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo =
            ffmpeg.getMediaInfo(mmsAssetPathName);

        tie(durationInMilliSeconds, bitRate, 
            videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
            audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;

        contentType = MMSEngineDBFacade::ContentType::Video;
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }        

    try
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                inCaseOfLinkHasItToBeRead);   

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata..."
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", contentType: " + MMSEngineDBFacade::toString(contentType)
            + ", relativePathToBeUsed: " + relativePathToBeUsed
            + ", encodedFileName: " + encodedFileName
            + ", mmsPartitionIndexUsed: " + to_string(mmsPartitionIndexUsed)
            + ", sizeInBytes: " + to_string(sizeInBytes)

            + ", durationInMilliSeconds: " + to_string(durationInMilliSeconds)
            + ", bitRate: " + to_string(bitRate)
            + ", videoCodecName: " + videoCodecName
            + ", videoProfile: " + videoProfile
            + ", videoWidth: " + to_string(videoWidth)
            + ", videoHeight: " + to_string(videoHeight)
            + ", videoAvgFrameRate: " + videoAvgFrameRate
            + ", videoBitRate: " + to_string(videoBitRate)
            + ", audioCodecName: " + audioCodecName
            + ", audioSampleRate: " + to_string(audioSampleRate)
            + ", audioChannels: " + to_string(audioChannels)
            + ", audioBitRate: " + to_string(audioBitRate)

            + ", imageWidth: " + to_string(imageWidth)
            + ", imageHeight: " + to_string(imageHeight)
            + ", imageFormat: " + imageFormat
            + ", imageQuality: " + to_string(imageQuality)
        );

        mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveIngestedContentMetadata (
                    _encodingItem->_workspace,
                    _encodingItem->_ingestionJobKey,
                    true, // ingestionRowToBeUpdatedAsSuccess
                    contentType,
                    _encodingItem->_overlayTextOnVideoData->_overlayTextParametersRoot,
                    relativePathToBeUsed,
                    encodedFileName,
                    mmsPartitionIndexUsed,
                    sizeInBytes,
                
                    // video-audio
                    durationInMilliSeconds,
                    bitRate,
                    videoCodecName,
                    videoProfile,
                    videoWidth,
                    videoHeight,
                    videoAvgFrameRate,
                    videoBitRate,
                    audioCodecName,
                    audioSampleRate,
                    audioChannels,
                    audioBitRate,

                    // image
                    imageWidth,
                    imageHeight,
                    imageFormat,
                    imageQuality
        );

        _logger->info(__FILEREF__ + "Added a new ingested content"
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", mediaItemKey: " + to_string(mediaItemKeyAndPhysicalPathKey.first)
            + ", physicalPathKey: " + to_string(mediaItemKeyAndPhysicalPathKey.second)
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveIngestedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }    

    /*
    int64_t encodedPhysicalPathKey;
    string encodedFileName;
    string mmsAssetPathName;
    unsigned long mmsPartitionIndexUsed;
    try
    {
        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodedFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        bool partitionIndexToBeCalculated = true;
        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
            stagingEncodedAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            _encodingItem->_relativePath,

            partitionIndexToBeCalculated,
            &mmsPartitionIndexUsed, // OUT if bIsPartitionIndexToBeCalculated is true, IN is bIsPartitionIndexToBeCalculated is false

            deliveryRepositoriesToo,
            _encodingItem->_workspace->_territories
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_relativePath: " + _encodingItem->_relativePath
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _encodingItem->_workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodingItem->_relativePath: " + _encodingItem->_relativePath
        );

        throw e;
    }

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            FileIO::DirectoryEntryType_t detSourceFileType = 
                    FileIO::getDirectoryEntryType(mmsAssetPathName);

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType != FileIO::TOOLS_FILEIO_DIRECTORY &&
                    detSourceFileType != FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                string errorMessage = __FILEREF__ + "Wrong directory entry type"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", mmsAssetPathName: " + mmsAssetPathName
                        ;

                _logger->error(errorMessage);
                throw runtime_error(errorMessage);
            }

            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                mmsAssetSizeInBytes = FileIO::getDirectorySizeInBytes(mmsAssetPathName);   
            }
            else
            {
                bool inCaseOfLinkHasItToBeRead = false;
                mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                        inCaseOfLinkHasItToBeRead);   
            }
        }


        encodedPhysicalPathKey = _mmsEngineDBFacade->saveEncodedContentMetadata(
            _encodingItem->_workspace->_workspaceKey,
            _encodingItem->_mediaItemKey,
            encodedFileName,
            _encodingItem->_relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            encodingProfileKey);
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveEncodedContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

        FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(mmsAssetPathName);

        _logger->info(__FILEREF__ + "Remove"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", mmsAssetPathName: " + mmsAssetPathName
        );

        // file in case of .3gp content OR directory in case of IPhone content
        if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
        {
            Boolean_t bRemoveRecursively = true;
            FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
        }
        else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
        {
            FileIO::remove(mmsAssetPathName);
        }

        throw e;
    }
     */
    
    return mediaItemKeyAndPhysicalPathKey;
}

void EncoderVideoAudioProxy::generateFrames()
{    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingItem->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
        + ", _mp4Encoder: " + _mp4Encoder
    );

    if (
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MP4 &&
            _mp4Encoder == "FFMPEG") ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::MPEG2_TS &&
            _mpeg2TSEncoder == "FFMPEG") ||
        _encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WEBM ||
        (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::Adobe &&
            _mpeg2TSEncoder == "FFMPEG")
    )
    {
    */
        generateFrames_through_ffmpeg();
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */    
}

void EncoderVideoAudioProxy::generateFrames_through_ffmpeg()
{
    
    string imageDirectory;
    string imageFileName;
    double startTimeInSeconds;
    int maxFramesNumber;  
    string videoFilter;
    int periodInSeconds;
    bool mjpeg;
    int imageWidth;
    int imageHeight;
    int64_t ingestionJobKey;
    int64_t videoDurationInMilliSeconds;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "imageDirectory";
        imageDirectory = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "imageFileName";
        imageFileName = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "startTimeInSeconds";
        startTimeInSeconds = _encodingItem->_parametersRoot.get(field, 0).asDouble();

        field = "maxFramesNumber";
        maxFramesNumber = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "videoFilter";
        videoFilter = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "periodInSeconds";
        periodInSeconds = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "mjpeg";
        mjpeg = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "imageWidth";
        imageWidth = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "imageHeight";
        imageHeight = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "ingestionJobKey";
        ingestionJobKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "videoDurationInMilliSeconds";
        videoDurationInMilliSeconds = _encodingItem->_parametersRoot.get(field, 0).asInt64();
    }
    
    string mmsSourceVideoAssetPathName;

    
    #ifdef __LOCALENCODER__
        if (*_pRunningEncodingsNumber > _ffmpegMaxCapacity)
        {
            _logger->info("Max ffmpeg encoder capacity is reached"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            );

            throw MaxConcurrentJobsReached();
        }
    #endif

    // stagingEncodedAssetPathName preparation
    {        
        mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
            _encodingItem->_generateFramesData->_mmsVideoPartitionNumber,
            _encodingItem->_workspace->_directoryName,
            _encodingItem->_generateFramesData->_videoRelativePath,
            _encodingItem->_generateFramesData->_videoFileName);
    }

    #ifdef __LOCALENCODER__
        (*_pRunningEncodingsNumber)++;

    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + _ffmpegGenerateFramesURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
                Json::Value generateFramesMedatada;
                
                generateFramesMedatada["imageDirectory"] = imageDirectory;
                generateFramesMedatada["imageFileName"] = imageFileName;
                generateFramesMedatada["startTimeInSeconds"] = startTimeInSeconds;
                generateFramesMedatada["maxFramesNumber"] = maxFramesNumber;
                generateFramesMedatada["videoFilter"] = videoFilter;
                generateFramesMedatada["periodInSeconds"] = periodInSeconds;
                generateFramesMedatada["mjpeg"] = mjpeg;
                generateFramesMedatada["imageWidth"] = imageWidth;
                generateFramesMedatada["imageHeight"] = imageHeight;
                generateFramesMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
                generateFramesMedatada["mmsSourceVideoAssetPathName"] = mmsSourceVideoAssetPathName;
                generateFramesMedatada["videoDurationInMilliSeconds"] = (Json::LargestUInt) (videoDurationInMilliSeconds);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, generateFramesMedatada);
                }
            }
            
            list<string> header;

            header.push_back("Content-Type: application/json");
            {
                string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

            if (_ffmpegEncoderProtocol == "https")
            {
                /*
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                    typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                    typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                    typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                 */
                                                                                                  
                
                /*
                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                request.setOpt(sslCert);
                 */

                /*
                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                if(pPassphrase)
                  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
                */
                
                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);
                
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);
                
                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }
            request.setOpt(new curlpp::options::HttpHeader(header));
            request.setOpt(new curlpp::options::PostFields(body));
            request.setOpt(new curlpp::options::PostFieldSize(body.length()));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "Generating Frames"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value generateFramesContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &generateFramesContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }               
            }
            catch(runtime_error e)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw e;
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                // same string declared in FFMPEGEncoder.cpp
                string noEncodingAvailableMessage("__NO-ENCODING-AVAILABLE__");
            
                string field = "error";
                if (Validator::isMetadataPresent(generateFramesContentResponse, field))
                {
                    string error = generateFramesContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(noEncodingAvailableMessage) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }                        
                }
                /*
                else
                {
                    string field = "ffmpegEncoderHost";
                    if (Validator::isMetadataPresent(encodeContentResponse, field))
                    {
                        _currentUsedFFMpegEncoderHost = encodeContentResponse.get("ffmpegEncoderHost", "XXX").asString();
                        
                        _logger->info(__FILEREF__ + "Retrieving ffmpegEncoderHost"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
            
            // loop waiting the end of the encoding
            bool encodingFinished = false;
            int maxEncodingStatusFailures = 1;
            int encodingStatusFailures = 0;
            while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
                try
                {
                    encodingFinished = getEncodingStatus(_encodingItem->_encodingJobKey);
                }
                catch(...)
                {
                    _logger->error(__FILEREF__ + "getEncodingStatus failed");
                    
                    encodingStatusFailures++;
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
            _logger->info(__FILEREF__ + "Generated Frames"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
                    + ", sResponse: " + sResponse
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
            );
        }
        catch(MaxConcurrentJobsReached e)
        {
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", response.str(): " + response.str()
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

            throw e;
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif
}

void EncoderVideoAudioProxy::processGeneratedFrames()
{
    shared_ptr<MultiLocalAssetIngestionEvent>    multiLocalAssetIngestionEvent = _multiEventsSet->getEventsFactory()
            ->getFreeEvent<MultiLocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT);

    multiLocalAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
    multiLocalAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
    multiLocalAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

    multiLocalAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
    multiLocalAssetIngestionEvent->setEncodingJobKey(_encodingItem->_encodingJobKey);
    multiLocalAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
    multiLocalAssetIngestionEvent->setParametersRoot(_encodingItem->_generateFramesData->_generateFramesParametersRoot);

    shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(multiLocalAssetIngestionEvent);
    _multiEventsSet->addEvent(event);

    _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (MULTIINGESTASSETEVENT)"
        + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        + ", getEventKey().first: " + to_string(event->getEventKey().first)
        + ", getEventKey().second: " + to_string(event->getEventKey().second));
}


int EncoderVideoAudioProxy::getEncodingProgress(int64_t encodingJobKey)
{
    int encodingProgress = 0;
    
    #ifdef __LOCALENCODER__
        try
        {
            encodingProgress = _ffmpeg->getEncodingProgress();
        }
        catch(FFMpegEncodingStatusNotAvailable e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->getEncodingProgress failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", e.what(): " + e.what()
            );

            throw EncodingStatusNotAvailable();
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_ffmpeg->getEncodingProgress failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", e.what(): " + e.what()
            );

            throw e;
        }
    #else
        string ffmpegEncoderURL;
        ostringstream response;
        try
        {
            if (_currentUsedFFMpegEncoderHost == "")
            {
                string errorMessage = __FILEREF__ + "no _currentUsedFFMpegEncoderHost initialized"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
            
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + _ffmpegEncoderProgressURI
                    + "/" + to_string(encodingJobKey)
            ;
            
            list<string> header;

            {
                string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
                string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

                header.push_back(basicAuthorization);
            }
            
            curlpp::Cleanup cleaner;
            curlpp::Easy request;

            // Setting the URL to retrive.
            request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));
            if (_ffmpegEncoderProtocol == "https")
            {
                /*
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                    typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                    typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                    typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                    typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                    typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                 */
                                                                                                  
                
                /*
                // cert is stored PEM coded in file... 
                // since PEM is default, we needn't set it for PEM 
                // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
                curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
                equest.setOpt(sslCertType);

                // set the cert for client authentication
                // "testcert.pem"
                // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
                curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
                request.setOpt(sslCert);
                 */

                /*
                // sorry, for engine we must set the passphrase
                //   (if the key has one...)
                // const char *pPassphrase = NULL;
                if(pPassphrase)
                  curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

                // if we use a key stored in a crypto engine,
                //   we must set the key type to "ENG"
                // pKeyType  = "PEM";
                curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

                // set the private key (file or ID in engine)
                // pKeyName  = "testkey.pem";
                curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

                // set the file with the certs vaildating the server
                // *pCACertFile = "cacert.pem";
                curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
                */
                
                // disconnect if we can't validate server's cert
                bool bSslVerifyPeer = false;
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
                request.setOpt(sslVerifyPeer);
                
                curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
                request.setOpt(sslVerifyHost);
                
                // request.setOpt(new curlpp::options::SslEngineDefault());                                              

            }

            request.setOpt(new curlpp::options::HttpHeader(header));

            request.setOpt(new curlpp::options::WriteStream(&response));

            chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

            _logger->info(__FILEREF__ + "getEncodingProgress"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
            );
            request.perform();
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            _logger->info(__FILEREF__ + "getEncodingProgress"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", response.str: " + response.str()
            );
            
            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();
            
            try
            {
                Json::Value encodeProgressResponse;
                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &encodeProgressResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", errors: " + errors
                            + ", sResponse: " + sResponse
                            ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);
                }
                
                string field = "error";
                if (Validator::isMetadataPresent(encodeProgressResponse, field))
                {
                    string error = encodeProgressResponse.get(field, "XXX").asString();
                    
                    // same string declared in FFMPEGEncoder.cpp
                    string noEncodingJobKeyFound("__NO-ENCODINGJOBKEY-FOUND__");
            
                    if (error.find(noEncodingJobKeyFound) != string::npos)
                    {
                        string errorMessage = string("No EncodingJobKey found")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw NoEncodingJobKeyFound();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }                        
                }
                else
                {
                    string field = "encodingProgress";
                    if (Validator::isMetadataPresent(encodeProgressResponse, field))
                    {
                        encodingProgress = encodeProgressResponse.get("encodingProgress", "XXX").asInt();
                        
                        _logger->info(__FILEREF__ + "Retrieving encodingProgress"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + ", encodingProgress: " + to_string(encodingProgress)
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                            + ", _encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }                        
            }
            catch(NoEncodingJobKeyFound e)
            {
                string errorMessage = string("NoEncodingJobKeyFound")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->warn(__FILEREF__ + errorMessage);

                throw NoEncodingJobKeyFound();
            }
            catch(runtime_error e)
            {
                string errorMessage = string("runtime_error")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", sResponse: " + sResponse
                        + ", e.what(): " + e.what()
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
            catch(...)
            {
                string errorMessage = string("response Body json is not well format")
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }
        }
        catch (curlpp::LogicError & e) 
        {
            _logger->error(__FILEREF__ + "Progress URL failed (LogicError)"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );
            
            throw e;
        }
        catch (curlpp::RuntimeError & e) 
        { 
            string errorMessage = string("Progress URL failed (RuntimeError)")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            ;
            
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
        catch (NoEncodingJobKeyFound e)
        {
            _logger->warn(__FILEREF__ + "Progress URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Progress URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Progress URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", encodingJobKey: " + to_string(encodingJobKey) 
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
                + ", response.str(): " + response.str()
            );

            throw e;
        }
    #endif

    return encodingProgress;
}

bool EncoderVideoAudioProxy::getEncodingStatus(int64_t encodingJobKey)
{
    bool encodingFinished;
    
    string ffmpegEncoderURL;
    ostringstream response;
    try
    {
        ffmpegEncoderURL = 
                _ffmpegEncoderProtocol
                + "://"                
                + _currentUsedFFMpegEncoderHost + ":"
                + to_string(_ffmpegEncoderPort)
                + _ffmpegEncoderStatusURI
                + "/" + to_string(encodingJobKey)
        ;

        list<string> header;

        {
            string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":" + _ffmpegEncoderPassword);
            string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

            header.push_back(basicAuthorization);
        }

        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        // Setting the URL to retrive.
        request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

        if (_ffmpegEncoderProtocol == "https")
        {
            /*
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
             */


            /*
            // cert is stored PEM coded in file... 
            // since PEM is default, we needn't set it for PEM 
            // curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
            curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
            equest.setOpt(sslCertType);

            // set the cert for client authentication
            // "testcert.pem"
            // curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
            curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
            request.setOpt(sslCert);
             */

            /*
            // sorry, for engine we must set the passphrase
            //   (if the key has one...)
            // const char *pPassphrase = NULL;
            if(pPassphrase)
              curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

            // if we use a key stored in a crypto engine,
            //   we must set the key type to "ENG"
            // pKeyType  = "PEM";
            curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

            // set the private key (file or ID in engine)
            // pKeyName  = "testkey.pem";
            curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

            // set the file with the certs vaildating the server
            // *pCACertFile = "cacert.pem";
            curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
            */

            // disconnect if we can't validate server's cert
            bool bSslVerifyPeer = false;
            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYPEER> sslVerifyPeer(bSslVerifyPeer);
            request.setOpt(sslVerifyPeer);

            curlpp::OptionTrait<bool, CURLOPT_SSL_VERIFYHOST> sslVerifyHost(0L);
            request.setOpt(sslVerifyHost);

            // request.setOpt(new curlpp::options::SslEngineDefault());                                              

        }

        request.setOpt(new curlpp::options::HttpHeader(header));

        request.setOpt(new curlpp::options::WriteStream(&response));

        chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getEncodingStatus"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL
        );
        request.perform();
        chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
        _logger->info(__FILEREF__ + "getEncodingStatus"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
        );

        string sResponse = response.str();
        // LF and CR create problems to the json parser...
        while (sResponse.back() == 10 || sResponse.back() == 13)
            sResponse.pop_back();
            
        try
        {
            Json::Value encodeStatusResponse;

            Json::CharReaderBuilder builder;
            Json::CharReader* reader = builder.newCharReader();
            string errors;

            bool parsingSuccessful = reader->parse(sResponse.c_str(),
                    sResponse.c_str() + sResponse.size(), 
                    &encodeStatusResponse, &errors);
            delete reader;

            if (!parsingSuccessful)
            {
                string errorMessage = __FILEREF__ + "failed to parse the response body"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", errors: " + errors
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

            encodingFinished = encodeStatusResponse.get("encodingFinished", "XXX").asBool();
        }
        catch(...)
        {
            string errorMessage = string("response Body json is not well format")
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", sResponse: " + sResponse
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Progress URL failed (LogicError)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
            + ", response.str(): " + response.str()
        );

        throw e;
    }
    catch (curlpp::RuntimeError & e) 
    { 
        string errorMessage = string("Progress URL failed (RuntimeError)")
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
            + ", response.str(): " + response.str()
        ;

        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "Progress URL failed (exception)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
            + ", response.str(): " + response.str()
        );

        throw e;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Progress URL failed (exception)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
            + ", response.str(): " + response.str()
        );

        throw e;
    }

    return encodingFinished;
}

