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
// #include <sstream>
#ifdef __LOCALENCODER__
#else
    #include <curlpp/cURLpp.hpp>
    #include <curlpp/Easy.hpp>
    #include <curlpp/Options.hpp>
    #include <curlpp/Exception.hpp>
    #include <curlpp/Infos.hpp>
#endif
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/System.h"
#include "LocalAssetIngestionEvent.h"
#include "MultiLocalAssetIngestionEvent.h"
#include "catralibraries/Convert.h"
#include "Validator.h"
#include "FFMpeg.h"
#include "EncoderVideoAudioProxy.h"
#include "opencv2/objdetect.hpp"
#include "opencv2/face.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"


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
		shared_ptr<long> faceRecognitionNumber,
		int maxFaceRecognitionNumber,
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
    
	_faceRecognitionNumber	= faceRecognitionNumber;
	_maxFaceRecognitionNumber	= maxFaceRecognitionNumber;

	_hostName				= System::getHostName();

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
    _maxSecondsToWaitUpdateEncodingJobLock         = _configuration["mms"]["locks"].get("maxSecondsToWaitUpdateEncodingJobLock", 30).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
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
    _ffmpegSlideShowURI = _configuration["ffmpeg"].get("slideShowURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->slideShowURI: " + _ffmpegSlideShowURI
    );
    _ffmpegLiveRecorderURI = _configuration["ffmpeg"].get("liveRecorderURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderURI: " + _ffmpegLiveRecorderURI
    );
    _ffmpegVideoSpeedURI = _configuration["ffmpeg"].get("videoSpeedURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->videoSpeedURI: " + _ffmpegVideoSpeedURI
    );
    _ffmpegPictureInPictureURI = _configuration["ffmpeg"].get("pictureInPictureURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->pictureInPictureURI: " + _ffmpegPictureInPictureURI
    );
            
    _computerVisionCascadePath             = configuration["computerVision"].get("cascadePath", "XXX").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->cascadePath: " + _computerVisionCascadePath
    );
	if (_computerVisionCascadePath.back() == '/')
		_computerVisionCascadePath.pop_back();
    _computerVisionDefaultScale				= configuration["computerVision"].get("defaultScale", 1.1).asDouble();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultScale: " + to_string(_computerVisionDefaultScale)
    );
    _computerVisionDefaultMinNeighbors		= configuration["computerVision"].get("defaultMinNeighbors", 2).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultMinNeighbors: " + to_string(_computerVisionDefaultMinNeighbors)
    );
    _computerVisionDefaultTryFlip		= configuration["computerVision"].get("defaultTryFlip", 2).asBool();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultTryFlip: " + to_string(_computerVisionDefaultTryFlip)
    );

	_timeBeforeToPrepareResourcesInMinutes		= configuration["mms"].get("liveRecording_timeBeforeToPrepareResourcesInMinutes", 2).asInt();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", mms->liveRecording_timeBeforeToPrepareResourcesInMinutes: " + to_string(_timeBeforeToPrepareResourcesInMinutes)
	);

    _waitingNFSSync_attemptNumber = configuration["storage"].
		get("waitingNFSSync_attemptNumber", 1).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_attemptNumber: " + to_string(_waitingNFSSync_attemptNumber)
    );
    _waitingNFSSync_sleepTimeInSeconds = configuration["storage"].
		get("waitingNFSSync_sleepTimeInSeconds", 3).asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_sleepTimeInSeconds: "
			+ to_string(_waitingNFSSync_sleepTimeInSeconds)
    );

	/*
    _mmsAPIProtocol = _configuration["api"].get("protocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->protocol: " + _mmsAPIProtocol
    );
    _mmsAPIHostname = _configuration["api"].get("hostname", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->hostname: " + _mmsAPIHostname
    );
    _mmsAPIPort = _configuration["api"].get("port", "").asInt();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->port: " + to_string(_mmsAPIPort)
    );
    _mmsAPIUser = _configuration["api"].get("user", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->user: " + _mmsAPIUser
    );
    _mmsAPIPassword = _configuration["api"].get("password", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->password: " + "..."
    );
    _mmsAPIIngestionURI = _configuration["api"].get("ingestionURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", api->ingestionURI: " + _mmsAPIIngestionURI
    );
	*/

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
        + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
        + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
        + ", _encodingParameters: " + _encodingItem->_encodingParameters
    );

    string stagingEncodedAssetPathName;
	bool killedByUser;
	bool main = true;
    try
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = encodeContentVideoAudio();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = overlayImageOnVideo();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = overlayTextOnVideo();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            killedByUser = generateFrames();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::SlideShow)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = slideShow();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceRecognition)
        {
            stagingEncodedAssetPathName = faceRecognition();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceIdentification)
        {
            stagingEncodedAssetPathName = faceIdentification();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
        {
			tuple<bool, bool> killedByUserAndMain = liveRecorder();
			tie(killedByUser, main) = killedByUserAndMain;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VideoSpeed)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = videoSpeed();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
        {
			pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = pictureInPicture();
			tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
        }
        else
        {
            string errorMessage = string("Wrong EncodingType")
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                    ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(MaxConcurrentJobsReached e)
    {
		_logger->warn(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
		);

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob MaxCapacityReached"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", main: " + to_string(main)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			_mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", main: " + to_string(main)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", e.what(): " + e.what()
			);
		}
		catch(exception e)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", main: " + to_string(main)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", e.what(): " + e.what()
			);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(EncoderError e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
	catch(EncodingKilledByUser e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob KilledByUser"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::KilledByUser, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob KilledByUser FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
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
                stagingEncodedAssetPathName, killedByUser);
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
//            pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey = processOverlayedImageOnVideo(
//                stagingEncodedAssetPathName);

            processOverlayedImageOnVideo(stagingEncodedAssetPathName, killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;
            
//            mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;
//            encodedPhysicalPathKey = mediaItemKeyAndPhysicalPathKey.second;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            /*
            pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey = processOverlayedTextOnVideo(
                stagingEncodedAssetPathName);
            
            mediaItemKey = mediaItemKeyAndPhysicalPathKey.first;
            encodedPhysicalPathKey = mediaItemKeyAndPhysicalPathKey.second;
             */
            processOverlayedTextOnVideo(stagingEncodedAssetPathName, killedByUser);     
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            processGeneratedFrames(killedByUser);     
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::SlideShow)
        {
            processSlideShow(stagingEncodedAssetPathName, killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceRecognition)
        {
            processFaceRecognition(stagingEncodedAssetPathName);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceIdentification)
        {
            processFaceIdentification(stagingEncodedAssetPathName);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
        {
            processLiveRecorder(killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VideoSpeed)
        {
            processVideoSpeed(stagingEncodedAssetPathName, killedByUser);     
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
        {
            processPictureInPicture(stagingEncodedAssetPathName, killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;
        }
        else
        {
            string errorMessage = string("Wrong EncodingType")
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
                ;
            
            _logger->error(__FILEREF__ + errorMessage);
            
            throw runtime_error(errorMessage);
        }
    }
    catch(runtime_error e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

        if (FileIO::fileExisting(stagingEncodedAssetPathName) || 
                FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->error(__FILEREF__ + "Remove"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                Boolean_t bRemoveRecursively = true;
                _logger->info(__FILEREF__ + "removeDirectory"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    catch(exception e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

        if (FileIO::fileExisting(stagingEncodedAssetPathName)
                || FileIO::directoryExisting(stagingEncodedAssetPathName))
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(stagingEncodedAssetPathName);

            _logger->error(__FILEREF__ + "Remove"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            );

            // file in case of .3gp content OR directory in case of IPhone content
            if (detSourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
            {
                _logger->info(__FILEREF__ + "removeDirectory"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(stagingEncodedAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "remove"
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                );
                FileIO::remove(stagingEncodedAssetPathName);
            }
        }

		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
			);
		}

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }

    try
    {
        _logger->info(__FILEREF__ + "updateEncodingJob NoError"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

		// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
		// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
		// 'no update is done'
        _mmsEngineDBFacade->updateEncodingJob (
            _encodingItem->_encodingJobKey, 
            MMSEngineDBFacade::EncodingError::NoError,
            mediaItemKey, encodedPhysicalPathKey,
           main ? _encodingItem->_ingestionJobKey : -1);
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "updateEncodingJob failed: " + e.what()
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
        );

        {
            lock_guard<mutex> locker(*_mtEncodingJobs);

            *_status = EncodingJobStatus::Free;
        }

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
        );

        // throw e;
        return;
    }
    
    {
        lock_guard<mutex> locker(*_mtEncodingJobs);

        *_status = EncodingJobStatus::Free;
    }        
    
    _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
        + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
        + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
        + ", _encodingParameters: " + _encodingItem->_encodingParameters
    );
        
}

pair<string, bool> EncoderVideoAudioProxy::encodeContentVideoAudio()
{
	pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;

	_logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
		+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
		+ ", _encodeData->_encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodeData->_encodingProfileTechnology))
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
        stagingEncodedAssetPathNameAndKilledByUser = encodeContent_VideoAudio_through_ffmpeg();
		if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
                ;
			_logger->error(errorMessage);
        
			throw EncodingKilledByUser();
		}
    }
    else if (_encodingItem->_encodeData->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
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
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegEncodeURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "getEncoderHost"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				string encodedFileName;
				string mmsSourceAssetPathName;

    
				_logger->info(__FILEREF__ + "building body for encoder 1"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
					+ ", _fileName: " + _encodingItem->_encodeData->_fileName
					+ ", _encodingProfileTechnology: " + to_string(static_cast<int>(_encodingItem->_encodeData->_encodingProfileTechnology))
					+ ", _directoryName: " + _encodingItem->_workspace->_directoryName
					+ ", _durationInMilliSeconds: " + to_string(_encodingItem->_encodeData->_durationInMilliSeconds)
					+ ", _contentType: " + to_string(static_cast<int>(_encodingItem->_encodeData->_contentType))
					+ ", _relativePath: " + _encodingItem->_encodeData->_relativePath
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", _encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
				);

				// stagingEncodedAssetPathName preparation
				{
					/*
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
+ ", 1: "
);
// 2019-09-14: crashed after the above message
*/
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(sourcePhysicalPathKey);
					tie(mmsSourceAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;
/*
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", 2: "
);
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
+ ", _encodingItem->_encodeData->_fileName: " + _encodingItem->_encodeData->_fileName
);
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", 3: "
);
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", sourcePhysicalPathKey: " + to_string(sourcePhysicalPathKey)
+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
+ ", _encodingItem->_encodeData->_fileName: " + _encodingItem->_encodeData->_fileName
);
*/
					size_t extensionIndex = _encodingItem->_encodeData->_fileName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _encodeData->_fileName: " + _encodingItem->_encodeData->_fileName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

/*
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", 4: "
);
*/
					encodedFileName =
						to_string(_encodingItem->_ingestionJobKey)
						+ "_"
						+ to_string(_encodingItem->_encodingJobKey)
						+ "_" 
						+ to_string(encodingProfileKey);
/*
_logger->info(__FILEREF__ + "building body for encoder 2"
+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
+ ", 5: "
);
*/
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
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
/*
				_logger->info(__FILEREF__ + "building body for encoder 3"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
					+ ", encodedFileName: " + encodedFileName

					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);
*/


					bool removeLinuxPathIfExist = true;
					bool neededForTranscoder = false;
					stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
						neededForTranscoder,
						_encodingItem->_workspace->_directoryName,
						to_string(_encodingItem->_encodingJobKey),
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
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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

				_logger->info(__FILEREF__ + "building body for encoder 2"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", mmsSourceAssetPathName: " + mmsSourceAssetPathName
					+ ", encodedFileName: " + encodedFileName

					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);

                Json::Value encodingMedatada;
                
                encodingMedatada["mmsSourceAssetPathName"] = mmsSourceAssetPathName;
                encodingMedatada["durationInMilliSeconds"] = (Json::LargestUInt) (_encodingItem->_encodeData->_durationInMilliSeconds);
                // encodingMedatada["encodedFileName"] = encodedFileName;
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
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                    + ", errors: " + errors
                                    + ", _encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
                                    ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);
                        }
                    }
                    catch(...)
                    {
                        string errorMessage = string("_encodingItem->_jsonProfile json is not well format")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", _encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
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

            _logger->info(__FILEREF__ + "Encoding media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
					+ ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(encodeContentResponse, field))
                {
                    // remove the staging directory created just for this encoding
					/* 2019-05-24: Commented because the remove is already done in the exception generated
					 * by this error
                    {
                        size_t directoryEndIndex = stagingEncodedAssetPathName.find_last_of("/");
                        if (directoryEndIndex == string::npos)
                        {
                            string errorMessage = __FILEREF__ + "No directory found in the staging asset path name"
                                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
                            _logger->error(errorMessage);

                            // throw runtime_error(errorMessage);
                        }
                        else
                        {
                            string stagingDirectory = stagingEncodedAssetPathName.substr(0, directoryEndIndex);
                            
                            try
                            {
                                _logger->info(__FILEREF__ + "removeDirectory"
                                    + ", stagingDirectory: " + stagingDirectory
                                );
                                Boolean_t bRemoveRecursively = true;
                                FileIO::removeDirectory(stagingDirectory, bRemoveRecursively);
                            }
                            catch (runtime_error e)
                            {
                                _logger->warn(__FILEREF__ + "FileIO::removeDirectory failed (runtime_error)"
                                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                    + ", stagingDirectory: " + stagingDirectory
                                    + ", exception: " + e.what()
                                    + ", response.str(): " + response.str()
                                );
                            }
                        }
                    }
					*/
                    
                    string error = encodeContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "Encode content. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		bool killedByUser = false;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
               
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{
				encodingStatusFailures++;

				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
            
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "Encoded media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(stagingEncodedAssetPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

		throw e;
	}
}

int64_t EncoderVideoAudioProxy::processEncodedContentVideoAudio(
		string stagingEncodedAssetPathName,
		bool killedByUser)
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
        _logger->info(__FILEREF__ + "Calling ffmpeg.getMediaInfo"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );
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
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
            + ", e.what(): " + e.what()
        );

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName =
						stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "EncoderVideoAudioProxy::getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
        );

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

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
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

			/*
			 * 2019-05-24: commented because the remove directory is already done into the exception
			 *
			if (stagingEncodedAssetPathName != "")
			{
				string directoryPathName;
				try
				{
					size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
					if (endOfDirectoryIndex != string::npos)
					{
						directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

						_logger->info(__FILEREF__ + "removeDirectory"
							+ ", directoryPathName: " + directoryPathName
						);
						Boolean_t bRemoveRecursively = true;
						FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
					}
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "removeDirectory failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", directoryPathName: " + directoryPathName
						+ ", exception: " + e.what()
					);
				}
			}
			*/

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
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
            + ", e.what(): " + e.what()
        );

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", _encodeData->_relativePath: " + _encodingItem->_encodeData->_relativePath
        );

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

        throw e;
    }

	// remove staging directory
	{
		string directoryPathName;
		try
		{
			size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex != string::npos)
			{
				directoryPathName = stagingEncodedAssetPathName.substr(0,
						endOfDirectoryIndex);

				_logger->info(__FILEREF__ + "removeDirectory"
					+ ", directoryPathName: " + directoryPathName
				);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
			}
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + "removeDirectory failed"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				+ ", directoryPathName: " + directoryPathName
				+ ", exception: " + e.what()
			);
		}
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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


		bool externalReadOnlyStorage = false;
		string externalDeliveryTechnology;
		string externalDeliveryURL;
		int64_t liveRecordingIngestionJobKey = -1;
        encodedPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
            _encodingItem->_workspace->_workspaceKey,
			_encodingItem->_ingestionJobKey,
			liveRecordingIngestionJobKey,
            _encodingItem->_encodeData->_mediaItemKey,
			externalReadOnlyStorage,
			externalDeliveryTechnology,
			externalDeliveryURL,
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
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
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
                _logger->info(__FILEREF__ + "removeDirectory"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                );
                Boolean_t bRemoveRecursively = true;
                FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
            }
            else if (detSourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
            {
                _logger->info(__FILEREF__ + "remove"
                    + ", mmsAssetPathName: " + mmsAssetPathName
                );
                FileIO::remove(mmsAssetPathName);
            }
        }

		if (stagingEncodedAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

					_logger->info(__FILEREF__ + "removeDirectory"
						+ ", directoryPathName: " + directoryPathName
					);
					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(directoryPathName, bRemoveRecursively);
				}
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "removeDirectory failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", directoryPathName: " + directoryPathName
					+ ", exception: " + e.what()
				);
			}
		}

        throw e;
    }
    
    return encodedPhysicalPathKey;
}

pair<string, bool> EncoderVideoAudioProxy::overlayImageOnVideo()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
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
        stagingEncodedAssetPathNameAndKilledByUser = overlayImageOnVideo_through_ffmpeg();
		if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
                ;
			_logger->error(errorMessage);
        
			throw EncodingKilledByUser();
		}
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::overlayImageOnVideo_through_ffmpeg()
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
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegOverlayImageOnVideoURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;
		bool killedByUser = false;

		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsSourceVideoAssetPathName;
				string mmsSourceImageAssetPathName;

    
				// stagingEncodedAssetPathName preparation
				{        
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_video =
						_mmsStorage->getPhysicalPath(sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_video;

					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_image =
						_mmsStorage->getPhysicalPath(sourceImagePhysicalPathKey);
					tie(mmsSourceImageAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_image;

					/*
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
					*/

					size_t extensionIndex = _encodingItem->_overlayImageOnVideoData->_videoFileName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _overlayImageOnVideoData->_videoFileName: " + _encodingItem->_overlayImageOnVideoData->_videoFileName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					/*
					encodedFileName =
					to_string(_encodingItem->_ingestionJobKey)
					+ "_"
					+ to_string(_encodingItem->_encodingJobKey)
					+ _encodingItem->_overlayImageOnVideoData->_videoFileName.substr(extensionIndex)
					;
					*/

					string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
						_encodingItem->_workspace);
					stagingEncodedAssetPathName = 
						workspaceIngestionRepository + "/" 
						+ to_string(_encodingItem->_ingestionJobKey)
						+ "_overlayedimage"
						+ _encodingItem->_overlayImageOnVideoData->_videoFileName.substr(extensionIndex)
					;
					/*
					bool removeLinuxPathIfExist = true;
					stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					_encodingItem->_workspace->_directoryName,
					"/",    // _encodingItem->_relativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
					removeLinuxPathIfExist);        
					*/
				}

                Json::Value overlayMedatada;
                
                overlayMedatada["mmsSourceVideoAssetPathName"] = mmsSourceVideoAssetPathName;
                overlayMedatada["videoDurationInMilliSeconds"] =
					(Json::LargestUInt) (_encodingItem->_overlayImageOnVideoData->_videoDurationInMilliSeconds);
                overlayMedatada["mmsSourceImageAssetPathName"] = mmsSourceImageAssetPathName;
                overlayMedatada["imagePosition_X_InPixel"] = imagePosition_X_InPixel;
                overlayMedatada["imagePosition_Y_InPixel"] = imagePosition_Y_InPixel;
                // overlayMedatada["encodedFileName"] = encodedFileName;
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

            _logger->info(__FILEREF__ + "Overlaying media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(overlayContentResponse, field))
                {
                    string error = overlayContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "overlayImageOnVideo. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}
            
		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{
				encodingStatusFailures++;

				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
		_logger->info(__FILEREF__ + "Overlayed media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(stagingEncodedAssetPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processOverlayedImageOnVideo(string stagingEncodedAssetPathName,
		bool killedByUser)
{
    try
    {
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_overlayImageOnVideoData->_overlayParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

        localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
    
    /*
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
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
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }        

    try
    {
        bool inCaseOfLinkHasItToBeRead = false;
        unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                inCaseOfLinkHasItToBeRead);   

        _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata..."
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

        mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveSourceContentMetadata (
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
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }    

    
    return mediaItemKeyAndPhysicalPathKey;
    */
}

pair<string, bool> EncoderVideoAudioProxy::overlayTextOnVideo()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
    stagingEncodedAssetPathNameAndKilledByUser = overlayTextOnVideo_through_ffmpeg();
	if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->error(errorMessage);
       
		throw EncodingKilledByUser();
	}
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::overlayTextOnVideo_through_ffmpeg()
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
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegOverlayTextOnVideoURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;

		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsSourceVideoAssetPathName;

    
				// stagingEncodedAssetPathName preparation
				{
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;

					/*
					mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
						_encodingItem->_overlayTextOnVideoData->_mmsVideoPartitionNumber,
						_encodingItem->_workspace->_directoryName,
						_encodingItem->_overlayTextOnVideoData->_videoRelativePath,
						_encodingItem->_overlayTextOnVideoData->_videoFileName);
					*/

					size_t extensionIndex = _encodingItem->_overlayTextOnVideoData->_videoFileName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _overlayTextOnVideoData->_videoFileName: " + _encodingItem->_overlayTextOnVideoData->_videoFileName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					/*
					encodedFileName =
					to_string(_encodingItem->_ingestionJobKey)
					+ "_"
					+ to_string(_encodingItem->_encodingJobKey)
					+ _encodingItem->_overlayTextOnVideoData->_videoFileName.substr(extensionIndex)
					;
					*/
					string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
						_encodingItem->_workspace);
					stagingEncodedAssetPathName = 
						workspaceIngestionRepository + "/" 
						+ to_string(_encodingItem->_ingestionJobKey)
						+ "_overlayedtext"
						+ _encodingItem->_overlayTextOnVideoData->_videoFileName.substr(extensionIndex)
						;
					/*
					bool removeLinuxPathIfExist = true;
					stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					_encodingItem->_workspace->_directoryName,
					"/",    // _encodingItem->_relativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
					removeLinuxPathIfExist);        
					*/
				}

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
                
                // overlayTextMedatada["encodedFileName"] = encodedFileName;
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

            _logger->info(__FILEREF__ + "OverlayText media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(overlayTextContentResponse, field))
                {
                    string error = overlayTextContentResponse.get(field, "XXX").asString();

                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "overlayTextOnVideo. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, stagingEncodedAssetPathName);

		bool killedByUser = false;
		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{                    
				encodingStatusFailures++;
                    
				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
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
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
            
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "OverlayedText media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(stagingEncodedAssetPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processOverlayedTextOnVideo(string stagingEncodedAssetPathName,
		bool killedByUser)
{
    try
    {
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_overlayTextOnVideoData->_overlayTextParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

        localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
    /*
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
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
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "ffmpeg.getMediaInfo failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            );

            throw e;
        }        

        try
        {
            bool inCaseOfLinkHasItToBeRead = false;
            unsigned long sizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                    inCaseOfLinkHasItToBeRead);   

            _logger->info(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata..."
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

            mediaItemKeyAndPhysicalPathKey = _mmsEngineDBFacade->saveSourceContentMetadata (
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
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
                + ", e.what(): " + e.what()
            );

            throw e;
        }
        catch(exception e)
        {
            _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveSourceContentMetadata failed"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _encodingParameters: " + _encodingItem->_encodingParameters
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
                + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            );

            throw e;
        }    


        return mediaItemKeyAndPhysicalPathKey;
     */
}

pair<string, bool> EncoderVideoAudioProxy::videoSpeed()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
    stagingEncodedAssetPathNameAndKilledByUser = videoSpeed_through_ffmpeg();
	if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->error(errorMessage);
       
		throw EncodingKilledByUser();
	}
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::videoSpeed_through_ffmpeg()
{
    
    int64_t sourceVideoPhysicalPathKey;
    string videoSpeedType;
    int videoSpeedSize;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayTextOnVideoJob
    {
        string field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "videoSpeedType";
        videoSpeedType = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "videoSpeedSize";
        videoSpeedSize = _encodingItem->_parametersRoot.get(field, 0).asInt();
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegVideoSpeedURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;

		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsSourceVideoAssetPathName;

    
				// stagingEncodedAssetPathName preparation
				{
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;

					/*
					mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
						_encodingItem->_videoSpeedData->_mmsVideoPartitionNumber,
						_encodingItem->_workspace->_directoryName,
						_encodingItem->_videoSpeedData->_videoRelativePath,
						_encodingItem->_videoSpeedData->_videoFileName);
					*/

					size_t extensionIndex = _encodingItem->_videoSpeedData->_videoFileName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _videoSpeedData->_videoFileName: " + _encodingItem->_videoSpeedData->_videoFileName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					/*
					encodedFileName =
					to_string(_encodingItem->_ingestionJobKey)
					+ "_"
					+ to_string(_encodingItem->_encodingJobKey)
					+ _encodingItem->_overlayTextOnVideoData->_videoFileName.substr(extensionIndex)
					;
					*/
					string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
						_encodingItem->_workspace);
					stagingEncodedAssetPathName = workspaceIngestionRepository + "/" 
							+ to_string(_encodingItem->_ingestionJobKey)
							+ "_videoSpeed"
							+ _encodingItem->_videoSpeedData->_videoFileName.substr(extensionIndex)
					;
					/*
					bool removeLinuxPathIfExist = true;
					stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					_encodingItem->_workspace->_directoryName,
					"/",    // _encodingItem->_relativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
					removeLinuxPathIfExist);        
					*/
				}

                Json::Value videoSpeedMetadata;
                
                videoSpeedMetadata["mmsSourceVideoAssetPathName"] = mmsSourceVideoAssetPathName;
                videoSpeedMetadata["videoDurationInMilliSeconds"] =
					(Json::LargestUInt) (_encodingItem->_videoSpeedData->_videoDurationInMilliSeconds);

                videoSpeedMetadata["videoSpeedType"] = videoSpeedType;
                videoSpeedMetadata["videoSpeedSize"] = videoSpeedSize;

                videoSpeedMetadata["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                videoSpeedMetadata["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                videoSpeedMetadata["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, videoSpeedMetadata);
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

            _logger->info(__FILEREF__ + "VideoSpeed media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value videoSpeedContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &videoSpeedContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(videoSpeedContentResponse, field))
                {
                    string error = videoSpeedContentResponse.get(field, "XXX").asString();

                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "videoSpeed. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, stagingEncodedAssetPathName);

		bool killedByUser = false;

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{                    
				encodingStatusFailures++;
                    
				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
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
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
            
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
		_logger->info(__FILEREF__ + "VideoSpeed media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(stagingEncodedAssetPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processVideoSpeed(string stagingEncodedAssetPathName,
		bool killedByUser)
{
    try
    {
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_videoSpeedData->_videoSpeedParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

        localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processVideoSpeed failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processVideoSpeed failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

pair<string, bool> EncoderVideoAudioProxy::pictureInPicture()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
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
        stagingEncodedAssetPathNameAndKilledByUser = pictureInPicture_through_ffmpeg();
		if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
                ;
			_logger->error(errorMessage);
        
			throw EncodingKilledByUser();
		}
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::pictureInPicture_through_ffmpeg()
{

    int64_t mainVideoPhysicalPathKey;
    int64_t overlayVideoPhysicalPathKey;
    string overlayPosition_X_InPixel;
    string overlayPosition_Y_InPixel;
    string overlay_Width_InPixel;
    string overlay_Height_InPixel;
	bool soundOfMain;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "mainVideoPhysicalPathKey";
        mainVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "overlayVideoPhysicalPathKey";
        overlayVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "overlayPosition_X_InPixel";
        overlayPosition_X_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "overlayPosition_Y_InPixel";
        overlayPosition_Y_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "overlay_Width_InPixel";
        overlay_Width_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "overlay_Height_InPixel";
        overlay_Height_InPixel = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "soundOfMain";
        soundOfMain = _encodingItem->_parametersRoot.get(field, true).asBool();
    }

	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegPictureInPictureURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;
		bool killedByUser = false;

		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsMainVideoAssetPathName;
				string mmsOverlayVideoAssetPathName;


				// stagingEncodedAssetPathName preparation
				{
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_main =
						_mmsStorage->getPhysicalPath(mainVideoPhysicalPathKey);
					tie(mmsMainVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_main;

					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_overlay =
						_mmsStorage->getPhysicalPath(overlayVideoPhysicalPathKey);
					tie(mmsOverlayVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_overlay;

					/*
					mmsMainVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
						_encodingItem->_pictureInPictureData->_mmsMainVideoPartitionNumber,
						_encodingItem->_workspace->_directoryName,
						_encodingItem->_pictureInPictureData->_mainVideoRelativePath,
						_encodingItem->_pictureInPictureData->_mainVideoFileName);

					mmsOverlayVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
						_encodingItem->_pictureInPictureData->_mmsOverlayVideoPartitionNumber,
						_encodingItem->_workspace->_directoryName,
						_encodingItem->_pictureInPictureData->_overlayVideoRelativePath,
						_encodingItem->_pictureInPictureData->_overlayVideoFileName);
					*/

					size_t extensionIndex = _encodingItem->_pictureInPictureData->_mainVideoFileName.find_last_of(".");
					if (extensionIndex == string::npos)
					{
						string errorMessage = __FILEREF__ + "No extension find in the asset file name"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _pictureInPictureData->_mainVideoFileName: " + _encodingItem->_pictureInPictureData->_mainVideoFileName;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					/*
					encodedFileName =
					to_string(_encodingItem->_ingestionJobKey)
					+ "_"
					+ to_string(_encodingItem->_encodingJobKey)
					+ _encodingItem->_overlayImageOnVideoData->_videoFileName.substr(extensionIndex)
					;
					*/

					string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
						_encodingItem->_workspace);
					stagingEncodedAssetPathName = 
						workspaceIngestionRepository + "/" 
						+ to_string(_encodingItem->_ingestionJobKey)
						+ "_pictureInPicture"
						+ _encodingItem->_pictureInPictureData->_mainVideoFileName.substr(extensionIndex)
					;
					/*
					bool removeLinuxPathIfExist = true;
					stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					_encodingItem->_workspace->_directoryName,
					"/",    // _encodingItem->_relativePath,
					encodedFileName,
					-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
					-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
					removeLinuxPathIfExist);        
					*/
				}

                Json::Value pictureInPictureMedatada;
                
                pictureInPictureMedatada["mmsMainVideoAssetPathName"] = mmsMainVideoAssetPathName;
                pictureInPictureMedatada["mainVideoDurationInMilliSeconds"] =
					(Json::LargestUInt) (_encodingItem->_pictureInPictureData->_mainVideoDurationInMilliSeconds);

                pictureInPictureMedatada["mmsOverlayVideoAssetPathName"] = mmsOverlayVideoAssetPathName;
                pictureInPictureMedatada["overlayVideoDurationInMilliSeconds"] =
					(Json::LargestUInt) (_encodingItem->_pictureInPictureData->_overlayVideoDurationInMilliSeconds);

                pictureInPictureMedatada["soundOfMain"] = soundOfMain;

                pictureInPictureMedatada["overlayPosition_X_InPixel"] = overlayPosition_X_InPixel;
                pictureInPictureMedatada["overlayPosition_Y_InPixel"] = overlayPosition_Y_InPixel;
                pictureInPictureMedatada["overlay_Width_InPixel"] = overlay_Width_InPixel;
                pictureInPictureMedatada["overlay_Height_InPixel"] = overlay_Height_InPixel;

                pictureInPictureMedatada["stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
                pictureInPictureMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                pictureInPictureMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, pictureInPictureMedatada);
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

            _logger->info(__FILEREF__ + "PictureInPicture"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value pictureInPictureContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &pictureInPictureContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(pictureInPictureContentResponse, field))
                {
                    string error = pictureInPictureContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "pictureInPicture. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}
            
		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{
				encodingStatusFailures++;

				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}

		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
		_logger->info(__FILEREF__ + "PictureInPicture"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(stagingEncodedAssetPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processPictureInPicture(string stagingEncodedAssetPathName,
		bool killedByUser)
{
    try
    {
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);

        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_pictureInPictureData->_parametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

        localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processPictureInPicture failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processPictureInPicture failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
}


bool EncoderVideoAudioProxy::generateFrames()
{    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
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
        bool killedByUser = generateFrames_through_ffmpeg();
		if (killedByUser)
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
                ;
			_logger->error(errorMessage);
        
			throw EncodingKilledByUser();
		}

		return killedByUser;
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */    
}

bool EncoderVideoAudioProxy::generateFrames_through_ffmpeg()
{
    
	int64_t sourceVideoPhysicalPathKey;
    string imageDirectory;
    double startTimeInSeconds;
    int maxFramesNumber;  
    string videoFilter;
    int periodInSeconds;
    bool mjpeg;
    int imageWidth;
    int imageHeight;
    int64_t ingestionJobKey;
    int64_t videoDurationInMilliSeconds;

	bool killedByUser = false;

    // _encodingItem->_parametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "imageDirectory";
        imageDirectory = _encodingItem->_parametersRoot.get(field, "XXX").asString();

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
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegGenerateFramesURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		if (_encodingItem->_transcoder == "") // || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				string mmsSourceVideoAssetPathName;

				// stagingEncodedAssetPathName preparation
				{
					tuple<string, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;

					/*
					mmsSourceVideoAssetPathName = _mmsStorage->getMMSAssetPathName(
						_encodingItem->_generateFramesData->_mmsVideoPartitionNumber,
						_encodingItem->_workspace->_directoryName,
						_encodingItem->_generateFramesData->_videoRelativePath,
						_encodingItem->_generateFramesData->_videoFileName);
					*/
				}

                Json::Value generateFramesMedatada;
                
                generateFramesMedatada["imageDirectory"] = imageDirectory;
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

            _logger->info(__FILEREF__ + "Generating Frames"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(generateFramesContentResponse, field))
                {
                    string error = generateFramesContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "generateFrames. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			// stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, "");

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{
				encodingStatusFailures++;

				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
            
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
		_logger->info(__FILEREF__ + "Generated Frames"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return killedByUser;
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processGeneratedFrames(bool killedByUser)
{    
    // here we do not have just a profile to be added into MMS but we have
    // one or more MediaItemKeys that have to be ingested
    // One MIK in case of a .mjpeg
    // One or more MIKs in case of .jpg
    
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

pair<string, bool> EncoderVideoAudioProxy::slideShow()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
    /*
    _logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
        + ", _encodingProfileTechnology" + to_string(static_cast<int>(_encodingItem->_encodingProfileTechnology))
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
        stagingEncodedAssetPathNameAndKilledByUser = slideShow_through_ffmpeg();
		if (stagingEncodedAssetPathNameAndKilledByUser.second)	// KilledByUser
		{
			string errorMessage = __FILEREF__ + "Encoding killed by the User"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
                ;
			_logger->error(errorMessage);
        
			throw EncodingKilledByUser();
		}
    /*
    }
    else if (_encodingItem->_encodingProfileTechnology == MMSEngineDBFacade::EncodingTechnology::WindowsMedia)
    {
        string errorMessage = __FILEREF__ + "No Encoder available to encode WindowsMedia technology"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    else
    {
        string errorMessage = __FILEREF__ + "Unknown technology and no Encoder available to encode"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                ;
        _logger->error(errorMessage);
        
        throw runtime_error(errorMessage);
    }
    */
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::slideShow_through_ffmpeg()
{
    
    double durationOfEachSlideInSeconds;
    int outputFrameRate;  
    Json::Value sourcePhysicalPathsRoot(Json::arrayValue);

    {
        string field = "durationOfEachSlideInSeconds";
        durationOfEachSlideInSeconds = _encodingItem->_parametersRoot.get(field, 0).asDouble();

        field = "outputFrameRate";
        outputFrameRate = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "sourcePhysicalPaths";
        sourcePhysicalPathsRoot = _encodingItem->_parametersRoot[field];
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegSlideShowURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string slideShowMediaPathName;

		if (_encodingItem->_transcoder == "" || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
					encoderToSkip);
            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
            );
            ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
    
				{        
					string fileFormat = "mp4";

					/*
					encodedFileName =
					to_string(_encodingItem->_ingestionJobKey)
					+ "_"
					+ to_string(_encodingItem->_encodingJobKey)
					+ "." + fileFormat
					;
					*/

					string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
						_encodingItem->_workspace);
					slideShowMediaPathName = 
						workspaceIngestionRepository + "/" 
						+ to_string(_encodingItem->_ingestionJobKey)
						+ "." + fileFormat
					;
				}

                Json::Value slideShowMedatada;
                
                slideShowMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
                slideShowMedatada["durationOfEachSlideInSeconds"] = durationOfEachSlideInSeconds;
                slideShowMedatada["outputFrameRate"] = outputFrameRate;
                slideShowMedatada["slideShowMediaPathName"] = slideShowMediaPathName;
                slideShowMedatada["sourcePhysicalPaths"] = sourcePhysicalPathsRoot;

                {
                    Json::StreamWriterBuilder wbuilder;
                    
                    body = Json::writeString(wbuilder, slideShowMedatada);
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

            _logger->info(__FILEREF__ + "SlideShow media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
            );
			responseInitialized = true;
            request.perform();

            string sResponse = response.str();
            // LF and CR create problems to the json parser...
            while (sResponse.back() == 10 || sResponse.back() == 13)
                sResponse.pop_back();

            Json::Value slideShowContentResponse;
            try
            {                
                Json::CharReaderBuilder builder;
                Json::CharReader* reader = builder.newCharReader();
                string errors;

                bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &slideShowContentResponse, &errors);
                delete reader;

                if (!parsingSuccessful)
                {
                    string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(__FILEREF__ + errorMessage);

                throw runtime_error(errorMessage);
            }

            {
                string field = "error";
                if (Validator::isMetadataPresent(slideShowContentResponse, field))
                {
                    string error = slideShowContentResponse.get(field, "XXX").asString();
                    
                    if (error.find(NoEncodingAvailable().what()) != string::npos)
                    {
                        string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
                        _logger->warn(__FILEREF__ + errorMessage);

                        throw MaxConcurrentJobsReached();
                    }
                    else
                    {
                        string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
                            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                            + "_currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                                );                                        
                    }
                    else
                    {
                        string errorMessage = string("Unexpected FFMPEGEncoder response")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

                        throw runtime_error(errorMessage);
                    }
                }
                */                        
            }
		}
		else
		{
			_logger->info(__FILEREF__ + "slideShow. The transcoder is already saved, the encoding should be already running"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _encodingItem->_transcoder
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;
			slideShowMediaPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_transcoder because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_transcoder	= "";

			ffmpegEncoderURL = 
                   _ffmpegEncoderProtocol
                   + "://"
                   + _currentUsedFFMpegEncoderHost + ":"
                   + to_string(_ffmpegEncoderPort)
                   + ffmpegURI
                   + "/" + to_string(_encodingItem->_encodingJobKey)
			;
		}

		chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Running;
		}

		_logger->info(__FILEREF__ + "Update EncodingJob"
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", transcoder: " + _currentUsedFFMpegEncoderHost
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderHost, slideShowMediaPathName);

		bool killedByUser = false;

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		int maxEncodingStatusFailures = 1;
		int encodingStatusFailures = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
			catch(...)
			{
				encodingStatusFailures++;

				_logger->error(__FILEREF__ + "getEncodingStatus failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
				);

				if(encodingStatusFailures >= maxEncodingStatusFailures)
				{
					string errorMessage = string("getEncodingStatus too many failures")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
            
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
		_logger->info(__FILEREF__ + "SlideShow media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return make_pair(slideShowMediaPathName, killedByUser);
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processSlideShow(string stagingEncodedAssetPathName,
		bool killedByUser)
{
    try
    {
        int outputFrameRate;  
        string field = "outputFrameRate";
        outputFrameRate = _encodingItem->_parametersRoot.get(field, 0).asInt();
    
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);


        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_slideShowData->_slideShowParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

        localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
        localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processOverlayedImageOnVideo failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

string EncoderVideoAudioProxy::faceRecognition()
{
    
	{
		lock_guard<mutex> locker(*_mtEncodingJobs);

		*_status = EncodingJobStatus::Running;
	}

	if (_faceRecognitionNumber.use_count() > _maxFaceRecognitionNumber)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _faceRecognitionNumber.use_count: " + to_string(_faceRecognitionNumber.use_count())
            + ", _maxFaceRecognitionNumber: " + to_string(_maxFaceRecognitionNumber)
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw MaxConcurrentJobsReached();
	}

	int64_t sourceMediaItemKey;
	string faceRecognitionCascadeName;
	string sourcePhysicalPath;
	string faceRecognitionOutput;
	long initialFramesNumberToBeSkipped;
	bool oneFramePerSecond;
	{
		string field = "sourceMediaItemKey";
		sourceMediaItemKey = _encodingItem->_parametersRoot.get(field, 0).asInt64();

		field = "faceRecognitionCascadeName";
		faceRecognitionCascadeName = _encodingItem->_parametersRoot.get(field, 0).asString();

		field = "sourcePhysicalPath";
		sourcePhysicalPath = _encodingItem->_parametersRoot.get(field, 0).asString();

		// VideoWithHighlightedFaces, ImagesToBeUsedInDeepLearnedModel or FrameContainingFace
		field = "faceRecognitionOutput";
		faceRecognitionOutput = _encodingItem->_parametersRoot.get(field, 0).asString();

		field = "initialFramesNumberToBeSkipped";
		initialFramesNumberToBeSkipped = _encodingItem->_parametersRoot.get(field, 0).asInt();

		field = "oneFramePerSecond";
		oneFramePerSecond = _encodingItem->_parametersRoot.get(field, 0).asBool();
	}
    
	string cascadePathName = _computerVisionCascadePath + "/" + faceRecognitionCascadeName + ".xml";

	_logger->info(__FILEREF__ + "faceRecognition"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceRecognitionCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
	);

	cv::CascadeClassifier cascade;
	if (!cascade.load(cascadePathName))
	{
		string errorMessage = __FILEREF__ + "CascadeName could not be loaded"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", cascadePathName: " + cascadePathName;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	cv::VideoCapture capture;
	// sometimes the file was created by another MMSEngine and it is not found
	// just because of nfs delay. For this reason we implemented a retry mechanism
	int attemptIndex = 0;
	bool captureFinished = false;
	chrono::system_clock::time_point startCapture = chrono::system_clock::now();
	while (!captureFinished)
	{
		capture.open(sourcePhysicalPath, cv::CAP_FFMPEG);
		if (!capture.isOpened())
		{
			if (FileIO::fileExisting(sourcePhysicalPath))
			{
				string errorMessage = __FILEREF__ + "Capture could not be opened"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", sourcePhysicalPath: " + sourcePhysicalPath;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				if (attemptIndex < _waitingNFSSync_attemptNumber)
				{
					attemptIndex++;

					string errorMessage = __FILEREF__ + "The file does not exist, waiting because of nfs delay"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", sourcePhysicalPath: " + sourcePhysicalPath;
						+ ", attemptIndex: " + to_string(attemptIndex)
						+ ", sleepTime: " + to_string(_waitingNFSSync_sleepTimeInSeconds)
							;
					_logger->warn(errorMessage);

					this_thread::sleep_for(chrono::seconds(_waitingNFSSync_sleepTimeInSeconds));
				}
				else
				{
					string errorMessage = __FILEREF__
						+ "Capture could not be opened because the file does not exist"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", sourcePhysicalPath: " + sourcePhysicalPath
						+ ", attemptIndex: " + to_string(attemptIndex)
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else
		{
			captureFinished = true;
		}
	}

	chrono::system_clock::time_point endCapture = chrono::system_clock::now();
	_logger->info(__FILEREF__ + "capture.open"
		+ ", sourcePhysicalPath: " + sourcePhysicalPath
		+ ", statistics duration (secs): "
			+ to_string(chrono::duration_cast<chrono::seconds>(endCapture - startCapture).count())
	);

	string faceRecognitionMediaPathName;
	string fileFormat;
	{
		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
			_encodingItem->_workspace);
		if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel"
				|| faceRecognitionOutput == "FrameContainingFace")
		{
			fileFormat = "jpg";

			faceRecognitionMediaPathName = 
				workspaceIngestionRepository + "/"
				; // sourceFileName is added later
		}
		else // if (faceRecognitionOutput == "VideoWithHighlightedFaces")
		{
			// opencv does not have issues with avi and mov (it seems has issues with mp4)
			fileFormat = "avi";

			faceRecognitionMediaPathName = 
				workspaceIngestionRepository + "/" 
				+ to_string(_encodingItem->_ingestionJobKey)
				+ "." + fileFormat
			;
		}
	}

	_logger->info(__FILEREF__ + "faceRecognition started"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceRecognitionCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
	);

	cv::VideoWriter writer;
	long totalFramesNumber;
	double fps;
	{
		totalFramesNumber = (long) capture.get(cv::CAP_PROP_FRAME_COUNT);
		fps = capture.get(cv::CAP_PROP_FPS);
		cv::Size size(
			(int) capture.get(cv::CAP_PROP_FRAME_WIDTH),
			(int) capture.get(cv::CAP_PROP_FRAME_HEIGHT)
		);

		if (faceRecognitionOutput == "VideoWithHighlightedFaces")
		{
			writer.open(faceRecognitionMediaPathName,
				cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, size);
		}
	}

	_logger->info(__FILEREF__ + "generating Face Recognition start"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceRecognitionCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
            + ", totalFramesNumber: " + to_string(totalFramesNumber)
            + ", fps: " + to_string(fps)
	);

	cv::Mat bgrFrame;
	cv::Mat grayFrame;
	cv::Mat smallFrame;

	// this is used only in case of faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel"
	// Essentially the last image source file name will be ingested when we will go out of the
	// loop (while(true)) in order to set the IngestionRowToBeUpdatedAsSuccess flag a true for this last
	// ingestion
	string lastImageSourceFileName;

	long currentFrameIndex = 0;
	long framesContainingFaces = 0;

	bool bgrFrameEmpty = false;
	if (faceRecognitionOutput == "FrameContainingFace")
	{
		long initialFrameIndex = 0;
		while (initialFrameIndex++ < initialFramesNumberToBeSkipped)
		{
			capture >> bgrFrame;
			currentFrameIndex++;
			if (bgrFrame.empty())
			{
				bgrFrameEmpty = true;

				break;
			}
		}
	}

	bool frameContainingFaceFound = false;
	while(!bgrFrameEmpty)
	{
		if (faceRecognitionOutput == "FrameContainingFace"
			&& oneFramePerSecond)
		{
			int frameIndex = fps - 1;
			while(--frameIndex >= 0)
			{
				capture >> bgrFrame;
				currentFrameIndex++;
				if (bgrFrame.empty())
				{
					bgrFrameEmpty = true;

					break;
				}
			}

			if (bgrFrameEmpty)
				continue;
		}

		capture >> bgrFrame;
		if (bgrFrame.empty())
		{
			bgrFrameEmpty = true;

			continue;
		}

		{
			/*
			double progress = (currentFrameIndex / totalFramesNumber) * 100;
			// this is to have one decimal in the percentage
			double faceRecognitionPercentage = ((double) ((int) (progress * 10))) / 10;
			*/
			_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;
		}

		if (currentFrameIndex % 100 == 0)
		{
			_logger->info(__FILEREF__ + "generating Face Recognition progress"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", cascadeName: " + faceRecognitionCascadeName
				+ ", sourcePhysicalPath: " + sourcePhysicalPath
				+ ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
				+ ", currentFrameIndex: " + to_string(currentFrameIndex)
				+ ", totalFramesNumber: " + to_string(totalFramesNumber)
				+ ", _localEncodingProgress: " + to_string(_localEncodingProgress)
			);
		}

		currentFrameIndex++;

		cv::cvtColor(bgrFrame, grayFrame, cv::COLOR_BGR2GRAY);
		double xAndYScaleFactor = 1 / _computerVisionDefaultScale;
		cv::resize(grayFrame, smallFrame, cv::Size(), xAndYScaleFactor, xAndYScaleFactor,
				cv::INTER_LINEAR_EXACT);
		cv::equalizeHist(smallFrame, smallFrame);

		vector<cv::Rect> faces;
		cascade.detectMultiScale(
			smallFrame,
			faces,
			_computerVisionDefaultScale,
			_computerVisionDefaultMinNeighbors,
			0 | cv::CASCADE_SCALE_IMAGE,
			cv::Size(30,30)
		);

		if (_computerVisionDefaultTryFlip)
		{
			// 1: flip (mirror) horizontally
			cv::flip(smallFrame, smallFrame, 1);
			vector<cv::Rect> faces2;
			cascade.detectMultiScale(
				smallFrame,
				faces2,
				_computerVisionDefaultScale,
				_computerVisionDefaultMinNeighbors,
				0 | cv::CASCADE_SCALE_IMAGE,
				cv::Size(30,30)
			);
			for (vector<cv::Rect>::const_iterator r = faces2.begin(); r != faces2.end(); ++r)
				faces.push_back(cv::Rect(
					smallFrame.cols - r->x - r->width,
					r->y,
					r->width,
					r->height
				));
		}

		if (faceRecognitionOutput == "VideoWithHighlightedFaces"
			|| faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel")
		{
			if (faces.size() > 0)
				framesContainingFaces++;

			for (size_t i = 0; i < faces.size(); i++)
			{
				cv::Rect roiRectScaled = faces[i];
				// cv::Mat smallROI;

				if (faceRecognitionOutput == "VideoWithHighlightedFaces")
				{
					cv::Scalar color = cv::Scalar(255,0,0);
					double aspectRatio = (double) roiRectScaled.width / roiRectScaled.height;
					int thickness = 3;
					int lineType = 8;
					int shift = 0;
					if (0.75 < aspectRatio && aspectRatio < 1.3)
					{
						cv::Point center;
						int radius;

						center.x = cvRound((roiRectScaled.x + roiRectScaled.width*0.5)*_computerVisionDefaultScale);
						center.y = cvRound((roiRectScaled.y + roiRectScaled.height*0.5)*_computerVisionDefaultScale);
						radius = cvRound((roiRectScaled.width + roiRectScaled.height)*0.25*_computerVisionDefaultScale);
						cv::circle(bgrFrame, center, radius, color, thickness, lineType, shift);
					}
					else
					{
						cv::rectangle(bgrFrame,
							cv::Point(cvRound(roiRectScaled.x*_computerVisionDefaultScale),
								cvRound(roiRectScaled.y*_computerVisionDefaultScale)),
							cv::Point(cvRound((roiRectScaled.x + roiRectScaled.width-1)*_computerVisionDefaultScale),
								cvRound((roiRectScaled.y + roiRectScaled.height-1)*_computerVisionDefaultScale)),
							color, thickness, lineType, shift);
					}
				}
				else
				{
					// Crop the full image to that image contained by the rectangle myROI
					// Note that this doesn't copy the data
					cv::Rect roiRect(
						roiRectScaled.x * _computerVisionDefaultScale,
						roiRectScaled.y * _computerVisionDefaultScale,
						roiRectScaled.width * _computerVisionDefaultScale,
						roiRectScaled.height * _computerVisionDefaultScale
					);
					cv::Mat grayFrameCropped(grayFrame, roiRect);

					/*
					cv::Mat cropped;
					// Copy the data into new matrix
					grayFrameCropped.copyTo(cropped);
					*/

					string sourceFileName = to_string(_encodingItem->_ingestionJobKey)
						+ "_"
						+ to_string(currentFrameIndex)
						+ "." + fileFormat
					;

					string faceRecognitionImagePathName = faceRecognitionMediaPathName + sourceFileName;

					cv::imwrite(faceRecognitionImagePathName, grayFrameCropped);
					// cv::imwrite(faceRecognitionImagePathName, cropped);

					if (lastImageSourceFileName == "")
						lastImageSourceFileName = sourceFileName;
					else
					{
						// ingest the face
						int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
						string mediaMetaDataContent = generateMediaMetadataToIngest(
							_encodingItem->_ingestionJobKey,
							fileFormat, faceOfVideoMediaItemKey,
							_encodingItem->_faceRecognitionData->_faceRecognitionParametersRoot);
    
						shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
							->getFreeEvent<LocalAssetIngestionEvent>(
								MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

						localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
						localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
						localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

						localAssetIngestionEvent->setExternalReadOnlyStorage(false);
						localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
						localAssetIngestionEvent->setIngestionSourceFileName(lastImageSourceFileName);
						localAssetIngestionEvent->setMMSSourceFileName(lastImageSourceFileName);
						localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
						localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
						localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(false);
						// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

						localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

						shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
						_multiEventsSet->addEvent(event);

						_logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", sourceFileName: " + lastImageSourceFileName
							+ ", getEventKey().first: " + to_string(event->getEventKey().first)
							+ ", getEventKey().second: " + to_string(event->getEventKey().second));

						lastImageSourceFileName = sourceFileName;
					}
				}
			}

			if (faceRecognitionOutput == "VideoWithHighlightedFaces")
			{
				writer << bgrFrame;
			}
		}
		else // if (faceRecognitionOutput == "FrameContainingFace")
		{
			if (faces.size() > 0)
			{
				framesContainingFaces++;

				// ingest the frame
				string sourceFileName = to_string(_encodingItem->_ingestionJobKey)
					+ "_frameContainingFace"
					+ "_" + to_string(currentFrameIndex)
					+ "." + fileFormat
				;

				string faceRecognitionImagePathName = faceRecognitionMediaPathName + sourceFileName;

				cv::imwrite(faceRecognitionImagePathName, bgrFrame);

				int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
				string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
					fileFormat, faceOfVideoMediaItemKey,
					_encodingItem->_faceRecognitionData->_faceRecognitionParametersRoot);
  
				shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
					->getFreeEvent<LocalAssetIngestionEvent>(
						MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

				localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
				localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
				localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

				localAssetIngestionEvent->setExternalReadOnlyStorage(false);
				localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
				localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
				localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
				localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
				localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
				localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
				// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

				localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

				shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
				_multiEventsSet->addEvent(event);

				frameContainingFaceFound = true;

				_logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT) - FrameContainingFace"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", sourceFileName: " + sourceFileName
					+ ", getEventKey().first: " + to_string(event->getEventKey().first)
					+ ", getEventKey().second: " + to_string(event->getEventKey().second));

				break;
			}
		}
	}

	if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel")
	{
		if (lastImageSourceFileName != "")
		{
			// ingest the face
			int64_t faceOfVideoMediaItemKey = sourceMediaItemKey;
			string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
				fileFormat, faceOfVideoMediaItemKey,
				_encodingItem->_faceRecognitionData->_faceRecognitionParametersRoot);
  
			shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
				->getFreeEvent<LocalAssetIngestionEvent>(
						MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

			localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
			localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
			localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

			localAssetIngestionEvent->setExternalReadOnlyStorage(false);
			localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
			localAssetIngestionEvent->setIngestionSourceFileName(lastImageSourceFileName);
			localAssetIngestionEvent->setMMSSourceFileName(lastImageSourceFileName);
			localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
			localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
			localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
			// localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

			localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

			shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
			_multiEventsSet->addEvent(event);

			_logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", sourceFileName: " + lastImageSourceFileName
				+ ", getEventKey().first: " + to_string(event->getEventKey().first)
				+ ", getEventKey().second: " + to_string(event->getEventKey().second));
		}
		else
		{
			// no faces were met, let's update ingestion status
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;                        

			string errorMessage = "No faces recognized";
			string processorMMS;
			_logger->info(__FILEREF__ + "Update IngestionJob"                                             
				+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)                                      
				+ ", IngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
				+ ", errorMessage: " + errorMessage
				+ ", processorMMS: " + processorMMS
			);                                                                                            
			_mmsEngineDBFacade->updateIngestionJob (_encodingItem->_ingestionJobKey,
					newIngestionStatus, errorMessage);
		}
	}
	else if (faceRecognitionOutput == "FrameContainingFace")
	{
		// in case the frame containing a face was not found
		if (!frameContainingFaceFound)
		{
			MMSEngineDBFacade::IngestionStatus newIngestionStatus = MMSEngineDBFacade::IngestionStatus::End_IngestionFailure;                        

			string errorMessage = "No face recognized";
			string processorMMS;
			_logger->info(__FILEREF__ + "Update IngestionJob"                                             
				+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)                                      
				+ ", IngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
				+ ", errorMessage: " + errorMessage
				+ ", processorMMS: " + processorMMS
			);                                                                                            
			_mmsEngineDBFacade->updateIngestionJob (_encodingItem->_ingestionJobKey,
					newIngestionStatus, errorMessage);
		}
		else
		{
			_logger->info(__FILEREF__ + "faceRecognition media done"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", cascadeName: " + faceRecognitionCascadeName
				+ ", sourcePhysicalPath: " + sourcePhysicalPath
				+ ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
				+ ", currentFrameIndex: " + to_string(currentFrameIndex)
				+ ", framesContainingFaces: " + to_string(framesContainingFaces)
				+ ", frameContainingFaceFound: " + to_string(frameContainingFaceFound)
			);
		}
	}

	capture.release();

	_logger->info(__FILEREF__ + "faceRecognition media done"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceRecognitionCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceRecognitionMediaPathName: " + faceRecognitionMediaPathName
            + ", currentFrameIndex: " + to_string(currentFrameIndex)
            + ", framesContainingFaces: " + to_string(framesContainingFaces)
	);


	return faceRecognitionMediaPathName;
}


void EncoderVideoAudioProxy::processFaceRecognition(string stagingEncodedAssetPathName)
{
    try
    {
		string faceRecognitionOutput;
		{
			// VideoWithHighlightedFaces or ImagesToBeUsedInDeepLearnedModel
			string field = "faceRecognitionOutput";
			faceRecognitionOutput = _encodingItem->_parametersRoot.get(field, 0).asString();
		}

		if (faceRecognitionOutput == "FacesImagesToBeUsedInDeepLearnedModel"
			|| faceRecognitionOutput == "FrameContainingFace")
		{
			// nothing to do, all the faces (images) were already ingested

			return;
		}

		// faceRecognitionOutput is "VideoWithHighlightedFaces"

        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);


        
		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_faceRecognitionData->_faceRecognitionParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent = _multiEventsSet->getEventsFactory()
                ->getFreeEvent<LocalAssetIngestionEvent>(
						MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
        // localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processFaceRecognition failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processFaceRecognition failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}


string EncoderVideoAudioProxy::faceIdentification()
{
	{
		lock_guard<mutex> locker(*_mtEncodingJobs);

		*_status = EncodingJobStatus::Running;
	}

	// build the deep learned model
	vector<cv::Mat> images;
	vector<int> idImages;
	unordered_map<int, string> idTagMap;
	{
		vector<string> deepLearnedModelTags;

		string field = "deepLearnedModelTags";
		Json::Value deepLearnedModelTagsRoot = _encodingItem->_parametersRoot[field];
		for (int deepLearnedModelTagsIndex = 0;
				deepLearnedModelTagsIndex < deepLearnedModelTagsRoot.size();
				deepLearnedModelTagsIndex++)
		{
			deepLearnedModelTags.push_back(
					deepLearnedModelTagsRoot[deepLearnedModelTagsIndex].asString());
		}

		int64_t mediaItemKey = -1;
		vector<int64_t> otherMediaItemsKey;
		string uniqueName;
		int64_t physicalPathKey = -1;
		bool contentTypePresent = true;
		MMSEngineDBFacade::ContentType contentType = MMSEngineDBFacade::ContentType::Image;
		bool startAndEndIngestionDatePresent = false;
		string startIngestionDate;
		string endIngestionDate;
		string title;
		int liveRecordingChunk = -1;
		string jsonCondition;
		string ingestionDateOrder;
		string jsonOrderBy;
		bool admin = true;

		int start = 0;
		int rows = 200;
		int totalImagesNumber = -1;
		bool imagesFinished = false;

		int idImageCounter = 0;
		unordered_map<string, int> tagIdMap;
		vector<string> tagsNotIn;

		while(!imagesFinished)
		{
			Json::Value mediaItemsListRoot = _mmsEngineDBFacade->getMediaItemsList(
				_encodingItem->_workspace->_workspaceKey, mediaItemKey, uniqueName, physicalPathKey,
				otherMediaItemsKey, start, rows, contentTypePresent, contentType,
				startAndEndIngestionDatePresent, startIngestionDate, endIngestionDate,
				title, liveRecordingChunk, jsonCondition, deepLearnedModelTags, tagsNotIn,
				ingestionDateOrder, jsonOrderBy, admin);

			field = "response";
			Json::Value responseRoot = mediaItemsListRoot[field];

			if (totalImagesNumber == -1)
			{
				field = "numFound";
				totalImagesNumber = responseRoot.get(field, 0).asInt();
			}
			
			field = "mediaItems";
			Json::Value mediaItemsArrayRoot = responseRoot[field];
			if (mediaItemsArrayRoot.size() < rows)
				imagesFinished = true;
			else
				start += rows;

			_logger->info(__FILEREF__ + "Called getMediaItemsList"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", mediaItemsArrayRoot.size(): " + to_string(mediaItemsArrayRoot.size())
			);

			for (int imageIndex = 0; imageIndex < mediaItemsArrayRoot.size(); imageIndex++)
			{
				Json::Value mediaItemRoot = mediaItemsArrayRoot[imageIndex];

				int currentIdImage;
				unordered_map<string, int>::iterator tagIdIterator;

				field = "tags";
				string tags = mediaItemRoot.get(field, 0).asString();
				if (tags.front() == ',')
					tags = tags.substr(1);
				if (tags.back() == ',')
					tags.pop_back();

				tagIdIterator = tagIdMap.find(tags);
				if (tagIdIterator == tagIdMap.end())
				{
					currentIdImage = idImageCounter++;
					tagIdMap.insert(make_pair(tags, currentIdImage));
				}
				else
					currentIdImage = (*tagIdIterator).second;

				{
					unordered_map<int, string>::iterator idTagIterator;

				   	idTagIterator = idTagMap.find(currentIdImage);
					if (idTagIterator == idTagMap.end())
						idTagMap.insert(make_pair(currentIdImage, tags));
				}

				field = "physicalPaths";
				Json::Value physicalPathsArrayRoot = mediaItemRoot[field];
				if (physicalPathsArrayRoot.size() > 0)
				{
					Json::Value physicalPathRoot = physicalPathsArrayRoot[0];

					field = "physicalPathKey";
					int64_t physicalPathKey = physicalPathRoot.get(field, 0).asInt64();

					tuple<string, string, string, int64_t, string>
						physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(physicalPathKey);
					string mmsImagePathName;
					tie(mmsImagePathName, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;

					images.push_back(cv::imread(mmsImagePathName, 0));
					idImages.push_back(currentIdImage);
				}
			}
		}
	}

	_logger->info(__FILEREF__ + "Deep learned model built"
		+ ", images.size: " + to_string(images.size())
		+ ", idImages.size: " + to_string(idImages.size())
		+ ", idTagMap.size: " + to_string(idTagMap.size())
	);

	if (images.size() == 0)
	{
		string errorMessage = __FILEREF__
			+ "The Deep Learned Model is empty, no deepLearnedModelTags found"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	string faceIdentificationCascadeName;
	string sourcePhysicalPath;
	{
		string field = "faceIdentificationCascadeName";
		faceIdentificationCascadeName = _encodingItem->_parametersRoot.get(field, 0).asString();

		field = "sourcePhysicalPath";
		sourcePhysicalPath = _encodingItem->_parametersRoot.get(field, 0).asString();
	}
    
	string cascadePathName = _computerVisionCascadePath + "/"
		+ faceIdentificationCascadeName + ".xml";

	cv::CascadeClassifier cascade;
	if (!cascade.load(cascadePathName))
	{
		string errorMessage = __FILEREF__ + "CascadeName could not be loaded"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", cascadePathName: " + cascadePathName;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	// The following lines create an LBPH model for
	// face recognition and train it with the images and
	// labels.
	//
	// The LBPHFaceRecognizer uses Extended Local Binary Patterns
	// (it's probably configurable with other operators at a later
	// point), and has the following default values
	//
	//      radius = 1
	//      neighbors = 8
	//      grid_x = 8
	//      grid_y = 8
	//
	// So if you want a LBPH FaceRecognizer using a radius of
	// 2 and 16 neighbors, call the factory method with:
	//
	//      cv::face::LBPHFaceRecognizer::create(2, 16);
	//
	// And if you want a threshold (e.g. 123.0) call it with its default values:
	//
	//      cv::face::LBPHFaceRecognizer::create(1,8,8,8,123.0)
	//
	cv::Ptr<cv::face::LBPHFaceRecognizer> recognizerModel = cv::face::LBPHFaceRecognizer::create();
	recognizerModel->train(images, idImages);

	cv::VideoCapture capture;
	// sometimes the file was created by another MMSEngine and it is not found
	// just because of nfs delay. For this reason we implemented a retry mechanism
	int attemptIndex = 0;
	bool captureFinished = false;
	chrono::system_clock::time_point startCapture = chrono::system_clock::now();
	while (!captureFinished)
	{
		capture.open(sourcePhysicalPath, cv::CAP_FFMPEG);
		if (!capture.isOpened())
		{
			if (FileIO::fileExisting(sourcePhysicalPath))
			{
				string errorMessage = __FILEREF__ + "Capture could not be opened"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", sourcePhysicalPath: " + sourcePhysicalPath;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			else
			{
				if (attemptIndex < _waitingNFSSync_attemptNumber)
				{
					attemptIndex++;

					string errorMessage = __FILEREF__ + "The file does not exist, waiting because of nfs delay"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", sourcePhysicalPath: " + sourcePhysicalPath;
						+ ", attemptIndex: " + to_string(attemptIndex)
						+ ", sleepTime: " + to_string(_waitingNFSSync_sleepTimeInSeconds)
							;
					_logger->warn(errorMessage);

					this_thread::sleep_for(chrono::seconds(_waitingNFSSync_sleepTimeInSeconds));
				}
				else
				{
					string errorMessage = __FILEREF__
						+ "Capture could not be opened because the file does not exist"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", sourcePhysicalPath: " + sourcePhysicalPath
						+ ", attemptIndex: " + to_string(attemptIndex)
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
			}
		}
		else
		{
			captureFinished = true;
		}
	}

	chrono::system_clock::time_point endCapture = chrono::system_clock::now();
	_logger->info(__FILEREF__ + "capture.open"
		+ ", sourcePhysicalPath: " + sourcePhysicalPath
		+ ", statistics duration (secs): "
			+ to_string(chrono::duration_cast<chrono::seconds>(endCapture - startCapture).count())
	);

	string faceIdentificationMediaPathName;
	string fileFormat;
	{
		string workspaceIngestionRepository = _mmsStorage->getWorkspaceIngestionRepository(
			_encodingItem->_workspace);

		{
			// opencv does not have issues with avi and mov (it seems has issues with mp4)
			fileFormat = "avi";

			faceIdentificationMediaPathName = 
				workspaceIngestionRepository + "/" 
				+ to_string(_encodingItem->_ingestionJobKey)
				+ "." + fileFormat
			;
		}
	}

	_logger->info(__FILEREF__ + "faceIdentification started"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceIdentificationCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);

	cv::VideoWriter writer;
	long totalFramesNumber;
	{
		totalFramesNumber = (long) capture.get(cv::CAP_PROP_FRAME_COUNT);
		double fps = capture.get(cv::CAP_PROP_FPS);
		cv::Size size(
			(int) capture.get(cv::CAP_PROP_FRAME_WIDTH),
			(int) capture.get(cv::CAP_PROP_FRAME_HEIGHT)
		);

		{
			writer.open(faceIdentificationMediaPathName,
				cv::VideoWriter::fourcc('X', '2', '6', '4'), fps, size);
		}
	}

	_logger->info(__FILEREF__ + "generating Face Identification"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceIdentificationCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);

	cv::Mat bgrFrame;
	cv::Mat grayFrame;
	cv::Mat smallFrame;

	long currentFrameIndex = 0;
	while(true)
	{
		capture >> bgrFrame;
		if (bgrFrame.empty())
			break;

		if (currentFrameIndex % 100 == 0)
		{
			_logger->info(__FILEREF__ + "generating Face Recognition"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", cascadeName: " + faceIdentificationCascadeName
				+ ", sourcePhysicalPath: " + sourcePhysicalPath
				+ ", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
				+ ", currentFrameIndex: " + to_string(currentFrameIndex)
				+ ", totalFramesNumber: " + to_string(totalFramesNumber)
			);
		}

		{
			/*
			double progress = (currentFrameIndex / totalFramesNumber) * 100;
			// this is to have one decimal in the percentage
			double faceRecognitionPercentage = ((double) ((int) (progress * 10))) / 10;
			*/
			_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;
		}
		currentFrameIndex++;

		cv::cvtColor(bgrFrame, grayFrame, cv::COLOR_BGR2GRAY);
		double xAndYScaleFactor = 1 / _computerVisionDefaultScale;
		cv::resize(grayFrame, smallFrame, cv::Size(), xAndYScaleFactor, xAndYScaleFactor,
				cv::INTER_LINEAR_EXACT);
		cv::equalizeHist(smallFrame, smallFrame);

		vector<cv::Rect> faces;
		cascade.detectMultiScale(
			smallFrame,
			faces,
			_computerVisionDefaultScale,
			_computerVisionDefaultMinNeighbors,
			0 | cv::CASCADE_SCALE_IMAGE,
			cv::Size(30,30)
		);

		if (_computerVisionDefaultTryFlip)
		{
			// 1: flip (mirror) horizontally
			cv::flip(smallFrame, smallFrame, 1);
			vector<cv::Rect> faces2;
			cascade.detectMultiScale(
				smallFrame,
				faces2,
				_computerVisionDefaultScale,
				_computerVisionDefaultMinNeighbors,
				0 | cv::CASCADE_SCALE_IMAGE,
				cv::Size(30,30)
			);
			for (vector<cv::Rect>::const_iterator r = faces2.begin(); r != faces2.end(); ++r)
				faces.push_back(cv::Rect(
					smallFrame.cols - r->x - r->width,
					r->y,
					r->width,
					r->height
				));
		}

		for (size_t i = 0; i < faces.size(); i++)
		{
			cv::Rect roiRectScaled = faces[i];

			// Crop the full image to that image contained by the rectangle myROI
			// Note that this doesn't copy the data
			cv::Rect roiRect(
					roiRectScaled.x * _computerVisionDefaultScale,
					roiRectScaled.y * _computerVisionDefaultScale,
					roiRectScaled.width * _computerVisionDefaultScale,
					roiRectScaled.height * _computerVisionDefaultScale
			);
			cv::Mat grayFrameCropped(grayFrame, roiRect);

			string predictedTags;
			{
				// int predictedLabel = recognizerModel->predict(grayFrameCropped);
				// To get the confidence of a prediction call the model with:
				int predictedIdImage = -1;
				double confidence = 0.0;
				recognizerModel->predict(grayFrameCropped, predictedIdImage, confidence);

				{
					unordered_map<int, string>::iterator idTagIterator;

					idTagIterator = idTagMap.find(predictedIdImage);
					if (idTagIterator != idTagMap.end())
						predictedTags = (*idTagIterator).second;
				}

				_logger->info(__FILEREF__ + "recognizerModel->predict"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", predictedIdImage: " + to_string(predictedIdImage)
					+ ", confidence: " + to_string(confidence)
					+ ", predictedTags: " + predictedTags
				);
			}

			{
				cv::Scalar color = cv::Scalar(255,0,0);
				double aspectRatio = (double) roiRectScaled.width / roiRectScaled.height;
				int thickness = 3;
				int lineType = 8;
				int shift = 0;
				if (0.75 < aspectRatio && aspectRatio < 1.3)
				{
					cv::Point center;
					int radius;

					center.x = cvRound((roiRectScaled.x + roiRectScaled.width*0.5)
							*_computerVisionDefaultScale);
					center.y = cvRound((roiRectScaled.y + roiRectScaled.height*0.5)
							*_computerVisionDefaultScale);
					radius = cvRound((roiRectScaled.width + roiRectScaled.height)*0.25
							*_computerVisionDefaultScale);
					cv::circle(bgrFrame, center, radius, color, thickness, lineType, shift);
				}
				else
				{
					cv::rectangle(bgrFrame,
						cv::Point(cvRound(roiRectScaled.x*_computerVisionDefaultScale),
							cvRound(roiRectScaled.y*_computerVisionDefaultScale)),
						cv::Point(cvRound((roiRectScaled.x + roiRectScaled.width-1)
								*_computerVisionDefaultScale),
							cvRound((roiRectScaled.y + roiRectScaled.height-1)
								*_computerVisionDefaultScale)),
						color, thickness, lineType, shift);
				}

				double fontScale = 2;
				cv::putText(
						bgrFrame,
					   	predictedTags,
						cv::Point(cvRound(roiRectScaled.x*_computerVisionDefaultScale),
							cvRound(roiRectScaled.y*_computerVisionDefaultScale)),
					   	cv::FONT_HERSHEY_PLAIN,
					   	fontScale,
						color,
						thickness);
			}
		}

		writer << bgrFrame;
	}

	capture.release();

	_logger->info(__FILEREF__ + "faceIdentification media done"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
			+ ", cascadeName: " + faceIdentificationCascadeName
			+ ", sourcePhysicalPath: " + sourcePhysicalPath
            + ", faceIdentificationMediaPathName: " + faceIdentificationMediaPathName
	);


	return faceIdentificationMediaPathName;
}

void EncoderVideoAudioProxy::processFaceIdentification(string stagingEncodedAssetPathName)
{
    try
    {
        size_t extensionIndex = stagingEncodedAssetPathName.find_last_of(".");
        if (extensionIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No extention find in the asset file name"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string fileFormat = stagingEncodedAssetPathName.substr(extensionIndex + 1);

        size_t fileNameIndex = stagingEncodedAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }
        string sourceFileName = stagingEncodedAssetPathName.substr(fileNameIndex + 1);


		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			_encodingItem->_faceIdentificationData->_faceIdentificationParametersRoot);
    
        shared_ptr<LocalAssetIngestionEvent>    localAssetIngestionEvent =
			_multiEventsSet->getEventsFactory() ->getFreeEvent<LocalAssetIngestionEvent>(
						MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

        localAssetIngestionEvent->setSource(ENCODERVIDEOAUDIOPROXY);
        localAssetIngestionEvent->setDestination(MMSENGINEPROCESSORNAME);
        localAssetIngestionEvent->setExpirationTimePoint(chrono::system_clock::now());

		localAssetIngestionEvent->setExternalReadOnlyStorage(false);
        localAssetIngestionEvent->setIngestionJobKey(_encodingItem->_ingestionJobKey);
        localAssetIngestionEvent->setIngestionSourceFileName(sourceFileName);
        localAssetIngestionEvent->setMMSSourceFileName(sourceFileName);
        localAssetIngestionEvent->setWorkspace(_encodingItem->_workspace);
        localAssetIngestionEvent->setIngestionType(MMSEngineDBFacade::IngestionType::AddContent);
        localAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(true);
        // localAssetIngestionEvent->setForcedAvgFrameRate(to_string(outputFrameRate) + "/1");

        localAssetIngestionEvent->setMetadataContent(mediaMetaDataContent);

        shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(localAssetIngestionEvent);
        _multiEventsSet->addEvent(event);

        _logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (INGESTASSETEVENT)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", sourceFileName: " + sourceFileName
            + ", getEventKey().first: " + to_string(event->getEventKey().first)
            + ", getEventKey().second: " + to_string(event->getEventKey().second));
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processFaceIdentification failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processFaceIdentification failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

tuple<bool, bool> EncoderVideoAudioProxy::liveRecorder()
{

	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	int segmentDurationInSeconds;
	bool autoRenew;
	{
        string field = "autoRenew";
        autoRenew = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "utcRecordingPeriodStart";
        utcRecordingPeriodStart = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "segmentDurationInSeconds";
        segmentDurationInSeconds = _encodingItem->_parametersRoot.get(field, 0).asInt();

		// since the first chunk is discarded, we will start recording before the period of the chunk
		utcRecordingPeriodStart -= segmentDurationInSeconds;

        field = "utcRecordingPeriodEnd";
        utcRecordingPeriodEnd = _encodingItem->_parametersRoot.get(field, 0).asInt64();
	}

	time_t utcNow;
	{
		chrono::system_clock::time_point now = chrono::system_clock::now();
		utcNow = chrono::system_clock::to_time_t(now);
	}

	// MMS allocates a thread just 5 minutes before the beginning of the recording
	if (utcNow < utcRecordingPeriodStart)
	{
	   	if (utcRecordingPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)
		{
			_logger->info(__FILEREF__ + "Too early to allocate a thread for recording"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", utcRecordingPeriodStart - utcNow: " + to_string(utcRecordingPeriodStart - utcNow)
				+ ", _timeBeforeToPrepareResourcesInSeconds: " + to_string(_timeBeforeToPrepareResourcesInMinutes * 60)
			);

			// it is simulated a MaxConcurrentJobsReached to avoid to increase the error counter
			throw MaxConcurrentJobsReached();
		}
	}

	if (!autoRenew)
	{
		if (utcRecordingPeriodEnd <= utcNow)
		{
			string errorMessage = __FILEREF__ + "Too late to activate the recording"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
				+ ", utcNow: " + to_string(utcNow)
				;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	tuple<bool, bool> killedByUserAndMain = liveRecorder_through_ffmpeg();
	if (get<0>(killedByUserAndMain))	// KilledByUser
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->warn(errorMessage);
        
		throw EncodingKilledByUser();
	}
    
	return killedByUserAndMain;
}

tuple<bool, bool> EncoderVideoAudioProxy::liveRecorder_through_ffmpeg()
{

	bool highAvailability;
	bool main;
	string liveURL;
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	int segmentDurationInSeconds;
	string outputFileFormat;
	{
        string field = "highAvailability";
        highAvailability = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "main";
        main = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "liveURL";
        liveURL = _encodingItem->_parametersRoot.get(field, "XXX").asString();

        field = "utcRecordingPeriodStart";
        utcRecordingPeriodStart = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "utcRecordingPeriodEnd";
        utcRecordingPeriodEnd = _encodingItem->_parametersRoot.get(field, 0).asInt64();

        field = "autoRenew";
        autoRenew = _encodingItem->_parametersRoot.get(field, 0).asBool();

        field = "segmentDurationInSeconds";
        segmentDurationInSeconds = _encodingItem->_parametersRoot.get(field, 0).asInt();

        field = "outputFileFormat";
        outputFileFormat = _encodingItem->_parametersRoot.get(field, "XXX").asString();
	}

	bool killedByUser = false;

	time_t utcNow = 0;
	while (!killedByUser && utcNow < utcRecordingPeriodEnd)
	{
		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveRecorderURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			if (_encodingItem->_transcoder == "")
			{
				// In case of HighAvailability, main and backup should run on different server
				// case Main:
				//		1. look for the transcoder of the backup
				//		2. if it is no defined
				//			a "random" transcoder is used
				//		   else
				//			a transcoder different by the backupTranscoder (if exist) is used
				// case Backup:
				//		1. look for the mainTransocder
				//		2. if present
				//			a transcoder different by the mainTranscoder (if exist) is used
				//		   else
				//		    sleep and come back to 1. The loop 1. and 2. is executed up to 60 secs
				if (highAvailability)
				{
					if (main)
					{
						string backupTranscoder = _mmsEngineDBFacade->getLiveRecorderOtherTranscoder(
							main, _encodingItem->_encodingJobKey);

						if (backupTranscoder == "")
						{
							_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (main). BackupTranscoder is not selected yet. Just get a transcoder"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", highAvailability: " + to_string(highAvailability)
									+ ", main: " + to_string(main)
								);

							string transcoderToSKip;
							_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
								transcoderToSKip);
						}
						else
						{
							_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (main). BackupTranscoder is already selected. Just get another transcoder"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", highAvailability: " + to_string(highAvailability)
									+ ", main: " + to_string(main)
									+ ", backupTranscoder: " + backupTranscoder
								);

							string transcoderToSKip = backupTranscoder;
							_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
								transcoderToSKip);
						}
					}
					else
					{
						// backup live recorder

						int intervalInSecondsToWaitLiveRecorderMainTranscoder = 5;
						int maxWaitInSecondsForLiveRecorderMainTranscoder = 60;

						chrono::system_clock::time_point startPoint = chrono::system_clock::now();
						chrono::system_clock::time_point intermediatePoint;
						bool transcoderFound = false;

						while (!transcoderFound)
						{
							string mainTranscoder = _mmsEngineDBFacade->getLiveRecorderOtherTranscoder(
								main, _encodingItem->_encodingJobKey);

							if (mainTranscoder == "")
							{
								if (chrono::duration_cast<chrono::seconds>(
									(chrono::system_clock::now() + chrono::seconds (intervalInSecondsToWaitLiveRecorderMainTranscoder)) - startPoint).count() >
										maxWaitInSecondsForLiveRecorderMainTranscoder)
								{
									_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (backup). Main Transcoder is not selected and time expired, Just get a transcoder"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", highAvailability: " + to_string(highAvailability)
										+ ", main: " + to_string(main)
									);

									string transcoderToSKip;
									_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
										_encodingItem->_workspace, transcoderToSKip);

									transcoderFound = true;
								}
								else
								{
									_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (backup). Main Transcoder is not selected. Just wait a bit"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", highAvailability: " + to_string(highAvailability)
										+ ", main: " + to_string(main)
									);

									this_thread::sleep_for(chrono::seconds(
										intervalInSecondsToWaitLiveRecorderMainTranscoder));

									intermediatePoint = chrono::system_clock::now();
								}
							}
							else
							{
								_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (backup). Main Transcoder is already selected. Just get another transcoder"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", highAvailability: " + to_string(highAvailability)
									+ ", main: " + to_string(main)
									+ ", mainTranscoder: " + mainTranscoder
								);

								string transcoderToSKip = mainTranscoder;
								_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
									_encodingItem->_workspace, transcoderToSKip);

								transcoderFound = true;
							}
						}
					}
				}
				else
				{
					// no high availability

					_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder. No high availability, just get a transcoder"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", highAvailability: " + to_string(highAvailability)
						);

					string encoderToSKip;
					_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(_encodingItem->_workspace,
						encoderToSKip);
				}

				_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
				);

				ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
				;

				string body;
				{
					string transcoderStagingContentsPath;
					string stagingContentsPath;
					string segmentListFileName;
					string recordedFileNamePrefix;
					{
						{
							bool removeLinuxPathIfExist = false;
							bool neededForTranscoder = true;
							string stagingLiveRecordingAssetPathName = _mmsStorage->getStagingAssetPathName(
								neededForTranscoder,
								_encodingItem->_workspace->_directoryName,
								to_string(_encodingItem->_encodingJobKey),	// directoryNamePrefix,
								"/",    // _encodingItem->_relativePath,
								to_string(_encodingItem->_ingestionJobKey),
								-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
								-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
								removeLinuxPathIfExist);
							size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of("/");
							if (directoryEndIndex == string::npos)
							{
								string errorMessage = __FILEREF__ + "No directory found in the staging asset path name"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
								_logger->error(errorMessage);

								// throw runtime_error(errorMessage);
							}
							transcoderStagingContentsPath = stagingLiveRecordingAssetPathName.substr(0, directoryEndIndex + 1);
						}

						{
							bool removeLinuxPathIfExist = false;
							bool neededForTranscoder = false;
							string stagingLiveRecordingAssetPathName = _mmsStorage->getStagingAssetPathName(
								neededForTranscoder,
								_encodingItem->_workspace->_directoryName,
								to_string(_encodingItem->_encodingJobKey),	// directoryNamePrefix,
								"/",    // _encodingItem->_relativePath,
								to_string(_encodingItem->_ingestionJobKey),
								-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
								-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
								removeLinuxPathIfExist);
							size_t directoryEndIndex = stagingLiveRecordingAssetPathName.find_last_of("/");
							if (directoryEndIndex == string::npos)
							{
								string errorMessage = __FILEREF__ + "No directory found in the staging asset path name"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", stagingLiveRecordingAssetPathName: " + stagingLiveRecordingAssetPathName;
								_logger->error(errorMessage);

								// throw runtime_error(errorMessage);
							}
							stagingContentsPath = stagingLiveRecordingAssetPathName.substr(0, directoryEndIndex + 1);
						}

						segmentListFileName = to_string(_encodingItem->_ingestionJobKey)
							+ "_" + to_string(_encodingItem->_encodingJobKey)
							+ ".liveRecorder.list"
						;

						recordedFileNamePrefix = string("liveRecorder_")
							+ to_string(_encodingItem->_ingestionJobKey)
							+ "_" + to_string(_encodingItem->_encodingJobKey)
							;
					}

					Json::Value liveRecorderMedatada;
                
					liveRecorderMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
					liveRecorderMedatada["transcoderStagingContentsPath"] = transcoderStagingContentsPath;
					liveRecorderMedatada["stagingContentsPath"] = stagingContentsPath;
					liveRecorderMedatada["segmentListFileName"] = segmentListFileName;
					liveRecorderMedatada["recordedFileNamePrefix"] = recordedFileNamePrefix;
					liveRecorderMedatada["encodingParametersRoot"] = _encodingItem->_parametersRoot;
					liveRecorderMedatada["liveRecorderParametersRoot"] = _encodingItem->_liveRecorderData->_liveRecorderParametersRoot;

					{
						Json::StreamWriterBuilder wbuilder;
                    
						body = Json::writeString(wbuilder, liveRecorderMedatada);
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

				_logger->info(__FILEREF__ + "LiveRecorder media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
				);
				responseInitialized = true;
				request.perform();

				string sResponse = response.str();
				// LF and CR create problems to the json parser...
				while (sResponse.back() == 10 || sResponse.back() == 13)
					sResponse.pop_back();

				Json::Value liveRecorderContentResponse;
				try
				{
					Json::CharReaderBuilder builder;
					Json::CharReader* reader = builder.newCharReader();
					string errors;

					bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &liveRecorderContentResponse, &errors);
					delete reader;

					if (!parsingSuccessful)
					{
						string errorMessage = __FILEREF__ + "failed to parse the response body"
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", sResponse: " + sResponse
                        ;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
				}

				{
					string field = "error";
					if (Validator::isMetadataPresent(liveRecorderContentResponse, field))
					{
						string error = liveRecorderContentResponse.get(field, "XXX").asString();
                    
						if (error.find(NoEncodingAvailable().what()) != string::npos)
						{
							string errorMessage = string("No Encodings available")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
							_logger->warn(__FILEREF__ + errorMessage);

							throw MaxConcurrentJobsReached();
						}
						else
						{
							string errorMessage = string("FFMPEGEncoder error")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                                + ", sResponse: " + sResponse
                                ;
							_logger->error(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}                        
					}
				}
			}
			else
			{
				_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder. The transcoder is already saved (DB), the encoding should be already running"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", transcoder: " + _encodingItem->_transcoder
				);

				_currentUsedFFMpegEncoderHost = _encodingItem->_transcoder;

				// we have to reset _encodingItem->_transcoder because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_transcoder	= "";

				ffmpegEncoderURL = 
                    _ffmpegEncoderProtocol
                    + "://"
                    + _currentUsedFFMpegEncoderHost + ":"
                    + to_string(_ffmpegEncoderPort)
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
				;
			}

			chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

			{
				lock_guard<mutex> locker(*_mtEncodingJobs);

				*_status = EncodingJobStatus::Running;
			}

			_logger->info(__FILEREF__ + "Update EncodingJob"
				+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _currentUsedFFMpegEncoderHost
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
				_currentUsedFFMpegEncoderHost, "");

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
            int maxEncodingStatusFailures = 10;
            int encodingStatusFailures = 0;
			// string lastRecordedAssetFileName;

			// see the comment few lines below (2019-05-03)
            // while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            while(!encodingFinished)
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

                try
                {
                    tuple<bool, bool, bool> encodingStatus = getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError) = encodingStatus;

					if (completedWithError)
					{
						string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey);
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}

					/*
					lastRecordedAssetFileName = processLastGeneratedLiveRecorderFiles(
						highAvailability, main, segmentDurationInSeconds,
						transcoderStagingContentsPath + segmentListFileName, recordedFileNamePrefix,
						liveRecordingContentsPath, lastRecordedAssetFileName);
					*/
                }
                catch(...)
                {
                    encodingStatusFailures++;

                    _logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                        + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                        + ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
                    );

					/*
					 2019-05-03: commented because we saw the following scenario:
						1. getEncodingStatus fails because of HTTP call failure (502 Bad Gateway)
							Really the live recording is still working into the encoder process
						2. EncoderVideoAudioProxy reached maxEncodingStatusFailures
						3. since the recording is not finished yet, this code/method activate a new live recording session
						Result: we have 2 live recording process into the encoder creating problems
						To avoid that we will exit from this loop ONLY when we are sure the recoridng is finished
					 2019-07-02: in case the encoder was shutdown or crashed, the Engine has to activate another
						Encoder, so we increased maxEncodingStatusFailures to be sure the encoder is not working anymore
						and, in this case we do a break in order to activate another encoder.
					*/
            		if(encodingStatusFailures >= maxEncodingStatusFailures)
					{
                        string errorMessage = string("getEncodingStatus too many failures")
                                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    			+ ", encodingFinished: " + to_string(encodingFinished)
                    			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    			+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
                                ;
                        _logger->error(__FILEREF__ + errorMessage);

						break;
                        // throw runtime_error(errorMessage);
					}
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
            
			utcNow = chrono::system_clock::to_time_t(endEncoding);

			if (utcNow < utcRecordingPeriodEnd)
			{
				_logger->error(__FILEREF__ + "LiveRecorder media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", still remaining seconds (utcRecordingPeriodEnd - utcNow): " + to_string(utcRecordingPeriodEnd - utcNow)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    + ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
                    + ", killedByUser: " + to_string(killedByUser)
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
			}
			else
			{
				_logger->info(__FILEREF__ + "LiveRecorder media file completed"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", autoRenew: " + to_string(autoRenew) 
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", killedByUser: " + to_string(killedByUser) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);

				if (autoRenew)
				{
					_logger->info(__FILEREF__ + "Renew Live Recording"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						);

					time_t recordingPeriodInSeconds = utcRecordingPeriodEnd - utcRecordingPeriodStart;

					utcRecordingPeriodStart		= utcRecordingPeriodEnd;
					utcRecordingPeriodEnd		+= recordingPeriodInSeconds;

					_logger->info(__FILEREF__ + "Update Encoding LiveRecording Period"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
						+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
					);
					_mmsEngineDBFacade->updateEncodingLiveRecordingPeriod(
							_encodingItem->_encodingJobKey,
							utcRecordingPeriodStart, utcRecordingPeriodEnd);

					// next update is important because the JSON is used in the getEncodingProgress method 
					{
						string field = "utcRecordingPeriodStart";
						_encodingItem->_parametersRoot[field] = utcRecordingPeriodStart;

						field = "utcRecordingPeriodEnd";
						_encodingItem->_parametersRoot[field] = utcRecordingPeriodEnd;
					}
				}
			}
		}
		catch(MaxConcurrentJobsReached e)
		{
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
            throw e;
        }
        catch (curlpp::LogicError& e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (LogicError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );
            
			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
        catch (curlpp::RuntimeError& e) 
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (RuntimeError)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
        catch (exception e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed (exception)"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
	}

    return make_tuple(killedByUser, main);
}

void EncoderVideoAudioProxy::processLiveRecorder(bool killedByUser)
{
    try
    {
		bool main;
		bool highAvailability;
		{
			string field = "main";
			main = _encodingItem->_parametersRoot.get(field, 0).asBool();

			field = "highAvailability";
			highAvailability = _encodingItem->_parametersRoot.get(field, 0).asBool();
		}

		/*
		_logger->info(__FILEREF__ + "remove"
			+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
		);
		FileIO::remove(stagingEncodedAssetPathName);
		*/

		if (main)
		{
			// in case of highAvailability, the IngestionJob is not updated to Success until all the
			// main and backup chunks are managed.
			// This is to avoid the 'on success' task receives input MIKs that are not validated (and that
			// will be removed soon)
			if (highAvailability)
			{
				// the setting of this variable is done also in MMSEngineDBFacade::manageMainAndBackupOfRunnungLiveRecordingHA method
				// So in case this is changed, also in MMSEngineDBFacade::manageMainAndBackupOfRunnungLiveRecordingHA has to be changed too
				int toleranceMinutes = 5;
				int sleepTimeInSeconds = 15;	// main and backup management starts: * * * * * * 15,30,45

				_logger->info(__FILEREF__ + "Waiting the finishing of main and backup chunks management"
					+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				);

				chrono::system_clock::time_point startPoint = chrono::system_clock::now();
				chrono::system_clock::time_point endPoint;
				bool mainAndBackupChunksManagementCompleted;
				do
				{
					this_thread::sleep_for(chrono::seconds(sleepTimeInSeconds));

					mainAndBackupChunksManagementCompleted = _mmsEngineDBFacade->liveRecorderMainAndBackupChunksManagementCompleted(
						_encodingItem->_ingestionJobKey);
					endPoint = chrono::system_clock::now();
				}
				while(!mainAndBackupChunksManagementCompleted &&
					chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count() < toleranceMinutes * 60);

				if (mainAndBackupChunksManagementCompleted)
					_logger->info(__FILEREF__ + "Managing of main and backup chunks completed"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					);
				else
					_logger->warn(__FILEREF__ + "Managing of main and backup chunks NOT completed"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					);
			}

			{
				string errorMessage;
				string processorMMS;
				MMSEngineDBFacade::IngestionStatus	newIngestionStatus =
					MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;
				_logger->info(__FILEREF__ + "Update IngestionJob"
					+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", IngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
					+ ", errorMessage: " + errorMessage
					+ ", processorMMS: " + processorMMS
				);
				_mmsEngineDBFacade->updateIngestionJob(_encodingItem->_ingestionJobKey, newIngestionStatus,
					errorMessage);
			}
		}
		else
		{
			_logger->info(__FILEREF__ + "IngestionJob does not update because it's backup recording"
				+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			);
		}
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processLiveRecorder failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );

        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processLiveRecorder failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

int EncoderVideoAudioProxy::getEncodingProgress()
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
		if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceRecognition
				|| _encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceIdentification)
		{
			_logger->info(__FILEREF__ + "encodingProgress"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", encodingProgress: " + to_string(_localEncodingProgress)
			);

			encodingProgress = _localEncodingProgress;
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
		{
			time_t utcRecordingPeriodStart;
			time_t utcRecordingPeriodEnd;
			{
				string field = "utcRecordingPeriodStart";
				utcRecordingPeriodStart = _encodingItem->_parametersRoot.get(field, 0).asInt64();

				field = "utcRecordingPeriodEnd";
				utcRecordingPeriodEnd = _encodingItem->_parametersRoot.get(field, 0).asInt64();
			}

			time_t utcNow;
			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNow = chrono::system_clock::to_time_t(now);
			}

			if (utcNow < utcRecordingPeriodStart)
				encodingProgress = 0;
			else if (utcRecordingPeriodStart < utcNow && utcNow < utcRecordingPeriodEnd)
				encodingProgress = ((utcNow - utcRecordingPeriodStart) * 100) /
					(utcRecordingPeriodEnd - utcRecordingPeriodStart);
			else
				encodingProgress = 100;

			_logger->info(__FILEREF__ + "encodingProgress"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", encodingProgress: " + to_string(encodingProgress)
			);
		}
		else
		{
			string ffmpegEncoderURL;
			ostringstream response;
			bool responseInitialized = false;
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
                    + "/" + to_string(_encodingItem->_encodingJobKey)
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
				responseInitialized = true;
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
                    
						if (error.find(FFMpegEncodingStatusNotAvailable().what()) != string::npos)
						{
							string errorMessage = string(FFMpegEncodingStatusNotAvailable().what())
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
							_logger->warn(__FILEREF__ + errorMessage);

							throw FFMpegEncodingStatusNotAvailable();
						}
						else
						{
							string errorMessage = string("FFMPEGEncoder error")
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
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
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
								+ ", encodingProgress: " + to_string(encodingProgress)
                                );                                        
						}
						else
						{
							string errorMessage = string("Unexpected FFMPEGEncoder response")
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                                + ", sResponse: " + sResponse
                                ;
							_logger->error(__FILEREF__ + errorMessage);

							throw runtime_error(errorMessage);
						}
					}                        
				}
				catch(FFMpegEncodingStatusNotAvailable e)
				{
					string errorMessage = string(e.what())
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", sResponse: " + sResponse
                        ;
					_logger->warn(__FILEREF__ + errorMessage);

					throw FFMpegEncodingStatusNotAvailable();
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
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);
            
				throw e;
			}
			catch (curlpp::RuntimeError & e) 
			{ 
				string errorMessage = string("Progress URL failed (RuntimeError)")
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				;
            
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
			catch (FFMpegEncodingStatusNotAvailable e)
			{
				_logger->warn(__FILEREF__ + "Progress URL failed (EncodingStatusNotAvailable)"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				throw e;
			}
			catch (runtime_error e)
			{
				_logger->error(__FILEREF__ + "Progress URL failed (runtime_error)"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				throw e;
			}
			catch (exception e)
			{
				_logger->error(__FILEREF__ + "Progress URL failed (exception)"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
					+ ", exception: " + e.what()
					+ ", response.str(): " + (responseInitialized ? response.str() : "")
				);

				throw e;
			}
		}
    #endif

    return encodingProgress;
}

tuple<bool,bool,bool> EncoderVideoAudioProxy::getEncodingStatus()
{
    bool encodingFinished;
    bool killedByUser;
	bool completedWithError;
    
    string ffmpegEncoderURL;
    ostringstream response;
	bool responseInitialized = false;
    try
    {
        ffmpegEncoderURL = 
                _ffmpegEncoderProtocol
                + "://"                
                + _currentUsedFFMpegEncoderHost + ":"
                + to_string(_ffmpegEncoderPort)
                + _ffmpegEncoderStatusURI
                + "/" + to_string(_encodingItem->_encodingJobKey)
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
		responseInitialized = true;
        request.perform();
        chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

        string sResponse = response.str();
        // LF and CR create problems to the json parser...
        while (sResponse.back() == 10 || sResponse.back() == 13)
            sResponse.pop_back();

        _logger->info(__FILEREF__ + "getEncodingStatus"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                + ", sResponse: " + sResponse
                + ", encodingDuration (secs): " + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count())
        );

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
                string errorMessage = __FILEREF__ + "getEncodingStatus. Failed to parse the response body"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                        + ", errors: " + errors
                        + ", sResponse: " + sResponse
                        ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }

			string field = "completedWithError";
			if (_mmsEngineDBFacade->isMetadataPresent(encodeStatusResponse, field))
				completedWithError = encodeStatusResponse.get(field, false).asBool();
			else
				completedWithError = false;

            encodingFinished = encodeStatusResponse.get("encodingFinished", "XXX").asBool();
            killedByUser = encodeStatusResponse.get("killedByUser", "XXX").asBool();
        }
        catch(...)
        {
            string errorMessage = string("getEncodingStatus. Response Body json is not well format")
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", sResponse: " + sResponse
                    ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    catch (curlpp::LogicError & e) 
    {
        _logger->error(__FILEREF__ + "Status URL failed (LogicError)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        );

        throw e;
    }
    catch (curlpp::RuntimeError & e) 
    { 
        string errorMessage = string("Status URL failed (RuntimeError)")
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        ;

        _logger->error(__FILEREF__ + errorMessage);

        throw runtime_error(errorMessage);
    }
    catch (runtime_error e)
    {
        _logger->error(__FILEREF__ + "Status URL failed (exception)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        );

        throw e;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "Status URL failed (exception)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
            + ", exception: " + e.what()
			+ ", response.str(): " + (responseInitialized ? response.str() : "")
        );

        throw e;
    }

    return make_tuple(encodingFinished, killedByUser, completedWithError);
}

string EncoderVideoAudioProxy::generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        string fileFormat,
		int64_t faceOfVideoMediaItemKey,
        Json::Value parametersRoot
)
{
    string field = "FileFormat";
    if (_mmsEngineDBFacade->isMetadataPresent(parametersRoot, field))
    {
        string fileFormatSpecifiedByUser = parametersRoot.get(field, "XXX").asString();
        if (fileFormatSpecifiedByUser != fileFormat)
        {
            string errorMessage = string("Wrong fileFormat")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", fileFormatSpecifiedByUser: " + fileFormatSpecifiedByUser
                + ", fileFormat: " + fileFormat
            ;
            _logger->error(__FILEREF__ + errorMessage);

            throw runtime_error(errorMessage);
        }
    }
    else
    {
        parametersRoot[field] = fileFormat;
    }
    
	if (faceOfVideoMediaItemKey != -1)
	{
		MMSEngineDBFacade::CrossReferenceType	crossReferenceType =
			MMSEngineDBFacade::CrossReferenceType::FaceOfVideo;

		Json::Value crossReferenceRoot;

		field = "Type";
        crossReferenceRoot[field] =
			MMSEngineDBFacade::toString(crossReferenceType);

		field = "MediaItemKey";
        crossReferenceRoot[field] = faceOfVideoMediaItemKey;

		field = "CrossReference";
        parametersRoot[field] = crossReferenceRoot;
	}

    string mediaMetadata;
    {
        Json::StreamWriterBuilder wbuilder;
        mediaMetadata = Json::writeString(wbuilder, parametersRoot);
    }

    _logger->info(__FILEREF__ + "Media metadata generated"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", mediaMetadata: " + mediaMetadata
            );

    return mediaMetadata;
}

