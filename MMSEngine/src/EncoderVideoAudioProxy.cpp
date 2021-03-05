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

#include "JSONUtils.h"
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
#include "catralibraries/StringUtils.h"
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
    
    _intervalInSecondsToCheckEncodingFinished         = JSONUtils::asInt(_configuration["encoding"],
			"intervalInSecondsToCheckEncodingFinished", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
    );        
    _maxSecondsToWaitUpdateEncodingJobLock         = JSONUtils::asInt(_configuration["mms"]["locks"],
			"maxSecondsToWaitUpdateEncodingJobLock", 30);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
    );        

	_liveRecorderVirtualVODImageLabel = _configuration["ffmpeg"].get("liveRecorderVirtualVODImageLabel",
		"").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel
	);

	/*
    _ffmpegEncoderProtocol = _configuration["ffmpeg"].get("encoderProtocol", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProtocol: " + _ffmpegEncoderProtocol
    );
    _ffmpegEncoderPort = JSONUtils::asInt(_configuration["ffmpeg"], "encoderPort", 0);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPort: " + to_string(_ffmpegEncoderPort)
    );
	*/
    _ffmpegEncoderUser = _configuration["ffmpeg"].get("encoderUser", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
    );
    _ffmpegEncoderPassword = _configuration["ffmpeg"].get("encoderPassword", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPassword: " + "..."
    );
    _ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(_configuration["ffmpeg"],
		"encoderTimeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderTimeoutInSeconds: " + to_string(_ffmpegEncoderTimeoutInSeconds)
    );
    _ffmpegEncoderProgressURI = _configuration["ffmpeg"].get("encoderProgressURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProgressURI: " + _ffmpegEncoderProgressURI
    );
    _ffmpegEncoderStatusURI = _configuration["ffmpeg"].get("encoderStatusURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI
    );
    _ffmpegEncoderKillEncodingURI = _configuration["ffmpeg"].get("encoderKillEncodingURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderKillEncodingURI: " + _ffmpegEncoderKillEncodingURI
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
    _ffmpegLiveProxyURI = _configuration["ffmpeg"].get("liveProxyURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveProxyURI: " + _ffmpegLiveProxyURI
    );
    _ffmpegAwaitingTheBeginningURI = _configuration["ffmpeg"].get("awaitingTheBeginningURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->awaitingTheBeginningURI: " + _ffmpegAwaitingTheBeginningURI
    );
    _ffmpegLiveGridURI = _configuration["ffmpeg"].get("liveGridURI", "").asString();
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveGridURI: " + _ffmpegLiveGridURI
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
	if (_computerVisionCascadePath.size() > 0 && _computerVisionCascadePath.back() == '/')
		_computerVisionCascadePath.pop_back();
    _computerVisionDefaultScale				= JSONUtils::asDouble(configuration["computerVision"], "defaultScale", 1.1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultScale: " + to_string(_computerVisionDefaultScale)
    );
    _computerVisionDefaultMinNeighbors		= JSONUtils::asInt(configuration["computerVision"],
			"defaultMinNeighbors", 2);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultMinNeighbors: " + to_string(_computerVisionDefaultMinNeighbors)
    );
    _computerVisionDefaultTryFlip		= JSONUtils::asBool(configuration["computerVision"], "defaultTryFlip", false);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultTryFlip: " + to_string(_computerVisionDefaultTryFlip)
    );

	_timeBeforeToPrepareResourcesInMinutes		= JSONUtils::asInt(configuration["mms"],
			"liveRecording_timeBeforeToPrepareResourcesInMinutes", 2);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", mms->liveRecording_timeBeforeToPrepareResourcesInMinutes: " + to_string(_timeBeforeToPrepareResourcesInMinutes)
	);

    _waitingNFSSync_attemptNumber = JSONUtils::asInt(configuration["storage"],
		"waitingNFSSync_attemptNumber", 1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", storage->waitingNFSSync_attemptNumber: " + to_string(_waitingNFSSync_attemptNumber)
    );
    _waitingNFSSync_sleepTimeInSeconds = JSONUtils::asInt(configuration["storage"],
		"waitingNFSSync_sleepTimeInSeconds", 3);
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

	_retrieveStreamingYouTubeURLPeriodInHours = 5;	// 5 hours

	_maxEncoderNotReachableFailures = 10;	// consecutive errors

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
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeImage)
        {
			stagingEncodedAssetPathName = encodeContentImage();
        }
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
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
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
        {
			killedByUser = liveProxy();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::AwaitingTheBeginning)
        {
			killedByUser = awaitingTheBeginning();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveGrid)
        {
			killedByUser = liveGrid();
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
    catch(YouTubeURLNotRetrieved e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			// 2020-09-17: in case of YouTubeURLNotRetrieved there is no retries
			//	just a failure of the ingestion job
			bool forceEncodingToBeFailed = true;

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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
    catch(EncoderError e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int64_t mediaItemKey = -1;
			int64_t encodedPhysicalPathKey = -1;
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                mediaItemKey, encodedPhysicalPathKey,
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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
                main ? _encodingItem->_ingestionJobKey : -1, e.what());
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
	catch(FFMpegURLForbidden e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy
				|| _encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: channel cielo, the URL return FORBIDDEN and it has to be generated again
				//		because it will have an expired timestamp. For this reason we have to stop this request
				//		in order the crontab script will generate a new URL
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1,
				e.what(),
				forceEncodingToBeFailed);
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
	catch(FFMpegURLNotFound e)
    {
		_logger->error(__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what()
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder
					|| _encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				// 2020-10-25: Added also LiveProxy to be here, in case of LiveProxy and URL not found error
				//	does not have sense to retry, we need the generation of a new URL (restream-auto case)
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1,
				e.what(),
				forceEncodingToBeFailed);
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

        _logger->info(__FILEREF__ + "EncoderVideoAudioProxy finished (url not found)"
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
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeImage)
        {
            mediaItemKey = _encodingItem->_encodeData->_mediaItemKey;

			encodedPhysicalPathKey = processEncodedImage(stagingEncodedAssetPathName);
        }
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
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
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
        {
            processLiveProxy(killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::AwaitingTheBeginning)
        {
            processAwaitingTheBeginning(killedByUser);
            
            mediaItemKey = -1;
            encodedPhysicalPathKey = -1;            
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveGrid)
        {
            processLiveGrid(killedByUser);
            
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

			try
			{
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
			catch(runtime_error er)
			{
				_logger->error(__FILEREF__ + "remove FAILED"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", _encodingParameters: " + _encodingItem->_encodingParameters
					+ ", er.what(): " + er.what()
				);
			}
        }

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries since it already run up
				// to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(__FILEREF__ + "updateEncodingJob PunctualError"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
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
                main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
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

string EncoderVideoAudioProxy::encodeContentImage()
{

	string          stagingEncodedAssetPathName;

    try
    {
		string mmsSourceAssetPathName;
		int64_t encodingProfileKey;    
		string                      newImageFormat;
		int                         newWidth;
		int                         newHeight;
		bool                        newAspectRatio;
		string                      sNewInterlaceType;
		Magick::InterlaceType       newInterlaceType;
		string encodedFileName;
		size_t extensionIndex;

		{
			int64_t sourcePhysicalPathKey;

			{
				string field = "sourcePhysicalPathKey";
				sourcePhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
						field, 0);

				field = "encodingProfileKey";
				encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);
			}

			extensionIndex = _encodingItem->_encodeData->_fileName.find_last_of(".");
			if (extensionIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "No extension find in the asset file name"
					+ ", encodingItem->_encodeData->_fileName: " + _encodingItem->_encodeData->_fileName;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
			encodedFileName =
				_encodingItem->_encodeData->_fileName.substr(0, extensionIndex)
				+ "_" 
				+ to_string(encodingProfileKey);
    
			tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
				_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourcePhysicalPathKey);
			tie(mmsSourceAssetPathName, ignore, ignore, ignore, ignore, ignore)
				= physicalPathFileNameSizeInBytesAndDeliveryFileName;

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
        
			readingImageProfile(_encodingItem->_encodeData->_jsonProfile,
				newImageFormat, newWidth, newHeight, newAspectRatio, sNewInterlaceType, newInterlaceType);
		}

        Magick:: Image      imageToEncode;
        
        imageToEncode.read (mmsSourceAssetPathName.c_str());

        string currentImageFormat = imageToEncode.magick ();
        
        if (currentImageFormat == "jpeg")
            currentImageFormat = "JPG";

        int currentWidth	= imageToEncode. columns ();
        int currentHeight	= imageToEncode. rows ();

        _logger->info(__FILEREF__ + "Image processing"
            + ", encodingProfileKey: " + to_string(encodingProfileKey)
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

            encodedFileName.append(_encodingItem->_encodeData->_fileName.substr(extensionIndex));

            bool removeLinuxPathIfExist = true;
			bool neededForTranscoder = false;
            stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder,
                _encodingItem->_workspace->_directoryName,
                to_string(_encodingItem->_encodingJobKey),
                _encodingItem->_encodeData->_relativePath,
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
			bool neededForTranscoder = false;
            stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
				neededForTranscoder,
                _encodingItem->_workspace->_directoryName,
                to_string(_encodingItem->_encodingJobKey),
                "/",    // encodingItem->_encodeData->_relativePath,
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
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
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

        throw runtime_error(e.what());
    }
    catch (exception e)
    {
        _logger->info(__FILEREF__ + "ImageMagick exception"
            + ", e.what(): " + e.what()
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
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
        
    return stagingEncodedAssetPathName;
}

int64_t EncoderVideoAudioProxy::processEncodedImage(
	string stagingEncodedAssetPathName)
{
    int64_t sourcePhysicalPathKey;
    int64_t encodingProfileKey;    

    {
        string field = "sourcePhysicalPathKey";
        sourcePhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);

        field = "encodingProfileKey";
        encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);
    }

	int64_t physicalItemRetentionInMinutes = -1;
	{
		string field = "PhysicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_encodeData->_ingestedParametersRoot, field))
		{
			string retention = _encodingItem->_encodeData->_ingestedParametersRoot.get(field, "1d").asString();
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
	}

	pair<int64_t, long> mediaInfoDetails;
	vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;
	/*
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
	*/

    int imageWidth = -1;
    int imageHeight = -1;
    string imageFormat;
    int imageQuality = -1;
    try
    {
        _logger->info(__FILEREF__ + "Processing through Magick"
            + ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );
        Magick::Image      imageToEncode;

        imageToEncode.read (stagingEncodedAssetPathName.c_str());

        imageWidth	= imageToEncode.columns();
        imageHeight	= imageToEncode.rows();
        imageFormat = imageToEncode.magick();
        imageQuality = imageToEncode.quality();
    }
    catch( Magick::WarningCoder &e )
    {
        // Process coder warning while loading file (e.g. TIFF warning)
        // Maybe the user will be interested in these warnings (or not).
        // If a warning is produced while loading an image, the image
        // can normally still be used (but not if the warning was about
        // something important!)
        _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

        throw runtime_error(e.what());
    }
    catch( Magick::Warning &e )
    {
        _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

        throw runtime_error(e.what());
    }
    catch( Magick::ErrorFileOpen &e ) 
    { 
        _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

        throw runtime_error(e.what());
    }
    catch (Magick::Error &e)
    { 
        _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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

        throw runtime_error(e.what());
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "ImageMagick failed to retrieve width and height"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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
        _logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
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
				+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
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
            bool inCaseOfLinkHasItToBeRead = false;
            mmsAssetSizeInBytes = FileIO::getFileSizeInBytes(mmsAssetPathName,
                    inCaseOfLinkHasItToBeRead);   
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
			physicalItemRetentionInMinutes,
                
			mediaInfoDetails,
			videoTracks,
			audioTracks,
			/*
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
			*/

            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
                );
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
        );
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed"
            + ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodingItem->_encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
        );

		bool exceptionInCaseOfErr = false;
        _logger->info(__FILEREF__ + "Remove"
            + ", mmsAssetPathName: " + mmsAssetPathName
        );
        FileIO::remove(mmsAssetPathName, exceptionInCaseOfErr);

        throw e;
    }
    
    return encodedPhysicalPathKey;
}

pair<string, bool> EncoderVideoAudioProxy::encodeContentVideoAudio()
{
	pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;

	_logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
		+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
		+ ", _encodeData->_deliveryTechnology: "
			+ MMSEngineDBFacade::toString(_encodingItem->_encodeData->_deliveryTechnology)
		+ ", _mp4Encoder: " + _mp4Encoder
	);

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

    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg()
{

	string encodersPool;
    int64_t sourcePhysicalPathKey;
    int64_t encodingProfileKey;    

    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_encodeData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "sourcePhysicalPathKey";
        sourcePhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);

        field = "encodingProfileKey";
        encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);
    }
    
	string stagingEncodedAssetPathName;
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegEncodeURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "getEncoderHost"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
					+ ", _deliveryTechnology: " + MMSEngineDBFacade::toString(_encodingItem->_encodeData->_deliveryTechnology)
					+ ", _directoryName: " + _encodingItem->_workspace->_directoryName
					+ ", _durationInMilliSeconds: " + to_string(_encodingItem->_encodeData->_durationInMilliSeconds)
					+ ", _contentType: " + to_string(static_cast<int>(_encodingItem->_encodeData->_contentType))
					+ ", _relativePath: " + _encodingItem->_encodeData->_relativePath
					+ ", encodingProfileKey: " + to_string(encodingProfileKey)
					+ ", _encodeData->_jsonProfile: " + _encodingItem->_encodeData->_jsonProfile
				);

                Json::Value encodingProfileDetails;
                {
                    try
                    {
                        Json::CharReaderBuilder builder;
                        Json::CharReader* reader = builder.newCharReader();
                        string errors;

                        bool parsingSuccessful = reader->parse(_encodingItem->_encodeData->_jsonProfile.c_str(),
                                _encodingItem->_encodeData->_jsonProfile.c_str() + _encodingItem->_encodeData->_jsonProfile.size(), 
                                &encodingProfileDetails, &errors);
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

				string fileFormat = encodingProfileDetails.get("FileFormat", "XXX").asString();
				string fileFormatLowerCase;
				fileFormatLowerCase.resize(fileFormat.size());
				transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
					[](unsigned char c){return tolower(c); } );

				// stagingEncodedAssetPathName preparation
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourcePhysicalPathKey);
					tie(mmsSourceAssetPathName, ignore, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName;
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

					{
						encodedFileName =
							to_string(_encodingItem->_ingestionJobKey)
							+ "_"
							+ to_string(_encodingItem->_encodingJobKey)
							+ "_" 
							+ to_string(encodingProfileKey);

						if (fileFormatLowerCase == "mp4")
							encodedFileName.append(".mp4");
						else if (fileFormatLowerCase == "mov")
							encodedFileName.append(".mov");
						else if (fileFormatLowerCase == "hls"
							|| fileFormatLowerCase == "dash")
							;
						else if (fileFormatLowerCase == "webm")
							encodedFileName.append(".webm");
						else if (fileFormatLowerCase == "ts")
							encodedFileName.append(".ts");
						else if (fileFormatLowerCase == "mkv")
							encodedFileName.append(".mkv");
						else
						{
							string errorMessage = __FILEREF__ + "Unknown fileFormat"
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", fileFormat: " + fileFormat
								;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}
					}

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

					if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
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

				Json::Value videoTracksRoot(Json::arrayValue);
				Json::Value audioTracksRoot(Json::arrayValue);
				if (_encodingItem->_encodeData->_contentType == MMSEngineDBFacade::ContentType::Video)
				{
					vector<tuple<int64_t, int, int64_t, int, int, string, string, long, string>> videoTracks;
					vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

					int64_t sourceMediaItemKey = -1;
					_mmsEngineDBFacade->getVideoDetails(
						sourceMediaItemKey, sourcePhysicalPathKey, videoTracks, audioTracks);

					for (tuple<int64_t, int, int64_t, int, int, string, string, long, string> videoTrack:
							videoTracks)
					{
						int trackIndex;
						tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, ignore, ignore)
							= videoTrack;

						if (trackIndex != -1)
						{
							Json::Value videoTrackRoot;

							string field = "trackIndex";
							videoTrackRoot[field] = trackIndex;

							videoTracksRoot.append(videoTrackRoot);
						}
					}

					for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack:
							audioTracks)
					{
						int trackIndex;
						string language;
						tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, language)
							= audioTrack;

						if (trackIndex != -1 && language != "")
						{
							Json::Value audioTrackRoot;

							string field = "trackIndex";
							audioTrackRoot[field] = trackIndex;

							field = "language";
							audioTrackRoot[field] = language;

							audioTracksRoot.append(audioTrackRoot);
						}
					}
				}
				else if (_encodingItem->_encodeData->_contentType == MMSEngineDBFacade::ContentType::Audio)
				{
					vector<tuple<int64_t, int, int64_t, long, string, long, int, string>> audioTracks;

					int64_t sourceMediaItemKey = -1;
					_mmsEngineDBFacade->getAudioDetails(
						sourceMediaItemKey, sourcePhysicalPathKey, audioTracks);

					for (tuple<int64_t, int, int64_t, long, string, long, int, string> audioTrack:
							audioTracks)
					{
						int trackIndex;
						string language;
						tie(ignore, trackIndex, ignore, ignore, ignore, ignore, ignore, language)
							= audioTrack;

						if (trackIndex != -1 && language != "")
						{
							Json::Value audioTrackRoot;

							string field = "trackIndex";
							audioTrackRoot[field] = trackIndex;

							field = "language";
							audioTrackRoot[field] = language;

							audioTracksRoot.append(audioTrackRoot);
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
                encodingMedatada["encodingProfileDetails"] = encodingProfileDetails;
                encodingMedatada["contentType"] = MMSEngineDBFacade::toString(_encodingItem->_encodeData->_contentType);
                encodingMedatada["physicalPathKey"] = (Json::LargestUInt) (sourcePhysicalPathKey);
                encodingMedatada["workspaceDirectoryName"] = _encodingItem->_workspace->_directoryName;
                encodingMedatada["relativePath"] = _encodingItem->_encodeData->_relativePath;
                encodingMedatada["encodingJobKey"] = (Json::LargestUInt) (_encodingItem->_encodingJobKey);
                encodingMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
                encodingMedatada["videoTracks"] = videoTracksRoot;
                encodingMedatada["audioTracks"] = audioTracksRoot;

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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		bool killedByUser = false;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
               
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
					urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
        sourcePhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);

        field = "encodingProfileKey";
        encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);
    }
    
	int64_t physicalItemRetentionInMinutes = -1;
	{
		string field = "PhysicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_encodeData->_ingestedParametersRoot, field))
		{
			string retention = _encodingItem->_encodeData->_ingestedParametersRoot.get(field, "1d").asString();
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
	}

	Json::Value encodingProfileDetails;
	{
		try
		{
			Json::CharReaderBuilder builder;
			Json::CharReader* reader = builder.newCharReader();
			string errors;

			bool parsingSuccessful = reader->parse(_encodingItem->_encodeData->_jsonProfile.c_str(),
				_encodingItem->_encodeData->_jsonProfile.c_str() + _encodingItem->_encodeData->_jsonProfile.size(), 
				&encodingProfileDetails, &errors);
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
	string fileFormat = encodingProfileDetails.get("FileFormat", "XXX").asString();
	string fileFormatLowerCase;
	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(),
		[](unsigned char c){return tolower(c); } );

	// manifestFileName is used in case of MMSEngineDBFacade::EncodingTechnology::MPEG2_TS
	// the manifestFileName naming convention is used also in FFMpeg.cpp
	string manifestFileName = to_string(_encodingItem->_ingestionJobKey) + "_"
			+ to_string(_encodingItem->_encodingJobKey);
	if (fileFormatLowerCase == "hls")
		manifestFileName += ".m3u8";
	else if (fileFormatLowerCase == "dash")
		manifestFileName += ".mpd";

	/*
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
	*/
	pair<int64_t, long> mediaInfoDetails;
	vector<tuple<int, int64_t, string, string, int, int, string, long>> videoTracks;
	vector<tuple<int, int64_t, string, long, int, long, string>> audioTracks;

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
		// tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> mediaInfo;
		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(stagingEncodedAssetPathName + "/" + manifestFileName,
					videoTracks, audioTracks);
		}
		else
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(stagingEncodedAssetPathName,
					videoTracks, audioTracks);
		}

        // tie(durationInMilliSeconds, bitRate, 
        //     videoCodecName, videoProfile, videoWidth, videoHeight, videoAvgFrameRate, videoBitRate,
        //     audioCodecName, audioSampleRate, audioChannels, audioBitRate) = mediaInfo;
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

		string relativePath = _encodingItem->_encodeData->_relativePath;

		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			size_t segmentsDirectoryIndex = stagingEncodedAssetPathName.find_last_of("/");
			if (segmentsDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "No segmentsDirectory find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName;
				_logger->error(errorMessage);

	            throw runtime_error(errorMessage);
			}

			// in case of MPEG2_TS next 'stagingEncodedAssetPathName.substr' extract the directory name
			// containing manifest and ts files. So relativePath has to be extended with this directory
			relativePath += (stagingEncodedAssetPathName.substr(segmentsDirectoryIndex + 1) + "/");

			// in case of MPEG2_TS, encodedFileName is the manifestFileName
			encodedFileName = manifestFileName;
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
            relativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            encodingProfileKey,
			physicalItemRetentionInMinutes,
                
			mediaInfoDetails,
			videoTracks,
			audioTracks,
			/*
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
			*/

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
            + ", relativePath: " + relativePath
            + ", encodedFileName: " + encodedFileName
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			+ ", e.what(): " + e.what()
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

        throw e;
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

        throw e;
    }
    
    return encodedPhysicalPathKey;
}

pair<string, bool> EncoderVideoAudioProxy::overlayImageOnVideo()
{
    pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser;
    
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
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::overlayImageOnVideo_through_ffmpeg()
{
    
	string encodersPool;
    int64_t sourceVideoPhysicalPathKey;
    int64_t sourceImagePhysicalPathKey;  
    string imagePosition_X_InPixel;
    string imagePosition_Y_InPixel;

    // _encodingItem->_encodingParametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_overlayImageOnVideoData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);

        field = "sourceImagePhysicalPathKey";
        sourceImagePhysicalPathKey = JSONUtils::asInt64(
				_encodingItem->_encodingParametersRoot, field, 0);

        field = "imagePosition_X_InPixel";
        imagePosition_X_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "imagePosition_Y_InPixel";
        imagePosition_Y_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegOverlayImageOnVideoURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;
		bool killedByUser = false;

		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace, encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_video =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_video;

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_image =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourceImagePhysicalPathKey);
					tie(mmsSourceImageAssetPathName, ignore, ignore, ignore, ignore, ignore)
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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(overlayContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
					urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
			_encodingItem->_overlayImageOnVideoData->_ingestedParametersRoot);
    
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
                    _encodingItem->_overlayImageOnVideoData->_ingestedParametersRoot,
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
    
	string encodersPool;
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

    // _encodingItem->_encodingParametersRoot filled in MMSEngineDBFacade::addOverlayTextOnVideoJob
    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_overlayTextOnVideoData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
				field, 0);

        field = "text";
        text = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "textPosition_X_InPixel";
        textPosition_X_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "textPosition_Y_InPixel";
        textPosition_Y_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "fontType";
        fontType = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "fontSize";
        fontSize = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "fontColor";
        fontColor = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "textPercentageOpacity";
        textPercentageOpacity = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "boxEnable";
        boxEnable = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "boxColor";
        boxColor = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "boxPercentageOpacity";
        boxPercentageOpacity = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegOverlayTextOnVideoURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;

		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
				+ ffmpegURI
				+ "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsSourceVideoAssetPathName;

    
				// stagingEncodedAssetPathName preparation
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(overlayTextContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, stagingEncodedAssetPathName);

		bool killedByUser = false;
		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
					urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
			_encodingItem->_overlayTextOnVideoData->_ingestedParametersRoot);
    
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
                        _encodingItem->_overlayTextOnVideoData->_ingestedParametersRoot,
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
    
	string encodersPool;
    int64_t sourceVideoPhysicalPathKey;
    string videoSpeedType;
    int videoSpeedSize;

    // _encodingItem->_encodingParametersRoot filled in MMSEngineDBFacade::addOverlayTextOnVideoJob
    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_videoSpeedData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "videoSpeedType";
        videoSpeedType = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "videoSpeedSize";
        videoSpeedSize = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegVideoSpeedURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;

		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
				+ ffmpegURI
				+ "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				// string encodedFileName;
				string mmsSourceVideoAssetPathName;

    
				// stagingEncodedAssetPathName preparation
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(videoSpeedContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, stagingEncodedAssetPathName);

		bool killedByUser = false;

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
			_encodingItem->_videoSpeedData->_ingestedParametersRoot);
    
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
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::pictureInPicture_through_ffmpeg()
{

	string encodersPool;
    int64_t mainVideoPhysicalPathKey;
    int64_t overlayVideoPhysicalPathKey;
    string overlayPosition_X_InPixel;
    string overlayPosition_Y_InPixel;
    string overlay_Width_InPixel;
    string overlay_Height_InPixel;
	bool soundOfMain;

    // _encodingItem->_encodingParametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_pictureInPictureData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "mainVideoPhysicalPathKey";
        mainVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "overlayVideoPhysicalPathKey";
        overlayVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "overlayPosition_X_InPixel";
        overlayPosition_X_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "overlayPosition_Y_InPixel";
        overlayPosition_Y_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "overlay_Width_InPixel";
        overlay_Width_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "overlay_Height_InPixel";
        overlay_Height_InPixel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "soundOfMain";
        soundOfMain = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, true);
    }

	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegPictureInPictureURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string stagingEncodedAssetPathName;
		bool killedByUser = false;

		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_main =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, mainVideoPhysicalPathKey);
					tie(mmsMainVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
						= physicalPathFileNameSizeInBytesAndDeliveryFileName_main;

					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName_overlay =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, overlayVideoPhysicalPathKey);
					tie(mmsOverlayVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(pictureInPictureContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, stagingEncodedAssetPathName);

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
			_encodingItem->_pictureInPictureData->_ingestedParametersRoot);
    
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
}

bool EncoderVideoAudioProxy::generateFrames_through_ffmpeg()
{
    
	string encodersPool;
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

    // _encodingItem->_encodingParametersRoot filled in MMSEngineDBFacade::addOverlayImageOnVideoJob
    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_generateFramesData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "sourceVideoPhysicalPathKey";
        sourceVideoPhysicalPathKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "imageDirectory";
        imageDirectory = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "startTimeInSeconds";
        startTimeInSeconds = JSONUtils::asDouble(_encodingItem->_encodingParametersRoot, field, 0);

        field = "maxFramesNumber";
        maxFramesNumber = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "videoFilter";
        videoFilter = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();

        field = "periodInSeconds";
        periodInSeconds = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "mjpeg";
        mjpeg = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "imageWidth";
        imageWidth = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "imageHeight";
        imageHeight = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "ingestionJobKey";
        ingestionJobKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "videoDurationInMilliSeconds";
        videoDurationInMilliSeconds = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegGenerateFramesURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		if (_encodingItem->_encoderKey == -1) // || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
				+ ffmpegURI
				+ "/" + to_string(_encodingItem->_encodingJobKey)
            ;
            string body;
            {
				string mmsSourceVideoAssetPathName;

				// stagingEncodedAssetPathName preparation
				{
					tuple<string, int, string, string, int64_t, string> physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, sourceVideoPhysicalPathKey);
					tie(mmsSourceVideoAssetPathName, ignore, ignore, ignore, ignore, ignore)
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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(generateFramesContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			// stagingEncodedAssetPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, "");

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
    multiLocalAssetIngestionEvent->setParametersRoot(_encodingItem->_generateFramesData->_ingestedParametersRoot);

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
    
    return stagingEncodedAssetPathNameAndKilledByUser;
}

pair<string, bool> EncoderVideoAudioProxy::slideShow_through_ffmpeg()
{
    
	string encodersPool;
    int outputFrameRate;  
    Json::Value imagesSourcePhysicalPathsRoot(Json::arrayValue);
    double durationOfEachSlideInSeconds;
    Json::Value audiosSourcePhysicalPathsRoot(Json::arrayValue);
    double shortestAudioDurationInSeconds;

    {
        string field = "EncodersPool";
        encodersPool = _encodingItem->_slideShowData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "outputFrameRate";
        outputFrameRate = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "imagesSourcePhysicalPaths";
        imagesSourcePhysicalPathsRoot = _encodingItem->_encodingParametersRoot[field];

        field = "durationOfEachSlideInSeconds";
        durationOfEachSlideInSeconds = JSONUtils::asDouble(_encodingItem->_encodingParametersRoot, field, 0);

        field = "audiosSourcePhysicalPaths";
        audiosSourcePhysicalPathsRoot = _encodingItem->_encodingParametersRoot[field];

        field = "shortestAudioDurationInSeconds";
        shortestAudioDurationInSeconds = JSONUtils::asDouble(_encodingItem->_encodingParametersRoot, field, 0);
    }
    
	string ffmpegEncoderURL;
	string ffmpegURI = _ffmpegSlideShowURI;
	ostringstream response;
	bool responseInitialized = false;
	try
	{
		string slideShowMediaPathName;

		if (_encodingItem->_encoderKey == -1 || _encodingItem->_stagingEncodedAssetPathName == "")
		{
			/*
			string encoderToSkip;
            _currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace,
					encoderToSkip);
			*/
			int64_t encoderKeyToBeSkipped = -1;
            pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

            _logger->info(__FILEREF__ + "Configuration item"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            // ffmpegEncoderURL = 
            //         _ffmpegEncoderProtocol
            //         + "://"
            //         + _currentUsedFFMpegEncoderHost + ":"
            //         + to_string(_ffmpegEncoderPort)
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
                slideShowMedatada["videoSyncMethod"] = _encodingItem->
					_encodingParametersRoot.get("videoSyncMethod", "").asString();
                slideShowMedatada["outputFrameRate"] = outputFrameRate;
                slideShowMedatada["slideShowMediaPathName"] = slideShowMediaPathName;
                slideShowMedatada["imagesSourcePhysicalPaths"] = imagesSourcePhysicalPathsRoot;
                slideShowMedatada["durationOfEachSlideInSeconds"] = durationOfEachSlideInSeconds;
                slideShowMedatada["audiosSourcePhysicalPaths"] = audiosSourcePhysicalPathsRoot;
                slideShowMedatada["shortestAudioDurationInSeconds"] = shortestAudioDurationInSeconds;

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

			// timeout consistent with nginx configuration (fastcgi_read_timeout)
			request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

            // if (_ffmpegEncoderProtocol == "https")
			string httpsPrefix("https");
			if (ffmpegEncoderURL.size() >= httpsPrefix.size()
				&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
            while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
                if (JSONUtils::isMetadataPresent(slideShowContentResponse, field))
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
                    if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
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
				+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				+ ", stagingEncodedAssetPathName: " + _encodingItem->_stagingEncodedAssetPathName
			);

			_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
			slideShowMediaPathName = _encodingItem->_stagingEncodedAssetPathName;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

			// ffmpegEncoderURL = 
            //        _ffmpegEncoderProtocol
            //        + "://"
            //        + _currentUsedFFMpegEncoderHost + ":"
            //        + to_string(_ffmpegEncoderPort)
			ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
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
			+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
		);
		_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
			_currentUsedFFMpegEncoderKey, slideShowMediaPathName);

		bool killedByUser = false;

		// loop waiting the end of the encoding
		bool encodingFinished = false;
		bool completedWithError = false;
		string encodingErrorMessage;
		bool urlForbidden = false;
		bool urlNotFound = false;
		int encodingProgress = 0;
		int maxEncodingStatusFailures = 1;	// consecutive errors
		int encodingStatusFailures = 0;
		int encodingPid;
		int lastEncodingPid = 0;
		while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
		{
			this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
                
			try
			{
				tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
					getEncodingStatus(/* _encodingItem->_encodingJobKey */);
				tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

				if (encodingErrorMessage != "")
				{
					try
					{
						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, encodingErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}

				if (completedWithError)
				{
					string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingErrorMessage: " + encodingErrorMessage
						;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				// encodingProgress/encodingPid
				{
					try
					{
						_logger->info(__FILEREF__ + "updateEncodingJobProgress"
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingProgress: " + to_string(encodingProgress)
						);
						_mmsEngineDBFacade->updateEncodingJobProgress (
							_encodingItem->_encodingJobKey, encodingProgress);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);
					}

					if (lastEncodingPid != encodingPid)
					{
						try
						{
							_logger->info(__FILEREF__ + "updateEncodingPid"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
							_mmsEngineDBFacade->updateEncodingPid (
								_encodingItem->_encodingJobKey, encodingPid);

							lastEncodingPid = encodingPid;
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingPid failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", _encodingPid: " + to_string(encodingPid)
							);
						}
					}
				}

				// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
				//	Scenario:
				//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
				//			returns 'timeout'.
				//			Really the encoder was working fine, ffmpeg was also running fine,
				//			just FastCGIAccept was not getting the request
				//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
				//			and this thread terminates, even if the encoder and ffmpeg was working fine.
				//		This scenario creates problems and non-consistency between engine and encoder.
				//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
				encodingStatusFailures = 0;
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
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
        outputFrameRate = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);
    
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
			_encodingItem->_slideShowData->_ingestedParametersRoot);
    
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

	_localEncodingProgress = 0;

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
		sourceMediaItemKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

		field = "faceRecognitionCascadeName";
		faceRecognitionCascadeName = _encodingItem->_encodingParametersRoot.get(field, 0).asString();

		field = "sourcePhysicalPath";
		sourcePhysicalPath = _encodingItem->_encodingParametersRoot.get(field, 0).asString();

		// VideoWithHighlightedFaces, ImagesToBeUsedInDeepLearnedModel or FrameContainingFace
		field = "faceRecognitionOutput";
		faceRecognitionOutput = _encodingItem->_encodingParametersRoot.get(field, 0).asString();

		field = "initialFramesNumberToBeSkipped";
		initialFramesNumberToBeSkipped = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

		field = "oneFramePerSecond";
		oneFramePerSecond = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);
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
		+ ", @MMS statistics@ - duration (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(endCapture - startCapture).count()) + "@"
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

	int progressNotificationPeriodInSeconds = 2;
	chrono::system_clock::time_point lastProgressNotification =
		chrono::system_clock::now() - chrono::seconds(progressNotificationPeriodInSeconds + 1);

	long currentFrameIndex = 0;
	long framesContainingFaces = 0;

	bool bgrFrameEmpty = false;
	if (faceRecognitionOutput == "FrameContainingFace")
	{
		long initialFrameIndex = 0;
		while (initialFrameIndex++ < initialFramesNumberToBeSkipped)
		{
			if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastProgressNotification).count()
				> progressNotificationPeriodInSeconds)
			{
				lastProgressNotification = chrono::system_clock::now();

				_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;

				try
				{
					_logger->info(__FILEREF__ + "updateEncodingJobProgress"
						+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingProgress: " + to_string(_localEncodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress (
						_encodingItem->_encodingJobKey, _localEncodingProgress);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}

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
		if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastProgressNotification).count()
				> progressNotificationPeriodInSeconds)
		{
			lastProgressNotification = chrono::system_clock::now();

			/*
			double progress = (currentFrameIndex / totalFramesNumber) * 100;
			// this is to have one decimal in the percentage
			double faceRecognitionPercentage = ((double) ((int) (progress * 10))) / 10;
			*/
			_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;

			try
			{
				_logger->info(__FILEREF__ + "updateEncodingJobProgress"
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingProgress: " + to_string(_localEncodingProgress)
				);
				_mmsEngineDBFacade->updateEncodingJobProgress (
					_encodingItem->_encodingJobKey, _localEncodingProgress);
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				);
			}

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
							_encodingItem->_faceRecognitionData->_ingestedParametersRoot);
    
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
					_encodingItem->_faceRecognitionData->_ingestedParametersRoot);
  
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
				_encodingItem->_faceRecognitionData->_ingestedParametersRoot);
  
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
			faceRecognitionOutput = _encodingItem->_encodingParametersRoot.get(field, 0).asString();
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
			_encodingItem->_faceRecognitionData->_ingestedParametersRoot);
    
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

	_localEncodingProgress = 0;

	// build the deep learned model
	vector<cv::Mat> images;
	vector<int> idImages;
	unordered_map<int, string> idTagMap;
	{
		vector<string> deepLearnedModelTags;

		string field = "deepLearnedModelTagsCommaSeparated";
		stringstream ssDeepLearnedModelTagsCommaSeparated (_encodingItem->_encodingParametersRoot.get(field, "").asString());
		while (ssDeepLearnedModelTagsCommaSeparated.good())
		{
			string tag;
			getline(ssDeepLearnedModelTagsCommaSeparated, tag, ',');

			deepLearnedModelTags.push_back(tag);
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
		string orderBy;
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
				orderBy, jsonOrderBy, admin);

			field = "response";
			Json::Value responseRoot = mediaItemsListRoot[field];

			if (totalImagesNumber == -1)
			{
				field = "numFound";
				totalImagesNumber = JSONUtils::asInt(responseRoot, field, 0);
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
					int64_t physicalPathKey = JSONUtils::asInt64(physicalPathRoot, field, 0);

					tuple<string, int, string, string, int64_t, string>
						physicalPathFileNameSizeInBytesAndDeliveryFileName =
						_mmsStorage->getPhysicalPath(_mmsEngineDBFacade, physicalPathKey);
					string mmsImagePathName;
					tie(mmsImagePathName, ignore, ignore, ignore, ignore, ignore)
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
		faceIdentificationCascadeName = _encodingItem->_encodingParametersRoot.get(field, 0).asString();

		field = "sourcePhysicalPath";
		sourcePhysicalPath = _encodingItem->_encodingParametersRoot.get(field, 0).asString();
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
		+ ", @MMS statistics@ - duration (secs): @"
			+ to_string(chrono::duration_cast<chrono::seconds>(endCapture - startCapture).count()) + "@"
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

	int progressNotificationPeriodInSeconds = 2;
	chrono::system_clock::time_point lastProgressNotification =
		chrono::system_clock::now() - chrono::seconds(progressNotificationPeriodInSeconds + 1);

	cv::Mat bgrFrame;
	cv::Mat grayFrame;
	cv::Mat smallFrame;

	long currentFrameIndex = 0;
	while(true)
	{
		if (chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - lastProgressNotification).count()
				> progressNotificationPeriodInSeconds)
		{
			lastProgressNotification = chrono::system_clock::now();

			/*
			double progress = (currentFrameIndex / totalFramesNumber) * 100;
			// this is to have one decimal in the percentage
			double faceRecognitionPercentage = ((double) ((int) (progress * 10))) / 10;
			*/
			_localEncodingProgress = 100 * currentFrameIndex / totalFramesNumber;

			try
			{
				_logger->info(__FILEREF__ + "updateEncodingJobProgress"
					+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingProgress: " + to_string(_localEncodingProgress)
				);
				_mmsEngineDBFacade->updateEncodingJobProgress (
					_encodingItem->_encodingJobKey, _localEncodingProgress);
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", e.what(): " + e.what()
				);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				);
			}

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

		capture >> bgrFrame;
		if (bgrFrame.empty())
			break;

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
			_encodingItem->_faceIdentificationData->_ingestedParametersRoot);
    
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
	int monitorVirtualVODSegmentDurationInSeconds;
	bool autoRenew;
	{
        string field = "autoRenew";
        autoRenew = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "utcRecordingPeriodStart";
        utcRecordingPeriodStart = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "monitorVirtualVODSegmentDurationInSeconds";
        monitorVirtualVODSegmentDurationInSeconds = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

		string segmenterType = "hlsSegmenter";
		// string segmenterType = "streamSegmenter";
		if (segmenterType == "streamSegmenter")
		{
			// since the first chunk is discarded, we will start recording before the period of the chunk
			utcRecordingPeriodStart -= monitorVirtualVODSegmentDurationInSeconds;
		}

        field = "utcRecordingPeriodEnd";
        utcRecordingPeriodEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);
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

	// string channelType;
	string encodersPool;
	int64_t channelConfKey;
	bool highAvailability;
	bool main;
	string liveURL;
	string userAgent;
	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	bool monitorHLS = false;
	bool virtualVOD = false;
	int monitorVirtualVODSegmentDurationInSeconds;
	string outputFileFormat;
	{
		/*
        string field = "ChannelType";
        channelType = _encodingItem->_liveRecorderData->
			_ingestedParametersRoot.get(field, "IP_MMSAsClient").asString();
		*/

        string field = "EncodersPool";
        encodersPool = _encodingItem->_liveRecorderData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "confKey";
        channelConfKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "highAvailability";
        highAvailability = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "main";
        main = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "url";
        liveURL = _encodingItem->_encodingParametersRoot.get(field, "").asString();

        field = "userAgent";
        userAgent = _encodingItem->_encodingParametersRoot.get(field, "").asString();

        field = "utcRecordingPeriodStart";
        utcRecordingPeriodStart = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "utcRecordingPeriodEnd";
        utcRecordingPeriodEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "autoRenew";
        autoRenew = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "monitorHLS";
        monitorHLS = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "liveRecorderVirtualVOD";
        virtualVOD = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

        field = "monitorVirtualVODSegmentDurationInSeconds";
        monitorVirtualVODSegmentDurationInSeconds = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "outputFileFormat";
        outputFileFormat = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	time_t utcNowToCheckExit = 0;
	while (!killedByUser && !urlForbidden && !urlNotFound
			&& utcNowToCheckExit < utcRecordingPeriodEnd)
	{
		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveRecorderURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			if (_encodingItem->_encoderKey == -1)
			{
				// In case of HighAvailability, main and backup should run on different server.
				// The below algorithm assigns first the main transcoder and, only
				// once the main transcoder is assigned, trying to assign the backup transcoder
				// case Main:
				//		1. look for the transcoder of the backup
				//		2. if it is no defined
				//			a "random" transcoder is used
				//		   else
				//			a transcoder different by the backupTranscoder (if exist) is used
				// case Backup:
				//		1. look for the mainTransocder
				//		2. if present
				//				a transcoder different by the mainTranscoder (if exist) is used
				//		   else
				//				sleep waiting main transcoder is assigned and come back to 1. The loop 1. and 2. is executed up to 60 secs
				if (highAvailability)
				{
					if (main)
					{
						/*
						int64_t backupEncoderKey = _mmsEngineDBFacade->getLiveRecorderOtherTranscoder(
							main, _encodingItem->_encodingJobKey);

						if (backupEncoderKey == -1)
						*/
						{
							_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (main). "
								+ "BackupTranscoder is not selected yet. Just get a transcoder"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", highAvailability: " + to_string(highAvailability)
								+ ", main: " + to_string(main)
							);

							/*
							string transcoderToSKip;
							_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
									encodersPool, _encodingItem->_workspace, transcoderToSKip);
							*/
							int64_t encoderKeyToBeSkipped = -1;
							pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
								encodersPool, _encodingItem->_workspace,
								encoderKeyToBeSkipped);
							tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;
						}
						/*
						else
						{
							_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (main). "
								+ "BackupTranscoder is already selected. Just get another transcoder"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", highAvailability: " + to_string(highAvailability)
								+ ", main: " + to_string(main)
								+ ", backupEncoderKey: " + to_string(backupEncoderKey)
							);

							int64_t encoderKeyToBeSkipped = backupEncoderKey;
							pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
									encodersPool, _encodingItem->_workspace,
									encoderKeyToBeSkipped);
							tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;
						}
						*/
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
							/*
							string mainTranscoder;
							int64_t mainEncoderKey;
							pair<int64_t, string> otherTranscoder =
								_mmsEngineDBFacade->getLiveRecorderOtherTranscoder(
									main, _encodingItem->_encodingJobKey);
							tie(mainEncoderKey, mainTranscoder) = otherTranscoder;
							*/
							int64_t mainEncoderKey = _mmsEngineDBFacade->getLiveRecorderOtherTranscoder(
									main, _encodingItem->_encodingJobKey);

							if (mainEncoderKey == -1)
							{
								if (chrono::duration_cast<chrono::seconds>(
									(chrono::system_clock::now() + chrono::seconds (intervalInSecondsToWaitLiveRecorderMainTranscoder)) - startPoint).count() >
										maxWaitInSecondsForLiveRecorderMainTranscoder)
								{
									string errorMessage = string("LiveRecorder. Selection of the transcoder (backup). ")
										+ "Main Transcoder is not selected and time expired, set this encodingJob as failed"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", highAvailability: " + to_string(highAvailability)
										+ ", main: " + to_string(main)
									;
									_logger->info(__FILEREF__ + errorMessage);

									throw EncoderNotFound(errorMessage);
									/*
									int64_t encoderKeyToBeSkipped = -1;
									pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
											encodersPool, _encodingItem->_workspace,
											encoderKeyToBeSkipped);
									tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

									transcoderFound = true;
									*/
								}
								else
								{
									_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (backup). "
										+ "Main Transcoder is not selected. Just wait a bit"
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
								_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder (backup). "
									+ "Main Transcoder is already selected. Just get another transcoder"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", highAvailability: " + to_string(highAvailability)
									+ ", main: " + to_string(main)
									+ ", mainEncoderKey: " + to_string(mainEncoderKey)
								);

								/*
								string transcoderToSKip = mainTranscoder;
								_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
									encodersPool, _encodingItem->_workspace, transcoderToSKip);
								*/
								int64_t encoderKeyToBeSkipped = mainEncoderKey;
								pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
										encodersPool, _encodingItem->_workspace,
										encoderKeyToBeSkipped);
								tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

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

					/*
					string encoderToSKip;
					_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
							encodersPool, _encodingItem->_workspace, encoderToSKip);
					*/
					int64_t encoderKeyToBeSkipped = -1;
					pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
							encodersPool, _encodingItem->_workspace,
							encoderKeyToBeSkipped);
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;
				}

				_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
				;

				string body;
				{
					// the recorder generates the chunks in a local(transcoder) directory
					string transcoderStagingContentsPath;

					// the chunks are moved to a shared directory to be ingested using a copy/move source URL
					string stagingContentsPath;

					// playlist where the recorder writes the chunks generated
					string segmentListFileName;

					string recordedFileNamePrefix;

					// the virtual VOD is generated into a shared directory to be ingested
					// using a copy/move source URL
					string virtualVODStagingContentsPath;

					int64_t liveRecorderVirtualVODImageMediaItemKey = -1;

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

						if (virtualVOD)
						{
							bool removeLinuxPathIfExist = false;
							bool neededForTranscoder = false;
							virtualVODStagingContentsPath = _mmsStorage->getStagingAssetPathName(
								neededForTranscoder,
								_encodingItem->_workspace->_directoryName,
								to_string(_encodingItem->_ingestionJobKey) + "_virtualVOD",	// directoryNamePrefix,
								"/",    // _encodingItem->_relativePath,
								to_string(_encodingItem->_ingestionJobKey) + "_liveRecorderVirtualVOD",
								-1, // _encodingItem->_mediaItemKey, not used because encodedFileName is not ""
								-1, // _encodingItem->_physicalPathKey, not used because encodedFileName is not ""
								removeLinuxPathIfExist);
						}

						if (virtualVOD && _liveRecorderVirtualVODImageLabel != "")
						{
							try
							{
								bool warningIfMissing = true;
								pair<int64_t,MMSEngineDBFacade::ContentType> mediaItemDetails =
									_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName(
									_encodingItem->_workspace->_workspaceKey,
									_liveRecorderVirtualVODImageLabel,
									warningIfMissing);

								tie(liveRecorderVirtualVODImageMediaItemKey, ignore) = mediaItemDetails;
							}
							catch (exception e)
							{
								_logger->error(__FILEREF__ + "_mmsEngineDBFacade->getMediaItemKeyDetailsByUniqueName failed"
									+ ", _liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel
									+ ", exception: " + e.what()
								);

								liveRecorderVirtualVODImageMediaItemKey = -1;
							}
						}
					}

					string localLiveURL = liveURL;
					// in case of youtube url, the real URL to be used has to be calcolated
					{
						string youTubePrefix1 ("https://www.youtube.com/");
						string youTubePrefix2 ("https://youtu.be/");
						if (
							(liveURL.size() >= youTubePrefix1.size()
								&& 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1))
							||
							(liveURL.size() >= youTubePrefix2.size()
								&& 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2))
							)
						{
							string streamingYouTubeLiveURL;
							long hoursFromLastCalculatedURL;
							pair<long,string> lastYouTubeURLDetails;
							try
							{
								lastYouTubeURLDetails = getLastYouTubeURLDetails(
									_encodingItem->_ingestionJobKey,
									_encodingItem->_encodingJobKey,
									_encodingItem->_workspace->_workspaceKey,
									channelConfKey);

								string lastCalculatedURL;

								tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

								_logger->info(__FILEREF__
									+ "LiveRecorder. check youTubeURLCalculate"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									+ ", _retrieveStreamingYouTubeURLPeriodInHours: " + to_string(_retrieveStreamingYouTubeURLPeriodInHours)
								);
								if (hoursFromLastCalculatedURL < _retrieveStreamingYouTubeURLPeriodInHours)
									streamingYouTubeLiveURL = lastCalculatedURL;
							}
							catch(runtime_error e)
							{
								string errorMessage = __FILEREF__
									+ "LiveRecorder. youTubeURLCalculate. getLastYouTubeURLDetails failed"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", YouTube URL: " + streamingYouTubeLiveURL
								;
								_logger->error(errorMessage);
							}

							if (streamingYouTubeLiveURL == "")
							{
								try
								{
									FFMpeg ffmpeg (_configuration, _logger);
									pair<string, string> streamingLiveURLDetails =
										ffmpeg.retrieveStreamingYouTubeURL(
										_encodingItem->_ingestionJobKey,
										_encodingItem->_encodingJobKey,
										liveURL);

									tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

									_logger->info(__FILEREF__ + "LiveRecorder. youTubeURLCalculate. Retrieve streaming YouTube URL"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", channelConfKey: " + to_string(channelConfKey)
										+ ", initial YouTube URL: " + liveURL
										+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
										+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									);
								}
								catch(runtime_error e)
								{
									// in case ffmpeg.retrieveStreamingYouTubeURL fails
									// we will use the last saved URL
									tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

									string errorMessage = __FILEREF__
										+ "LiveRecorder. youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", channelConfKey: " + to_string(channelConfKey)
										+ ", YouTube URL: " + streamingYouTubeLiveURL
									;
									_logger->error(errorMessage);

									try
									{
										_mmsEngineDBFacade->appendIngestionJobErrorMessage(
											_encodingItem->_ingestionJobKey, errorMessage);
									}
									catch(runtime_error e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
											+ ", e.what(): " + e.what()
										);
									}
									catch(exception e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
										);
									}

									if (streamingYouTubeLiveURL == "")
									{
										// 2020-04-21: let's go ahead because it would be managed
										// the killing of the encodingJob
										// 2020-09-17: it does not have sense to continue
										//	if we do not have the right URL (m3u8)
										throw YouTubeURLNotRetrieved();
									}
								}

								if (streamingYouTubeLiveURL != "")
								{
									try
									{
										updateChannelDataWithNewYouTubeURL(
											_encodingItem->_ingestionJobKey,
											_encodingItem->_encodingJobKey,
											_encodingItem->_workspace->_workspaceKey,
											channelConfKey,
											streamingYouTubeLiveURL);
									}
									catch(runtime_error e)
									{
										string errorMessage = __FILEREF__
											+ "LiveRecorder. youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
											+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
											+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
											+ ", channelConfKey: " + to_string(channelConfKey)
											+ ", YouTube URL: " + streamingYouTubeLiveURL
										;
										_logger->error(errorMessage);
									}
								}
							}
							else
							{
								_logger->info(__FILEREF__ + "LiveRecorder. youTubeURLCalculate. Reuse a previous streaming YouTube URL"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", initial YouTube URL: " + liveURL
									+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
								);
							}

							localLiveURL = streamingYouTubeLiveURL;
						}
					}

					Json::Value liveRecorderMedatada;

					liveRecorderMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
					liveRecorderMedatada["userAgent"] = userAgent;
					liveRecorderMedatada["transcoderStagingContentsPath"] = transcoderStagingContentsPath;
					liveRecorderMedatada["stagingContentsPath"] = stagingContentsPath;
					liveRecorderMedatada["virtualVODStagingContentsPath"] = virtualVODStagingContentsPath;
					liveRecorderMedatada["liveRecorderVirtualVODImageMediaItemKey"] =
						liveRecorderVirtualVODImageMediaItemKey;
					liveRecorderMedatada["segmentListFileName"] = segmentListFileName;
					liveRecorderMedatada["recordedFileNamePrefix"] = recordedFileNamePrefix;
					liveRecorderMedatada["encodingParametersRoot"] =
						_encodingItem->_encodingParametersRoot;
					liveRecorderMedatada["liveRecorderParametersRoot"] =
						_encodingItem->_liveRecorderData->_ingestedParametersRoot;
					liveRecorderMedatada["monitorVirtualVODEncodingProfileContentType"] =
						MMSEngineDBFacade::toString(_encodingItem->_liveRecorderData
							->_monitorVirtualVODEncodingProfileContentType);
					liveRecorderMedatada["monitorVirtualVODEncodingProfileDetailsRoot"] =
						_encodingItem->_liveRecorderData->_monitorVirtualVODEncodingProfileDetailsRoot;
					liveRecorderMedatada["liveURL"] = localLiveURL;

					{
						Json::StreamWriterBuilder wbuilder;
                    
						body = Json::writeString(wbuilder, liveRecorderMedatada);
					}
				}
            
				list<string> header;

				header.push_back("Content-Type: application/json");
				{
					string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":"
						+ _ffmpegEncoderPassword);
					string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

					header.push_back(basicAuthorization);
				}
            
				curlpp::Cleanup cleaner;
				curlpp::Easy request;

				// Setting the URL to retrive.
				request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

				// timeout consistent with nginx configuration (fastcgi_read_timeout)
				request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

				// if (_ffmpegEncoderProtocol == "https")
				string httpsPrefix("https");
				if (ffmpegEncoderURL.size() >= httpsPrefix.size()
					&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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
				while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
					if (JSONUtils::isMetadataPresent(liveRecorderContentResponse, field))
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
					+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				);

				_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
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
				+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey,
				_currentUsedFFMpegEncoderKey, "");

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
            int maxEncodingStatusFailures = 5;	// consecutive errors
            int encodingStatusFailures = 0;
			int encodingPid;
			int lastEncodingPid = 0;
			// string lastRecordedAssetFileName;

			// see the comment few lines below (2019-05-03)
            // while(!(encodingFinished || encodingStatusFailures >= maxEncodingStatusFailures))
            while(!encodingFinished)
            {
                this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

                try
                {
					tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
						getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, ignore, encodingPid) = encodingStatus;

					if (encodingErrorMessage != "")
					{
						try
						{
							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, encodingErrorMessage);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError)
					{
						string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingErrorMessage: " + encodingErrorMessage
							;
						_logger->error(errorMessage);

						encodingStatusFailures++;

						// in this scenario encodingFinished is true

						// update EncodingJob failures number to notify the GUI EncodingJob is failing
						try
						{
							_logger->info(__FILEREF__ + "check and update encodingJob FailuresNumber"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
							);

							long previousEncodingStatusFailures =
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
									_encodingItem->_encodingJobKey, 
									encodingStatusFailures);
							if (previousEncodingStatusFailures < 0)
							{
								_logger->info(__FILEREF__ + "LiveRecorder Killed by user during waiting loop"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								// when previousEncodingStatusFailures is < 0 means:
								// 1. the live recording is not starting (ffmpeg is generating
								//		continuously an error)
								// 2. User killed the encoding through MMS GUI or API
								// 3. the kill procedure (in API module) was not able to kill the ffmpeg process,
								//		because it does not exist the process and set the failuresNumber DB field
								//		to a negative value in order to communicate with this thread 
								// 4. This thread, when it finds a negative failuresNumber, knows the encoding
								//		was killed and exit from the loop
								encodingFinished = true;
								killedByUser = true;
							}
						}
						catch(...)
						{
							_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
							);
						}

						throw runtime_error(errorMessage);
					}

					// encodingProgress/encodingPid
					{
						try
						{
							time_t utcNow;
							{
								chrono::system_clock::time_point now = chrono::system_clock::now();
								utcNow = chrono::system_clock::to_time_t(now);
							}

							int encodingProgress;

							if (utcNow < utcRecordingPeriodStart)
								encodingProgress = 0;
							else if (utcRecordingPeriodStart < utcNow && utcNow < utcRecordingPeriodEnd)
								encodingProgress = ((utcNow - utcRecordingPeriodStart) * 100) /
									(utcRecordingPeriodEnd - utcRecordingPeriodStart);
							else
								encodingProgress = 100;

							_logger->info(__FILEREF__ + "updateEncodingJobProgress"
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingProgress: " + to_string(encodingProgress)
							);
							_mmsEngineDBFacade->updateEncodingJobProgress (
								_encodingItem->_encodingJobKey, encodingProgress);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							);
						}

						if (lastEncodingPid != encodingPid)
						{
							try
							{
								_logger->info(__FILEREF__ + "updateEncodingPid"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingPid: " + to_string(encodingPid)
								);
								_mmsEngineDBFacade->updateEncodingPid (
									_encodingItem->_encodingJobKey, encodingPid);

								lastEncodingPid = encodingPid;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
								);
							}
						}
					}

					// 2020-06-10: encodingStatusFailures is reset since getEncodingStatus was successful.
					//	Scenario:
					//		1. only sometimes (about once every two hours) an encoder (deployed on centos) running a LiveRecorder continuously,
					//			returns 'timeout'.
					//			Really the encoder was working fine, ffmpeg was also running fine,
					//			just FastCGIAccept was not getting the request
					//		2. these errors was increasing encodingStatusFailures and at the end, it reached the max failures
					//			and this thread terminates, even if the encoder and ffmpeg was working fine.
					//		This scenario creates problems and non-consistency between engine and encoder.
					//		For this reason, if the getEncodingStatus is successful, encodingStatusFailures is reset.
                    encodingStatusFailures = 0;

					/*
					lastRecordedAssetFileName = processLastGeneratedLiveRecorderFiles(
						highAvailability, main, segmentDurationInSeconds,
						transcoderStagingContentsPath + segmentListFileName, recordedFileNamePrefix,
						liveRecordingContentsPath, lastRecordedAssetFileName);
					*/
                }
                catch(...)
                {
					// 2020-05-20: The initial loop will make the liveRecording to exit in case of urlNotFound.
					//	This is because in case the URL is not found, does not have sense to try again the liveRecording.
					//	This is true in case the URL not found error happens at the beginning of the liveRecording.
					//	This is not always the case. Sometimes the URLNotFound error is returned by ffmpeg after a lot of time
					//	the liveRecoridng is started and because just one ts file was not found (this is in case of m3u8 URL).
					//	In this case we do not have to exit from the loop and we have just to try again
					long urlNotFoundFakeAfterMinutes = 10;
					long encodingDurationInMinutes = chrono::duration_cast<chrono::minutes>(
						chrono::system_clock::now() - startEncoding).count();
					if (urlNotFound && encodingDurationInMinutes > urlNotFoundFakeAfterMinutes)
					{
						// 2020-06-06. Scenario:
						//	- MMS was sending RAI 1 to CDN77
						//	- here we were recording the streaming poiting to the CDN77 URL
						//	- MMS has an error (restarted because of 'Non-monotonous DTS in output stream/incorrect timestamps')
						//	- here we had the URL not found error
						// Asking again the URL raises again 'URL not found' error. For this reason we added
						// a waiting, let's see if 60 seconds is enough
						int waitingInSeconsBeforeTryingAgain = 60;

						_logger->error(__FILEREF__ + "fake urlNotFound, wait a bit and try again"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
							+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
							+ ", waitingInSeconsBeforeTryingAgain: " + to_string(waitingInSeconsBeforeTryingAgain)
						);

						urlNotFound = false;

						// in case URL not found is because of a segment not found
						// or in case of a temporary failures
						//		let's wait a bit before to try again
						this_thread::sleep_for(chrono::seconds(waitingInSeconsBeforeTryingAgain));
					}
					else
					{
						_logger->info(__FILEREF__ + "it is not a fake urlNotFound"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", urlNotFound: " + to_string(urlNotFound)
							+ ", @MMS statistics@ - encodingDurationInMinutes: @" + to_string(encodingDurationInMinutes) + "@"
							+ ", urlNotFoundFakeAfterMinutes: " + to_string(urlNotFoundFakeAfterMinutes)
							+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
							+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
						);
					}

					// already incremented in above in if (completedWithError)
                    // encodingStatusFailures++;

					_logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", liveURL: " + liveURL
						+ ", main: " + to_string(main)
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
            
			utcNowToCheckExit = chrono::system_clock::to_time_t(endEncoding);

			if (utcNowToCheckExit < utcRecordingPeriodEnd)
			{
				_logger->error(__FILEREF__ + "LiveRecorder media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", liveURL: " + liveURL
                    + ", main: " + to_string(main)
					+ ", still remaining seconds (utcRecordingPeriodEnd - utcNow): " + to_string(utcRecordingPeriodEnd - utcNowToCheckExit)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    + ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
                    + ", killedByUser: " + to_string(killedByUser)
                    + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
			}
			else
			{
				_logger->info(__FILEREF__ + "LiveRecorder media file completed"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", liveURL: " + liveURL
                    + ", main: " + to_string(main)
                    + ", autoRenew: " + to_string(autoRenew) 
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", killedByUser: " + to_string(killedByUser) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
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
					_mmsEngineDBFacade->updateIngestionAndEncodingLiveRecordingPeriod(
							_encodingItem->_ingestionJobKey,
							_encodingItem->_encodingJobKey,
							utcRecordingPeriodStart, utcRecordingPeriodEnd);

					// next update is important because the JSON is used in the getEncodingProgress method 
					{
						string field = "utcRecordingPeriodStart";
						_encodingItem->_encodingParametersRoot[field] = utcRecordingPeriodStart;

						field = "utcRecordingPeriodEnd";
						_encodingItem->_encodingParametersRoot[field] = utcRecordingPeriodEnd;
					}
				}
			}
		}
		catch(EncoderNotFound e)
		{
            string errorMessage = string("EncoderNotFound")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                + ", e.what(): " + e.what()
                ;
            _logger->error(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This encodingJob will be set as failed
            throw runtime_error(e.what());
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
				utcNowToCheckExit = chrono::system_clock::to_time_t(now);
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
				utcNowToCheckExit = chrono::system_clock::to_time_t(now);
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
				utcNowToCheckExit = chrono::system_clock::to_time_t(now);
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
				utcNowToCheckExit = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
	}

	// Ingestion/Encoding Status will be success if at least one Chunk was generated
	// otherwise it will be set as failed
	if (main)
	{
		if (urlForbidden)
		{
			string errorMessage = __FILEREF__ + "LiveRecorder: URL forbidden"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
			;
			_logger->error(errorMessage);

			throw FFMpegURLForbidden();
		}
		else if (urlNotFound)
		{
			string errorMessage = __FILEREF__ + "LiveRecorder: URL Not Found"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingParameters: " + _encodingItem->_encodingParameters
				+ ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
			;
			_logger->error(errorMessage);

			throw FFMpegURLNotFound();
		}
		else
		{
			long ingestionJobOutputsCount = _mmsEngineDBFacade->getIngestionJobOutputsCount(
				_encodingItem->_ingestionJobKey);
			if (ingestionJobOutputsCount <= 0)
			{
				string errorMessage = __FILEREF__ + "LiveRecorder: no chunks were generated"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingParameters: " + _encodingItem->_encodingParameters
					+ ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
					+ ", ingestionJobOutputsCount: " + to_string(ingestionJobOutputsCount)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
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
			main = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

			field = "highAvailability";
			highAvailability = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);
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

				// 2020-06-09: since it happened that this waiting was not enough and a MIK went as input
				// to a task and it was removed while the task was using it, it was added one more minute
				toleranceMinutes++;	

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

					mainAndBackupChunksManagementCompleted =
						_mmsEngineDBFacade->liveRecorderMainAndBackupChunksManagementCompleted(
						_encodingItem->_ingestionJobKey);
					endPoint = chrono::system_clock::now();
				}
				while(!mainAndBackupChunksManagementCompleted &&
					chrono::duration_cast<chrono::seconds>(endPoint - startPoint).count()
					< toleranceMinutes * 60);

				if (mainAndBackupChunksManagementCompleted)
				{
					// scenario:
					//	1. the manageMainAndBackupOfRunnungLiveRecordingHA method validated all the chunks
					//		and set validated = false and retentionInMinutes = 0 for the others chunks.
					//	2. the retention algorithm has still to process the retention and may be one content
					//		is not removed yet
					//	3. the LiveRecording->OnSuccess task starts, Validator gets all the IngestionJobOutput contents
					//		containing also the content having retentionInMinutes = 0
					//	4. the retention algorithm starts and remove the content
					//	5. the OnSuccess Task starts processing the content and, when it will process the removed content fails
					//
					//	To solve this issue we still wait the period of the retention in order to minimize the above issue
					//
					//	Another solution would be that the Validator gets only the contents having validated = true
					//	for the LiveRecorder output!!! This should be a bit difficult to be done, actually Validator
					//	calls MMSEngineDBFacade::getMediaItemDetailsByIngestionJobKey where there are no information
					//	about the source Task generating the content!!!

					int secondsToWaitTheRetentionAlgorithm = 60;

					_logger->info(__FILEREF__ + "Managing of main and backup chunks completed"
						+ ", still wait a bit the retention algorithm"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					);

					this_thread::sleep_for(chrono::milliseconds(secondsToWaitTheRetentionAlgorithm));
				}
				else
				{
					_logger->warn(__FILEREF__ + "Managing of main and backup chunks NOT completed"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					);
				}
			}

			// Status will be success if at least one Chunk was generated, otherwise it will be failed
			{
				string errorMessage;
				string processorMMS;
				MMSEngineDBFacade::IngestionStatus	newIngestionStatus
					= MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

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

bool EncoderVideoAudioProxy::liveProxy()
{

	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
		string field = "timePeriod";
		timePeriod = JSONUtils::asBool(_encodingItem->_liveProxyData->_ingestedParametersRoot, field, false);

		if (timePeriod)
		{
			string field = "utcProxyPeriodStart";
			utcProxyPeriodStart = JSONUtils::asInt64(_encodingItem->_liveProxyData->_ingestedParametersRoot, field, -1);

			field = "utcProxyPeriodEnd";
			utcProxyPeriodEnd = JSONUtils::asInt64(_encodingItem->_liveProxyData->_ingestedParametersRoot, field, -1);
		}
	}

	if (timePeriod)
	{
		time_t utcNow;                                                                                            

		{                                                                                                         
			chrono::system_clock::time_point now = chrono::system_clock::now();                                   
			utcNow = chrono::system_clock::to_time_t(now);                                                        
		}

		// MMS allocates a thread just 5 minutes before the beginning of the recording                            
		if (utcNow < utcProxyPeriodStart)                                                                     
		{                                                                                                         
			if (utcProxyPeriodStart - utcNow >= _timeBeforeToPrepareResourcesInMinutes * 60)                  
			{                                                                                                     
				_logger->info(__FILEREF__ + "Too early to allocate a thread for proxing"                        
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)                                        
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)                         
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)                           
					+ ", utcProyPeriodStart - utcNow: " + to_string(utcProxyPeriodStart - utcNow)        
					+ ", _timeBeforeToPrepareResourcesInSeconds: " + to_string(_timeBeforeToPrepareResourcesInMinutes * 60)
				);

				// it is simulated a MaxConcurrentJobsReached to avoid to increase the error counter              
				throw MaxConcurrentJobsReached();                                                                 
			}
		}

		if (utcProxyPeriodEnd <= utcNow)
		{
			string errorMessage = __FILEREF__ + "Too late to activate the proxy"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
				+ ", utcNow: " + to_string(utcNow)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	bool killedByUser = liveProxy_through_ffmpeg();
	if (killedByUser)
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->warn(errorMessage);
        
		throw EncodingKilledByUser();
	}
    
	return killedByUser;
}

bool EncoderVideoAudioProxy::liveProxy_through_ffmpeg()
{

	string channelType;
	string encodersPool;
	int64_t liveURLConfKey;
	string configurationLabel;
	string liveURL;
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	long maxAttemptsNumberInCaseOfErrors;
	string userAgent;
	int maxWidth = -1;
	string otherInputOptions;
	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
        string field = "ChannelType";
        channelType = _encodingItem->_liveProxyData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "EncodersPool";
        encodersPool = _encodingItem->_liveProxyData->
			_ingestedParametersRoot.get(field, "").asString();

        field = "liveURLConfKey";
        liveURLConfKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "ConfigurationLabel";
        // configurationLabel = _encodingItem->_encodingParametersRoot.get(field, "XXX").asString();
        configurationLabel = _encodingItem->_liveProxyData->_ingestedParametersRoot.get(field, "XXX").asString();

        field = "url";
        liveURL = _encodingItem->_encodingParametersRoot.get(field, "").asString();

        field = "UserAgent";
		if (JSONUtils::isMetadataPresent(_encodingItem->_liveProxyData->_ingestedParametersRoot, field))
			userAgent = _encodingItem->_liveProxyData->_ingestedParametersRoot.get(field, "").asString();

        field = "MaxWidth";
		if (JSONUtils::isMetadataPresent(_encodingItem->_liveProxyData->_ingestedParametersRoot, field))
			maxWidth = JSONUtils::asInt(_encodingItem->_liveProxyData->_ingestedParametersRoot, field, -1);

        field = "OtherInputOptions";
		if (JSONUtils::isMetadataPresent(_encodingItem->_liveProxyData->_ingestedParametersRoot, field))
			otherInputOptions = _encodingItem->_liveProxyData->_ingestedParametersRoot.get(field, "").asString();

        field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
        waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 600);

        field = "maxAttemptsNumberInCaseOfErrors";
        maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 2);

		field = "timePeriod";
		timePeriod = JSONUtils::asBool(_encodingItem->_encodingParametersRoot, field, false);

		if (timePeriod)
		{
			field = "utcProxyPeriodStart";
			utcProxyPeriodStart = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

			field = "utcProxyPeriodEnd";
			utcProxyPeriodEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);
		}
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	long currentAttemptsNumberInCaseOfErrors = 0;

	long encodingStatusFailures = 0;
	// 2020-04-19: Reset encodingStatusFailures into DB. That because if we comes from an error/exception
	//	encodingStatusFailures is > than 0 but we consider here like it is 0 because our variable is set to 0
	try
	{
		_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);

		_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
				_encodingItem->_encodingJobKey, 
				encodingStatusFailures
		);
	}
	catch(...)
	{
		_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);
	}

	// 2020-03-11: we saw the following scenarios:
	//	1. ffmpeg was running
	//	2. after several hours it failed (1:34 am)
	//	3. our below loop tried again and this new attempt returned 404 URL NOT FOUND
	//	4. we exit from this loop
	//	5. crontab started again it after 15 minutes
	//	In this scenarios, we have to retry again without waiting the crontab check
	// 2020-03-12: Removing the urlNotFound management generated duplication of ffmpeg process
	//	For this reason we rollbacked as it was before
	time_t utcNowCheckToExit = 0;
	while (!killedByUser && !urlForbidden && !urlNotFound
		// check on currentAttemptsNumberInCaseOfErrors is done only if there is no timePeriod
		&& (timePeriod || currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors)
	)
	{
		if (timePeriod)
		{
			if (utcNowCheckToExit >= utcProxyPeriodEnd)
				break;
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveProxyURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			if (_encodingItem->_encoderKey == -1)
			{
				/*
				string encoderToSkip;
				_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace, encoderToSkip);
				*/
				int64_t encoderKeyToBeSkipped = -1;
				pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
				tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

				_logger->info(__FILEREF__ + "Configuration item"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);
				// ffmpegEncoderURL = 
				// 	_ffmpegEncoderProtocol
				// 	+ "://"
				// 	+ _currentUsedFFMpegEncoderHost + ":"
				// 	+ to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
					+ ffmpegURI
					+ "/" + to_string(_encodingItem->_encodingJobKey)
				;

				_logger->info(__FILEREF__ + "LiveProxy. Selection of the transcoder. The transcoder is selected by load balancer"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", transcoder: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				string body;
				{
					// in case of youtube url, the real URL to be used has to be calcolated
					{
						string youTubePrefix1 ("https://www.youtube.com/");
						string youTubePrefix2 ("https://youtu.be/");
						if (
							(liveURL.size() >= youTubePrefix1.size()
								&& 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1))
							||
							(liveURL.size() >= youTubePrefix2.size()
								&& 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2))
							)
						{
							string streamingYouTubeLiveURL;
							long hoursFromLastCalculatedURL;
							pair<long,string> lastYouTubeURLDetails;
							try
							{
								lastYouTubeURLDetails = getLastYouTubeURLDetails(
									_encodingItem->_ingestionJobKey,
									_encodingItem->_encodingJobKey,
									_encodingItem->_workspace->_workspaceKey,
									liveURLConfKey);

								string lastCalculatedURL;

								tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

								_logger->info(__FILEREF__
									+ "LiveProxy. check youTubeURLCalculate"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", liveURLConfKey: " + to_string(liveURLConfKey)
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									+ ", _retrieveStreamingYouTubeURLPeriodInHours: " + to_string(_retrieveStreamingYouTubeURLPeriodInHours)
								);
								if (hoursFromLastCalculatedURL < _retrieveStreamingYouTubeURLPeriodInHours)
									streamingYouTubeLiveURL = lastCalculatedURL;
							}
							catch(runtime_error e)
							{
								string errorMessage = __FILEREF__
									+ "LiveProxy. youTubeURLCalculate. getLastYouTubeURLDetails failed"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", liveURLConfKey: " + to_string(liveURLConfKey)
									+ ", YouTube URL: " + streamingYouTubeLiveURL
								;
								_logger->error(errorMessage);
							}

							if (streamingYouTubeLiveURL == "")
							{
								// last calculated URL "expired" or not saved
								try
								{
									FFMpeg ffmpeg (_configuration, _logger);
									pair<string, string> streamingLiveURLDetails =
										ffmpeg.retrieveStreamingYouTubeURL(
										_encodingItem->_ingestionJobKey,
										_encodingItem->_encodingJobKey,
										liveURL);

									tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

									_logger->info(__FILEREF__ + "LiveProxy. youTubeURLCalculate. Retrieve streaming YouTube URL"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", liveURLConfKey: " + to_string(liveURLConfKey)
										+ ", initial YouTube URL: " + liveURL
										+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
										+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									);
								}
								catch(runtime_error e)
								{
									// in case ffmpeg.retrieveStreamingYouTubeURL fails
									// we will use the last saved URL
									tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

									string errorMessage = __FILEREF__
										+ "LiveProxy. youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", liveURLConfKey: " + to_string(liveURLConfKey)
										+ ", YouTube URL: " + streamingYouTubeLiveURL
									;
									_logger->error(errorMessage);

									try
									{
										_mmsEngineDBFacade->appendIngestionJobErrorMessage(
											_encodingItem->_ingestionJobKey, errorMessage);
									}
									catch(runtime_error e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
											+ ", e.what(): " + e.what()
										);
									}
									catch(exception e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
										);
									}

									if (streamingYouTubeLiveURL == "")
									{
										// 2020-04-21: let's go ahead because it would be managed
										// the killing of the encodingJob
										// 2020-09-17: it does not have sense to continue
										//	if we do not have the right URL (m3u8)
										throw YouTubeURLNotRetrieved();
									}
								}

								if (streamingYouTubeLiveURL != "")
								{
									try
									{
										updateChannelDataWithNewYouTubeURL(
											_encodingItem->_ingestionJobKey,
											_encodingItem->_encodingJobKey,
											_encodingItem->_workspace->_workspaceKey,
											liveURLConfKey,
											streamingYouTubeLiveURL);
									}
									catch(runtime_error e)
									{
										string errorMessage = __FILEREF__
											+ "LiveProxy. youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
											+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
											+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
											+ ", liveURLConfKey: " + to_string(liveURLConfKey)
											+ ", YouTube URL: " + streamingYouTubeLiveURL
										;
										_logger->error(errorMessage);
									}
								}
							}
							else
							{
								_logger->info(__FILEREF__ + "LiveProxy. youTubeURLCalculate. Reuse a previous streaming YouTube URL"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", liveURLConfKey: " + to_string(liveURLConfKey)
									+ ", initial YouTube URL: " + liveURL
									+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
								);
							}

							liveURL = streamingYouTubeLiveURL;
						}
					}

					Json::Value liveProxyMetadata;

					liveProxyMetadata["ingestionJobKey"] =
						(Json::LargestUInt) (_encodingItem->_ingestionJobKey);
					liveProxyMetadata["liveURL"] = liveURL;
					liveProxyMetadata["timePeriod"] = timePeriod;
					liveProxyMetadata["utcProxyPeriodStart"] = utcProxyPeriodStart;
					liveProxyMetadata["utcProxyPeriodEnd"] = utcProxyPeriodEnd;
					liveProxyMetadata["userAgent"] = userAgent;
					liveProxyMetadata["maxWidth"] = maxWidth;
					liveProxyMetadata["otherInputOptions"] = otherInputOptions;
					liveProxyMetadata["outputsRoot"] = _encodingItem->_liveProxyData->_outputsRoot;
					liveProxyMetadata["configurationLabel"] = configurationLabel;
					liveProxyMetadata["liveProxyIngestedParametersRoot"] = _encodingItem->_liveProxyData->_ingestedParametersRoot;
					{
						Json::StreamWriterBuilder wbuilder;

						body = Json::writeString(wbuilder, liveProxyMetadata);
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

				// timeout consistent with nginx configuration (fastcgi_read_timeout)
				request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

				// if (_ffmpegEncoderProtocol == "https")
				string httpsPrefix("https");
				if (ffmpegEncoderURL.size() >= httpsPrefix.size()
					&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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

				_logger->info(__FILEREF__ + "Calling transcoder for LiveProxy media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
				);
				responseInitialized = true;
				request.perform();

				string sResponse = response.str();
				// LF and CR create problems to the json parser...
				while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
					sResponse.pop_back();

				Json::Value liveProxyContentResponse;
				try
				{
					Json::CharReaderBuilder builder;
					Json::CharReader* reader = builder.newCharReader();
					string errors;

					bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &liveProxyContentResponse, &errors);
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
					if (JSONUtils::isMetadataPresent(liveProxyContentResponse, field))
					{
						string error = liveProxyContentResponse.get(field, "XXX").asString();
                    
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
				_logger->info(__FILEREF__ + "LiveProxy. Selection of the transcoder. The transcoder is already saved (DB), the encoding should be already running"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				);

				_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
				// manifestFilePathName = _encodingItem->_stagingEncodedAssetPathName;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
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
				+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(
				_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			/*
			string manifestDirectoryPathName;
			if (outputType == "HLS" || outputType == "DASH")
			{
				size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
				if (manifestFilePathIndex == string::npos)
				{
					string errorMessage = __FILEREF__ + "No manifestDirectoryPath find in the m3u8/mpd file path name"
							+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", manifestFilePathName: " + manifestFilePathName;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
			}
			*/

			if (timePeriod)
				;
			else
			{
				// encodingProgress: fixed to -1 (LIVE)

				try
				{
					int encodingProgress = -1;

					_logger->info(__FILEREF__ + "updateEncodingJobProgress"
						+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingProgress: " + to_string(encodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress (
						_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			// string lastRecordedAssetFileName;
			chrono::system_clock::time_point startCheckingEncodingStatus = chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int encodingPid;
			int lastEncodingPid = 0;

			// 2020-11-28: the next while, it was added encodingStatusFailures condition because,
			//  in case the transcoder is down (once I had to upgrade his operative system),
			//  the engine has to select another encoder and not remain in the next loop indefinitely
            while(!(encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures))
            // while(!encodingFinished)
            {
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
						getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, ignore, encodingPid) = encodingStatus;
					_logger->info(__FILEREF__ + "getEncodingStatus"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", completedWithError: " + to_string(completedWithError)
						+ ", urlForbidden: " + to_string(urlForbidden)
						+ ", urlNotFound: " + to_string(urlNotFound)
					);

					encoderNotReachableFailures = 0;

					// health check and retention is done by ffmpegEncoder.cpp
					
					if (encodingErrorMessage != "")
					{
						try
						{
							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, encodingErrorMessage);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError) // || chunksWereNotGenerated)
					{
						if (urlForbidden || urlNotFound)	// see my comment at the beginning of the while loop
						{
							string errorMessage =
								__FILEREF__ + "Encoding failed because of URL Forbidden or Not Found (look the Transcoder logs)"             
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingErrorMessage: " + encodingErrorMessage
								;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						currentAttemptsNumberInCaseOfErrors++;

						string errorMessage = __FILEREF__ + "Encoding failed"             
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", configurationLabel: " + configurationLabel
							+ ", encodingErrorMessage: " + encodingErrorMessage
							// + ", chunksWereNotGenerated: " + to_string(chunksWereNotGenerated)
						;
						_logger->error(errorMessage);

						encodingStatusFailures++;

						// in this scenario encodingFinished is true

						_logger->info(__FILEREF__ + "Start waiting loop for the next call"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);

						chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
						chrono::system_clock::time_point now;
						do
						{
							// 2021-02-12: moved sleep here because, in this case, if the task was killed
							// during the sleep, it will check that.
							// Before the sleep was after the check, so when the sleep is finished,
							// the flow will go out of the loop and no check is done and Task remains up
							// even if user kiiled it.
							this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

							// update EncodingJob failures number to notify the GUI EncodingJob is failing
							try
							{
								_logger->info(__FILEREF__ + "check and update encodingJob FailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								long previousEncodingStatusFailures =
									_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures);
								if (previousEncodingStatusFailures < 0)
								{
									_logger->info(__FILEREF__ + "LiveProxy Killed by user during waiting loop"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
									);

									// when previousEncodingStatusFailures is < 0 means:
									// 1. the live proxy is not starting (ffmpeg is generating continuously an error)
									// 2. User killed the encoding through MMS GUI or API
									// 3. the kill procedure (in API module) was not able to kill the ffmpeg process,
									//		because it does not exist the process and set the failuresNumber DB field
									//		to a negative value in order to communicate with this thread 
									// 4. This thread, when it finds a negative failuresNumber, knows the encoding
									//		was killed and exit from the loop
									encodingFinished = true;
									killedByUser = true;
								}
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}

							now = chrono::system_clock::now();
						}
						while (chrono::duration_cast<chrono::seconds>(now - startWaiting)
								< chrono::seconds(waitingSecondsBetweenAttemptsInCaseOfErrors)
						 		&& (timePeriod || currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors)
								&& !killedByUser);

						// if (chunksWereNotGenerated)
						// 	encodingFinished = true;

						throw runtime_error(errorMessage);
					}
					else
					{
						// ffmpeg is running successful, we will make sure currentAttemptsNumberInCaseOfErrors is reset
						currentAttemptsNumberInCaseOfErrors = 0;

						if (encodingStatusFailures > 0)
						{
							try
							{
								// update EncodingJob failures number to notify the GUI encodingJob is successful
								encodingStatusFailures = 0;

								_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures
								);
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}
						}
					}

					// encodingProgress/encodingPid
					{
						if (timePeriod)
						{
							time_t utcNow;                                                                    

							{                                                                                 
								chrono::system_clock::time_point now = chrono::system_clock::now();           
								utcNow = chrono::system_clock::to_time_t(now);                                
							}                                                                                 
                                                                                                              
							int encodingProgress;                                                             
                                                                                                              
							if (utcNow < utcProxyPeriodStart)                                             
								encodingProgress = 0;                                                         
							else if (utcProxyPeriodStart < utcNow && utcNow < utcProxyPeriodEnd)      
								encodingProgress = ((utcNow - utcProxyPeriodStart) * 100) /               
									(utcProxyPeriodEnd - utcProxyPeriodStart);                        
							else                                                                              
								encodingProgress = 100;

							try
							{
								_logger->info(__FILEREF__ + "updateEncodingJobProgress"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
								);
								_mmsEngineDBFacade->updateEncodingJobProgress (
									_encodingItem->_encodingJobKey, encodingProgress);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
								);
							}
						}

						if (lastEncodingPid != encodingPid)
						{
							try
							{
								_logger->info(__FILEREF__ + "updateEncodingPid"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingPid: " + to_string(encodingPid)
								);
								_mmsEngineDBFacade->updateEncodingPid (
									_encodingItem->_encodingJobKey, encodingPid);

								lastEncodingPid = encodingPid;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
								);
							}
						}
					}

					/*
					if (outputType == "HLS" || outputType == "DASH")
					{
						bool exceptionInCaseOfError = false;

						for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
						{
							try
							{
								_logger->info(__FILEREF__ + "Remove chunk because too old"
									+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
								FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "remove failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
									+ ", e.what(): " + e.what()
								);
							}
						}
					}
					*/
                }
				catch(EncoderNotReachable e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS version
					//	2. this thread remained in this loop (while(!encodingFinished))
					//		and the channel did not work until the Encoder was working again
					//	In this scenario, so when the encoder is not reachable at all, the engine
					//	has to select a new encoder.
					//	For this reason we added this EncoderNotReachable catch
					//	and the encoderNotReachableFailures variable

					_logger->error(__FILEREF__ + "Transcoder is not reachable at all, let's select another encoder"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", encoderNotReachableFailures: " + to_string(encoderNotReachableFailures)
						+ ", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures)
						+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
				}
                catch(...)
                {
					_logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			if (timePeriod)
			{
				if (utcNowCheckToExit < utcProxyPeriodEnd)
				{
					_logger->error(__FILEREF__ + "LiveProxy media file completed unexpected"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", still remaining seconds (utcProxyPeriodEnd - utcNow): " + to_string(utcProxyPeriodEnd - utcNowCheckToExit)
						+ ", configurationLabel: " + configurationLabel
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);
				}
				else
				{
					_logger->info(__FILEREF__ + "LiveProxy media file completed"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", configurationLabel: " + configurationLabel
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);
				}
			}
			else
			{
				_logger->error(__FILEREF__ + "LiveProxy media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", configurationLabel: " + configurationLabel
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    + ", killedByUser: " + to_string(killedByUser)
                    + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
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
		catch(YouTubeURLNotRetrieved e)
		{
            string errorMessage = string("YouTubeURLNotRetrieved")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
                + ", e.what(): " + e.what()
                ;
            _logger->error(__FILEREF__ + errorMessage);

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
            
			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed/runtime_error"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				// 2021-02-12: scenario, encodersPool does not exist, a runtime_error is generated
				// contiuosly. The task will never exist from this loop because
				// currentAttemptsNumberInCaseOfErrors always remain to 0 and the main loop
				// look currentAttemptsNumberInCaseOfErrors.
				// So added currentAttemptsNumberInCaseOfErrors++
				currentAttemptsNumberInCaseOfErrors++;

				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
	}

	if (urlForbidden)
	{
		string errorMessage = __FILEREF__ + "LiveProxy: URL forbidden"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
			+ ", _encodingParameters: " + _encodingItem->_encodingParameters
            ;
		_logger->error(errorMessage);
        
		throw FFMpegURLForbidden();
	}
	else if (urlNotFound)
	{
		string errorMessage = __FILEREF__ + "LiveProxy: URL Not Found"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
			+ ", _encodingParameters: " + _encodingItem->_encodingParameters
            ;
		_logger->error(errorMessage);
        
		throw FFMpegURLNotFound();
	}
	else if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		string errorMessage = __FILEREF__ + "Reached the max number of attempts to the URL"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) 
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors) 
			+ ", configurationLabel: " + configurationLabel
            ;
		_logger->error(errorMessage);
        
		throw EncoderError();
	}

    return killedByUser;
}

void EncoderVideoAudioProxy::processLiveProxy(bool killedByUser)
{
    try
    {
		// In case of Liveproxy where TimePeriod is false, this method is never called because,
		//	in both the scenarios below, an exception by EncoderVideoAudioProxy::liveProxy will be raised:
		// - transcoding killed by the user 
		// - The max number of calls to the URL were all done and all failed
		//
		// In case of LiveProxy where TimePeriod is true, this method is called

		// Status will be success if at least one Chunk was generated, otherwise it will be failed
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus	newIngestionStatus
				= MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

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
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processLiveProxy failed"
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
        _logger->error(__FILEREF__ + "processLiveProxy failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

bool EncoderVideoAudioProxy::awaitingTheBeginning()
{

	time_t utcCountDownEnd = -1;
	{
		string field = "utcCountDownEnd";
		utcCountDownEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);
	}

	{
		time_t utcNow;                                                                                            

		{                                                                                                         
			chrono::system_clock::time_point now = chrono::system_clock::now();                                   
			utcNow = chrono::system_clock::to_time_t(now);                                                        
		}

		if (utcCountDownEnd <= utcNow)
		{
			string errorMessage = __FILEREF__ + "Too late to activate awaitingTheBeginning"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", utcCountDownEnd: " + to_string(utcCountDownEnd)
				+ ", utcNow: " + to_string(utcNow)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	bool killedByUser = awaitingTheBeginning_through_ffmpeg();
	if (killedByUser)
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->warn(errorMessage);
        
		throw EncodingKilledByUser();
	}
    
	return killedByUser;
}

bool EncoderVideoAudioProxy::awaitingTheBeginning_through_ffmpeg()
{

	string encodersPool;
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	long maxAttemptsNumberInCaseOfErrors;
	time_t utcIngestionJobStartProcessing = -1;
	time_t utcCountDownEnd = -1;
	{
        string field = "EncodersPool";
        encodersPool = _encodingItem->_awaitingTheBeginningData->_ingestedParametersRoot.get(field, "").asString();

        field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
        waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 600);

        field = "maxAttemptsNumberInCaseOfErrors";
        maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 2);

		field = "utcIngestionJobStartProcessing";
		utcIngestionJobStartProcessing = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "utcCountDownEnd";
		utcCountDownEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	long currentAttemptsNumberInCaseOfErrors = 0;

	long encodingStatusFailures = 0;
	// 2020-04-19: Reset encodingStatusFailures into DB. That because if we comes from an error/exception
	//	encodingStatusFailures is > than 0 but we consider here like it is 0 because our variable is set to 0
	try
	{
		_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);

		_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
				_encodingItem->_encodingJobKey, 
				encodingStatusFailures
		);
	}
	catch(...)
	{
		_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);
	}

	// 2020-03-11: we saw the following scenarios:
	//	1. ffmpeg was running
	//	2. after several hours it failed (1:34 am)
	//	3. our below loop tried again and this new attempt returned 404 URL NOT FOUND
	//	4. we exit from this loop
	//	5. crontab started again it after 15 minutes
	//	In this scenarios, we have to retry again without waiting the crontab check
	// 2020-03-12: Removing the urlNotFound management generated duplication of ffmpeg process
	//	For this reason we rollbacked as it was before
	time_t utcNowCheckToExit = 0;
	while (!killedByUser && !urlForbidden && !urlNotFound
		&& currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors
	)
	{
		if (utcNowCheckToExit >= utcCountDownEnd)
			break;

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegAwaitingTheBeginningURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			if (_encodingItem->_encoderKey == -1)
			{
				/*
				string encoderToSkip;
				_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace, encoderToSkip);
				*/
				int64_t encoderKeyToBeSkipped = -1;
				pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
				tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

				_logger->info(__FILEREF__ + "Configuration item"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);
				// ffmpegEncoderURL = 
				// 	_ffmpegEncoderProtocol
				// 	+ "://"
				// 	+ _currentUsedFFMpegEncoderHost + ":"
				// 	+ to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
					+ ffmpegURI
					+ "/" + to_string(_encodingItem->_encodingJobKey)
				;

				_logger->info(__FILEREF__ + "AwaitingTheBeginning. Selection of the transcoder. The transcoder is selected by load balancer"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", transcoder: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				string body;
				{
					Json::Value awaitingTheBeginningMetadata;

					awaitingTheBeginningMetadata["ingestionJobKey"] =
						(Json::LargestUInt) (_encodingItem->_ingestionJobKey);

					awaitingTheBeginningMetadata["mmsSourceVideoAssetPathName"] =
						_encodingItem->_encodingParametersRoot.get("mmsSourceVideoAssetPathName", "").asString();
					awaitingTheBeginningMetadata["videoDurationInMilliSeconds"] =
						JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, "videoDurationInMilliSeconds", -1);

					awaitingTheBeginningMetadata["outputType"] =
						_encodingItem->_encodingParametersRoot.get("outputType", "").asString();
					awaitingTheBeginningMetadata["encodingProfileDetails"] =
						_encodingItem->_awaitingTheBeginningData->_encodingProfileDetailsRoot;
					awaitingTheBeginningMetadata["manifestDirectoryPath"] =
						_encodingItem->_encodingParametersRoot.get("manifestDirectoryPath", "").asString();
					awaitingTheBeginningMetadata["manifestFileName"] =
						_encodingItem->_encodingParametersRoot.get("manifestFileName", "").asString();
					awaitingTheBeginningMetadata["segmentDurationInSeconds"] =
						JSONUtils::asInt(_encodingItem->_encodingParametersRoot, "segmentDurationInSeconds", -1);
					awaitingTheBeginningMetadata["playlistEntriesNumber"] =
						JSONUtils::asInt(_encodingItem->_encodingParametersRoot, "playlistEntriesNumber", -1);
					awaitingTheBeginningMetadata["encodingProfileContentType"] =
						MMSEngineDBFacade::toString(_encodingItem->_awaitingTheBeginningData->_encodingProfileContentType);
					awaitingTheBeginningMetadata["rtmpUrl"] =
						_encodingItem->_encodingParametersRoot.get("rtmpUrl", "").asString();

					awaitingTheBeginningMetadata["utcCountDownEnd"] = utcCountDownEnd;
					awaitingTheBeginningMetadata["awaitingTheBeginningIngestedParametersRoot"] =
						_encodingItem->_awaitingTheBeginningData->_ingestedParametersRoot;

					{
						Json::StreamWriterBuilder wbuilder;

						body = Json::writeString(wbuilder, awaitingTheBeginningMetadata);
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

				// timeout consistent with nginx configuration (fastcgi_read_timeout)
				request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

				// if (_ffmpegEncoderProtocol == "https")
				string httpsPrefix("https");
				if (ffmpegEncoderURL.size() >= httpsPrefix.size()
					&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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

				_logger->info(__FILEREF__ + "Calling transcoder for AwaitingTheBeginning media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
				);
				responseInitialized = true;
				request.perform();

				string sResponse = response.str();
				// LF and CR create problems to the json parser...
				while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
					sResponse.pop_back();

				Json::Value awaitingTheBeginningContentResponse;
				try
				{
					Json::CharReaderBuilder builder;
					Json::CharReader* reader = builder.newCharReader();
					string errors;

					bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &awaitingTheBeginningContentResponse, &errors);
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
					if (JSONUtils::isMetadataPresent(awaitingTheBeginningContentResponse, field))
					{
						string error = awaitingTheBeginningContentResponse.get(field, "XXX").asString();
                    
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
				_logger->info(__FILEREF__ + "awaitingTheBeginning. Selection of the transcoder. The transcoder is already saved (DB), the encoding should be already running"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				);

				_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
				// manifestFilePathName = _encodingItem->_stagingEncodedAssetPathName;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
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
				+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(
				_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			/*
			string manifestDirectoryPathName;
			if (outputType == "HLS" || outputType == "DASH")
			{
				size_t manifestFilePathIndex = manifestFilePathName.find_last_of("/");
				if (manifestFilePathIndex == string::npos)
				{
					string errorMessage = __FILEREF__ + "No manifestDirectoryPath find in the m3u8/mpd file path name"
							+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", manifestFilePathName: " + manifestFilePathName;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}
				manifestDirectoryPathName = manifestFilePathName.substr(0, manifestFilePathIndex);
			}
			*/

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			// string lastRecordedAssetFileName;
			chrono::system_clock::time_point startCheckingEncodingStatus = chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int encodingPid;
			int lastEncodingPid = 0;

			// 2020-11-28: the next while, it was added encodingStatusFailures condition because,
			//  in case the transcoder is down (once I had to upgrade his operative system),
			//  the engine has to select another encoder and not remain in the next loop indefinitely
            while(!(encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures))
            // while(!encodingFinished)
            {
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
						getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, ignore, encodingPid) = encodingStatus;
					_logger->info(__FILEREF__ + "getEncodingStatus"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", completedWithError: " + to_string(completedWithError)
						+ ", urlForbidden: " + to_string(urlForbidden)
						+ ", urlNotFound: " + to_string(urlNotFound)
					);

					encoderNotReachableFailures = 0;

					// health check and retention is done by ffmpegEncoder.cpp
					
					if (encodingErrorMessage != "")
					{
						try
						{
							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, encodingErrorMessage);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError) // || chunksWereNotGenerated)
					{
						if (urlForbidden || urlNotFound)	// see my comment at the beginning of the while loop
						{
							string errorMessage =
								__FILEREF__ + "Encoding failed because of URL Forbidden or Not Found (look the Transcoder logs)"             
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingErrorMessage: " + encodingErrorMessage
								;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						currentAttemptsNumberInCaseOfErrors++;

						string errorMessage = __FILEREF__ + "Encoding failed"             
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingErrorMessage: " + encodingErrorMessage
							// + ", chunksWereNotGenerated: " + to_string(chunksWereNotGenerated)
						;
						_logger->error(errorMessage);

						encodingStatusFailures++;

						// in this scenario encodingFinished is true

						_logger->info(__FILEREF__ + "Start waiting loop for the next call"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);

						chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
						chrono::system_clock::time_point now;
						do
						{
							// update EncodingJob failures number to notify the GUI EncodingJob is failing
							try
							{
								_logger->info(__FILEREF__ + "check and update encodingJob FailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								long previousEncodingStatusFailures =
									_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures);
								if (previousEncodingStatusFailures < 0)
								{
									_logger->info(__FILEREF__ + "AwaitingTheBeginning Killed by user during waiting loop"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
									);

									// when previousEncodingStatusFailures is < 0 means:
									// 1. the live proxy is not starting (ffmpeg is generating continuously an error)
									// 2. User killed the encoding through MMS GUI or API
									// 3. the kill procedure (in API module) was not able to kill the ffmpeg process,
									//		because it does not exist the process and set the failuresNumber DB field
									//		to a negative value in order to communicate with this thread 
									// 4. This thread, when it finds a negative failuresNumber, knows the encoding
									//		was killed and exit from the loop
									encodingFinished = true;
									killedByUser = true;
								}
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}

							this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

							now = chrono::system_clock::now();
						}
						while (chrono::duration_cast<chrono::seconds>(now - startWaiting)
								< chrono::seconds(waitingSecondsBetweenAttemptsInCaseOfErrors)
						 		&& currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors
								&& !killedByUser);

						// if (chunksWereNotGenerated)
						// 	encodingFinished = true;

						throw runtime_error(errorMessage);
					}
					else
					{
						// ffmpeg is running successful, we will make sure currentAttemptsNumberInCaseOfErrors is reset
						currentAttemptsNumberInCaseOfErrors = 0;

						if (encodingStatusFailures > 0)
						{
							try
							{
								// update EncodingJob failures number to notify the GUI encodingJob is successful
								encodingStatusFailures = 0;

								_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures
								);
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}
						}
					}

					// encodingProgress/encodingPid
					{
						{
							time_t utcNow;                                                                    

							{                                                                                 
								chrono::system_clock::time_point now = chrono::system_clock::now();           
								utcNow = chrono::system_clock::to_time_t(now);                                
							}                                                                                 
                                                                                                              
							int encodingProgress;                                                             
                                                                                                              
							if (utcNow < utcCountDownEnd)      
								encodingProgress = ((utcNow - utcIngestionJobStartProcessing) * 100) /               
									(utcCountDownEnd - utcIngestionJobStartProcessing);                        
							else                                                                              
								encodingProgress = 100;

							try
							{
								_logger->info(__FILEREF__ + "updateEncodingJobProgress"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
								);
								_mmsEngineDBFacade->updateEncodingJobProgress (
									_encodingItem->_encodingJobKey, encodingProgress);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
								);
							}
						}

						if (lastEncodingPid != encodingPid)
						{
							try
							{
								_logger->info(__FILEREF__ + "updateEncodingPid"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingPid: " + to_string(encodingPid)
								);
								_mmsEngineDBFacade->updateEncodingPid (
									_encodingItem->_encodingJobKey, encodingPid);

								lastEncodingPid = encodingPid;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
								);
							}
						}
					}

					/*
					if (outputType == "HLS" || outputType == "DASH")
					{
						bool exceptionInCaseOfError = false;

						for (string segmentPathNameToBeRemoved: chunksTooOldToBeRemoved)
						{
							try
							{
								_logger->info(__FILEREF__ + "Remove chunk because too old"
									+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved);
								FileIO::remove(segmentPathNameToBeRemoved, exceptionInCaseOfError);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "remove failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", segmentPathNameToBeRemoved: " + segmentPathNameToBeRemoved
									+ ", e.what(): " + e.what()
								);
							}
						}
					}
					*/
                }
				catch(EncoderNotReachable e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS version
					//	2. this thread remained in this loop (while(!encodingFinished))
					//		and the channel did not work until the Encoder was working again
					//	In this scenario, so when the encoder is not reachable at all, the engine
					//	has to select a new encoder.
					//	For this reason we added this EncoderNotReachable catch
					//	and the encoderNotReachableFailures variable

					_logger->error(__FILEREF__ + "Transcoder is not reachable at all, let's select another encoder"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encoderNotReachableFailures: " + to_string(encoderNotReachableFailures)
						+ ", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures)
						+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
				}
                catch(...)
                {
					_logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			{
				if (utcNowCheckToExit < utcCountDownEnd)
				{
					_logger->error(__FILEREF__ + "AwaitingTheBeginning media file completed unexpected"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", still remaining seconds (utcAwaitingTheBeginningEnd - utcNow): "
							+ to_string(utcCountDownEnd - utcNowCheckToExit)
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);
				}
				else
				{
					_logger->info(__FILEREF__ + "AwaitingTheBeginning media file completed"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
					);
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
            
			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed/runtime_error"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			{
				chrono::system_clock::time_point now = chrono::system_clock::now();
				utcNowCheckToExit = chrono::system_clock::to_time_t(now);
			}

            // throw e;
        }
	}

	if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		string errorMessage = __FILEREF__ + "Reached the max number of attempts to the URL"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) 
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors) 
            ;
		_logger->error(errorMessage);
        
		throw EncoderError();
	}

    return killedByUser;
}

void EncoderVideoAudioProxy::processAwaitingTheBeginning(bool killedByUser)
{
    try
    {
		{
			string errorMessage;
			string processorMMS;
			MMSEngineDBFacade::IngestionStatus	newIngestionStatus
				= MMSEngineDBFacade::IngestionStatus::End_TaskSuccess;

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
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processAwaitingTheBeginning failed"
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
        _logger->error(__FILEREF__ + "processAwaitingTheBeginning failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

pair<long,string> EncoderVideoAudioProxy::getLastYouTubeURLDetails(
	int64_t ingestionKey,
	int64_t encodingJobKey,
	int64_t workspaceKey,
	int64_t liveURLConfKey
)
{
	long hoursFromLastCalculatedURL = -1;
	string lastCalculatedURL;

	try
	{
		tuple<string, string, string> channelDetails =
			_mmsEngineDBFacade->getIPChannelConfDetails(
			_encodingItem->_workspace->_workspaceKey,
			liveURLConfKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot;
		try
		{
			Json::CharReaderBuilder builder;
			Json::CharReader* reader = builder.newCharReader();
			string errors;

			bool parsingSuccessful = reader->parse(channelData.c_str(),
				channelData.c_str() + channelData.size(),
				&channelDataRoot, &errors);
			delete reader;

			if (!parsingSuccessful)
			{
				string errorMessage = __FILEREF__ + "failed to parse channelData"
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", channelData: " + channelData
					+ ", errors: " + errors
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch(...)
		{
			string errorMessage = string("channelData json is not well format")
				+ ", ingestionKey: " + to_string(ingestionKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", channelData: " + channelData
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}


		string field;

		Json::Value mmsDataRoot;
		{
			field = "mmsData";
			if (!JSONUtils::isMetadataPresent(channelDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no mmsData present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", liveURLConfKey: " + to_string(liveURLConfKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			mmsDataRoot = channelDataRoot[field];
		}

		Json::Value youTubeURLsRoot(Json::arrayValue);
		{
			field = "youTubeURLs";
			if (!JSONUtils::isMetadataPresent(mmsDataRoot, field))
			{
				_logger->info(__FILEREF__ + "no youTubeURLs present"                
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", workspaceKey: " + to_string(workspaceKey)
					+ ", liveURLConfKey: " + to_string(liveURLConfKey)
				);

				return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
			}

			youTubeURLsRoot = mmsDataRoot[field];
		}

		if (youTubeURLsRoot.size() == 0)
		{
			_logger->info(__FILEREF__ + "no youTubeURL present"                
				+ ", ingestionKey: " + to_string(ingestionKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", workspaceKey: " + to_string(workspaceKey)
				+ ", liveURLConfKey: " + to_string(liveURLConfKey)
			);

			return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
		}

		{
			Json::Value youTubeLiveURLRoot = youTubeURLsRoot[youTubeURLsRoot.size() - 1];

			time_t tNow;
			{
				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmNow;

				localtime_r (&utcNow, &tmNow);
				tNow = mktime(&tmNow);
			}

			time_t tLastCalculatedURLTime;
			{
				unsigned long       ulYear;
				unsigned long		ulMonth;
				unsigned long		ulDay;
				unsigned long		ulHour;
				unsigned long		ulMinutes;
				unsigned long		ulSeconds;
				int					sscanfReturn;

				field = "timestamp";
				string timestamp = youTubeLiveURLRoot.get(field, "").asString();

				if ((sscanfReturn = sscanf (timestamp.c_str(),
					"%4lu-%2lu-%2lu %2lu:%2lu:%2lu",
					&ulYear,
					&ulMonth,
					&ulDay,
					&ulHour,
					&ulMinutes,
					&ulSeconds)) != 6)
				{
					string errorMessage = __FILEREF__ + "timestamp has a wrong format (sscanf failed)"                
						+ ", ingestionKey: " + to_string(ingestionKey)
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", workspaceKey: " + to_string(workspaceKey)
						+ ", liveURLConfKey: " + to_string(liveURLConfKey)
						+ ", sscanfReturn: " + to_string(sscanfReturn)
					;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}

				time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());
				tm tmLastCalculatedURL;

				localtime_r (&utcNow, &tmLastCalculatedURL);

				tmLastCalculatedURL.tm_year	= ulYear - 1900;
				tmLastCalculatedURL.tm_mon	= ulMonth - 1;
				tmLastCalculatedURL.tm_mday	= ulDay;
				tmLastCalculatedURL.tm_hour	= ulHour;
				tmLastCalculatedURL.tm_min	= ulMinutes;
				tmLastCalculatedURL.tm_sec	= ulSeconds;

				tLastCalculatedURLTime = mktime(&tmLastCalculatedURL);
			}

			hoursFromLastCalculatedURL = (tNow - tLastCalculatedURLTime) / 3600;

			field = "youTubeURL";
			lastCalculatedURL = youTubeLiveURLRoot.get(field, "").asString();
		}

		return make_pair(hoursFromLastCalculatedURL, lastCalculatedURL);
	}
	catch(...)
	{
		string errorMessage = string("getLastYouTubeURLDetails failed")
			+ ", ingestionKey: " + to_string(ingestionKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", liveURLConfKey: " + to_string(liveURLConfKey)
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

void EncoderVideoAudioProxy::updateChannelDataWithNewYouTubeURL(
	int64_t ingestionKey,
	int64_t encodingJobKey,
	int64_t workspaceKey,
	int64_t liveURLConfKey,
	string streamingYouTubeLiveURL
)
{
	try
	{
		tuple<string, string, string> channelDetails =
			_mmsEngineDBFacade->getIPChannelConfDetails(
			_encodingItem->_workspace->_workspaceKey,
			liveURLConfKey);

		string channelData;

		tie(ignore, ignore, channelData) = channelDetails;

		Json::Value channelDataRoot;
		try
		{
			Json::CharReaderBuilder builder;
			Json::CharReader* reader = builder.newCharReader();
			string errors;

			bool parsingSuccessful = reader->parse(channelData.c_str(),
				channelData.c_str() + channelData.size(),
				&channelDataRoot, &errors);
			delete reader;

			if (!parsingSuccessful)
			{
				string errorMessage = __FILEREF__ + "failed to parse channelData"
					+ ", ingestionKey: " + to_string(ingestionKey)
					+ ", encodingJobKey: " + to_string(encodingJobKey)
					+ ", channelData: " + channelData
					+ ", errors: " + errors
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch(...)
		{
			string errorMessage = string("channelData json is not well format")
				+ ", ingestionKey: " + to_string(ingestionKey)
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", channelData: " + channelData
			;
			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}

		// add streamingYouTubeLiveURL info to the channelData
		{
			string field;

			Json::Value youTubeLiveURLRoot;
			{
				char strNow[64];
				{
					time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

					tm tmNow;

					localtime_r (&utcNow, &tmNow);
					sprintf (strNow, "%04d-%02d-%02d %02d:%02d:%02d",
						tmNow. tm_year + 1900,
						tmNow. tm_mon + 1,
						tmNow. tm_mday,
						tmNow. tm_hour,
						tmNow. tm_min,
						tmNow. tm_sec);
				}
				field = "timestamp";
				youTubeLiveURLRoot[field] = strNow;

				field = "youTubeURL";
				youTubeLiveURLRoot[field] = streamingYouTubeLiveURL;
			}

			Json::Value mmsDataRoot;
			{
				field = "mmsData";
				if (JSONUtils::isMetadataPresent(channelDataRoot, field))
					mmsDataRoot = channelDataRoot[field];
			}

			Json::Value previousYouTubeURLsRoot(Json::arrayValue);
			{
				field = "youTubeURLs";
				if (JSONUtils::isMetadataPresent(mmsDataRoot, field))
					previousYouTubeURLsRoot = mmsDataRoot[field];
			}

			Json::Value youTubeURLsRoot(Json::arrayValue);

			// maintain the last 10 URLs
			int youTubeURLIndex;
			if (previousYouTubeURLsRoot.size() > 10)
				youTubeURLIndex = 10;
			else
				youTubeURLIndex = previousYouTubeURLsRoot.size();
			for (; youTubeURLIndex >= 0; youTubeURLIndex--)
				youTubeURLsRoot.append(previousYouTubeURLsRoot[youTubeURLIndex]);
			youTubeURLsRoot.append(youTubeLiveURLRoot);

			field = "youTubeURLs";
			mmsDataRoot[field] = youTubeURLsRoot;

			field = "mmsData";
			channelDataRoot[field] = mmsDataRoot;
		}

		bool labelToBeModified = false;
		string label;
		bool urlToBeModified = false;
		string url;
		bool typeToBeModified = false;
		string type;
		bool descriptionToBeModified = false;
		string description;
		bool nameToBeModified = false;
		string name;
		bool regionToBeModified = false;
		string region;
		bool countryToBeModified = false;
		string country;
		bool imageToBeModified = false;
		int64_t imageMediaItemKey = -1;
		string imageUniqueName;
		bool positionToBeModified = false;
		int position = -1;
		bool channelDataToBeModified = true;

		_mmsEngineDBFacade->modifyIPChannelConf(
			liveURLConfKey,
			_encodingItem->_workspace->_workspaceKey,
			labelToBeModified, label,
			urlToBeModified, url,
			typeToBeModified, type,
			descriptionToBeModified, description,
			nameToBeModified, name,
			regionToBeModified, region,
			countryToBeModified, country,
			imageToBeModified, imageMediaItemKey, imageUniqueName,
			positionToBeModified, position,
			channelDataToBeModified, channelDataRoot);
	}
	catch(...)
	{
		string errorMessage = string("updateChannelDataWithNewYouTubeURL failed")
			+ ", ingestionKey: " + to_string(ingestionKey)
			+ ", encodingJobKey: " + to_string(encodingJobKey)
			+ ", workspaceKey: " + to_string(workspaceKey)
			+ ", liveURLConfKey: " + to_string(liveURLConfKey)
			+ ", streamingYouTubeLiveURL: " + streamingYouTubeLiveURL
		;
		_logger->error(__FILEREF__ + errorMessage);

		throw runtime_error(errorMessage);
	}
}

bool EncoderVideoAudioProxy::liveGrid()
{

	bool killedByUser = liveGrid_through_ffmpeg();
	if (killedByUser)
	{
		string errorMessage = __FILEREF__ + "Encoding killed by the User"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            ;
		_logger->warn(errorMessage);
        
		throw EncodingKilledByUser();
	}
    
	return killedByUser;
}

bool EncoderVideoAudioProxy::liveGrid_through_ffmpeg()
{

	Json::Value inputChannelsRoot;
	long maxAttemptsNumberInCaseOfErrors;
	string encodersPool;
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	/*
	int gridColumns;
	int gridWidth;
	int gridHeight;
	string outputType;
	int64_t outputChannelConfKey;
	int segmentDurationInSeconds;
	int playlistEntriesNumber;
	string userAgent;
	string srtURL;
	*/
	{
	/*
        field = "Columns";
        gridColumns = JSONUtils::asInt(_encodingItem->_liveGridData->_ingestedParametersRoot,
			field, 0);

        field = "GridWidth";
        gridWidth = JSONUtils::asInt(_encodingItem->_liveGridData->_ingestedParametersRoot,
			field, 0);

        field = "GridHeight";
        gridHeight = JSONUtils::asInt(_encodingItem->_liveGridData->_ingestedParametersRoot,
			field, 0);

        field = "SRT_URL";
        srtURL = _encodingItem->_liveGridData->_ingestedParametersRoot.get(field, "").asString();
		*/

        string field = "EncodersPool";
        encodersPool = _encodingItem->_liveGridData->_ingestedParametersRoot.get(field, "").asString();

        field = "inputChannels";
        inputChannelsRoot = _encodingItem->_encodingParametersRoot[field];

        field = "maxAttemptsNumberInCaseOfErrors";
        maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(_encodingItem->_encodingParametersRoot,
			field, 0);

        field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
        waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt(
			_encodingItem->_encodingParametersRoot, field, 600);

		/*
        field = "outputType";
        outputType = _encodingItem->_encodingParametersRoot.get(field, "").asString();

        field = "outputChannelConfKey";
        outputChannelConfKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

        field = "segmentDurationInSeconds";
        segmentDurationInSeconds = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "playlistEntriesNumber";
        playlistEntriesNumber = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 0);

        field = "UserAgent";
		if (JSONUtils::isMetadataPresent(_encodingItem->_liveGridData->_ingestedParametersRoot,
			field))
			userAgent = _encodingItem->_liveGridData->_ingestedParametersRoot.get(field, "").
				asString();
		*/
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	long currentAttemptsNumberInCaseOfErrors = 0;

	long encodingStatusFailures = 0;
	// 2020-04-19: Reset encodingStatusFailures into DB. That because if we comes from an error/exception
	//	encodingStatusFailures is > than 0 but we consider here like it is 0 because our variable is set to 0
	try
	{
		_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);

		_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
				_encodingItem->_encodingJobKey, 
				encodingStatusFailures
		);
	}
	catch(...)
	{
		_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
		);
	}

	// 2020-03-11: we saw the following scenarios:
	//	1. ffmpeg was running
	//	2. after several hours it failed (1:34 am)
	//	3. our below loop tried again and this new attempt returned 404 URL NOT FOUND
	//	4. we exit from this loop
	//	5. crontab started again it after 15 minutes
	//	In this scenarios, we have to retry again without waiting the crontab check
	// 2020-03-12: Removing the urlNotFound management generated duplication of ffmpeg process
	//	For this reason we rollbacked as it was before
	while (!killedByUser && !urlForbidden && !urlNotFound
		&& currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors)
	{
		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveGridURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			if (_encodingItem->_encoderKey == -1)
			{
				/*
				string encoderToSkip;
				_currentUsedFFMpegEncoderHost = _encodersLoadBalancer->getEncoderHost(
					encodersPool, _encodingItem->_workspace, encoderToSkip);
				*/
				int64_t encoderKeyToBeSkipped = -1;
				pair<int64_t, string> encoderURL = _encodersLoadBalancer->getEncoderURL(
					encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped);
				tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost) = encoderURL;

				_logger->info(__FILEREF__ + "Configuration item"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);
				// ffmpegEncoderURL = 
				// 	_ffmpegEncoderProtocol
				// 	+ "://"
				// 	+ _currentUsedFFMpegEncoderHost + ":"
				// 	+ to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
					+ ffmpegURI
					+ "/" + to_string(_encodingItem->_encodingJobKey)
				;

				_logger->info(__FILEREF__ + "LiveGrid. Selection of the transcoder. The transcoder is selected by load balancer"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", transcoder: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				string body;
				{
					// in case of youtube url, the real URL to be used has to be calcolated
					for (int inputChannelIndex = 0; inputChannelIndex < inputChannelsRoot.size(); inputChannelIndex++)
					{
						Json::Value inputChannelRoot = inputChannelsRoot[inputChannelIndex];

						string inputChannelURLField = "inputChannelURL";
						string liveURL = inputChannelRoot.get(inputChannelURLField, "").asString();

						string inputChannelConfKeyField = "inputChannelConfKey";
						int64_t channelConfKey = JSONUtils::asInt64(inputChannelRoot, inputChannelConfKeyField, 0);

						string youTubePrefix1 ("https://www.youtube.com/");
						string youTubePrefix2 ("https://youtu.be/");
						if (
							(liveURL.size() >= youTubePrefix1.size()
								&& 0 == liveURL.compare(0, youTubePrefix1.size(), youTubePrefix1))
							||
							(liveURL.size() >= youTubePrefix2.size()
								&& 0 == liveURL.compare(0, youTubePrefix2.size(), youTubePrefix2))
							)
						{
							string streamingYouTubeLiveURL;
							long hoursFromLastCalculatedURL;
							pair<long,string> lastYouTubeURLDetails;
							try
							{
								lastYouTubeURLDetails = getLastYouTubeURLDetails(
									_encodingItem->_ingestionJobKey,
									_encodingItem->_encodingJobKey,
									_encodingItem->_workspace->_workspaceKey,
									channelConfKey);

								string lastCalculatedURL;

								tie(hoursFromLastCalculatedURL, lastCalculatedURL) = lastYouTubeURLDetails;

								_logger->info(__FILEREF__
									+ "LiveGrid. check youTubeURLCalculate"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									+ ", _retrieveStreamingYouTubeURLPeriodInHours: " + to_string(_retrieveStreamingYouTubeURLPeriodInHours)
								);
								if (hoursFromLastCalculatedURL < _retrieveStreamingYouTubeURLPeriodInHours)
									streamingYouTubeLiveURL = lastCalculatedURL;
							}
							catch(runtime_error e)
							{
								string errorMessage = __FILEREF__
									+ "LiveGrid. youTubeURLCalculate. getLastYouTubeURLDetails failed"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", YouTube URL: " + streamingYouTubeLiveURL
								;
								_logger->error(errorMessage);
							}

							if (streamingYouTubeLiveURL == "")
							{
								try
								{
									FFMpeg ffmpeg (_configuration, _logger);
									pair<string, string> streamingLiveURLDetails =
										ffmpeg.retrieveStreamingYouTubeURL(
										_encodingItem->_ingestionJobKey,
										_encodingItem->_encodingJobKey,
										liveURL);

									tie(streamingYouTubeLiveURL, ignore) = streamingLiveURLDetails;

									_logger->info(__FILEREF__ + "LiveGrid. youTubeURLCalculate. Retrieve streaming YouTube URL"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", channelConfKey: " + to_string(channelConfKey)
										+ ", initial YouTube URL: " + liveURL
										+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
										+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
									);
								}
								catch(runtime_error e)
								{
									// in case ffmpeg.retrieveStreamingYouTubeURL fails
									// we will use the last saved URL
									tie(ignore, streamingYouTubeLiveURL) = lastYouTubeURLDetails;

									string errorMessage = __FILEREF__
										+ "LiveGrid. youTubeURLCalculate. ffmpeg.retrieveStreamingYouTubeURL failed"
										+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", channelConfKey: " + to_string(channelConfKey)
										+ ", YouTube URL: " + streamingYouTubeLiveURL
									;
									_logger->error(errorMessage);

									try
									{
										_mmsEngineDBFacade->appendIngestionJobErrorMessage(
											_encodingItem->_ingestionJobKey, errorMessage);
									}
									catch(runtime_error e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
											+ ", e.what(): " + e.what()
										);
									}
									catch(exception e)
									{
										_logger->error(__FILEREF__ + "youTubeURLCalculate. appendIngestionJobErrorMessage failed"
											+ ", _ingestionJobKey: " +
												to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
										);
									}

									if (streamingYouTubeLiveURL == "")
									{
										// 2020-04-21: let's go ahead because it would be managed
										// the killing of the encodingJob
										// 2020-09-17: it does not have sense to continue
										//	if we do not have the right URL (m3u8)
										throw YouTubeURLNotRetrieved();
									}
								}

								if (streamingYouTubeLiveURL != "")
								{
									try
									{
										updateChannelDataWithNewYouTubeURL(
											_encodingItem->_ingestionJobKey,
											_encodingItem->_encodingJobKey,
											_encodingItem->_workspace->_workspaceKey,
											channelConfKey,
											streamingYouTubeLiveURL);
									}
									catch(runtime_error e)
									{
										string errorMessage = __FILEREF__
											+ "LiveGrid. youTubeURLCalculate. updateChannelDataWithNewYouTubeURL failed"
											+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
											+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
											+ ", channelConfKey: " + to_string(channelConfKey)
											+ ", YouTube URL: " + streamingYouTubeLiveURL
										;
										_logger->error(errorMessage);
									}
								}
							}
							else
							{
								_logger->info(__FILEREF__ + "LiveGrid. youTubeURLCalculate. Reuse a previous streaming YouTube URL"
									+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", channelConfKey: " + to_string(channelConfKey)
									+ ", initial YouTube URL: " + liveURL
									+ ", streaming YouTube Live URL: " + streamingYouTubeLiveURL
									+ ", hoursFromLastCalculatedURL: " + to_string(hoursFromLastCalculatedURL)
								);
							}

							inputChannelRoot[inputChannelURLField] = streamingYouTubeLiveURL;
						}
					}

					/*
					if (outputType == "HLS") // || outputType == "DASH")
					{

					}
					*/

					Json::Value liveGridMetadata;

					liveGridMetadata["ingestionJobKey"] =
						(Json::LargestUInt) (_encodingItem->_ingestionJobKey);
					liveGridMetadata["inputChannels"] = inputChannelsRoot;
					liveGridMetadata["ingestedParametersRoot"] =
						_encodingItem->_liveGridData->_ingestedParametersRoot;
					liveGridMetadata["encodingProfileDetails"] =
						_encodingItem->_liveGridData->_encodingProfileDetailsRoot;
					liveGridMetadata["encodingParametersRoot"] =
						_encodingItem->_encodingParametersRoot;

					/*
					liveGridMetadata["userAgent"] = userAgent;
					liveGridMetadata["encodingProfileDetails"] = encodingProfileDetailsRoot;
					liveGridMetadata["gridColumns"] = gridColumns;
					liveGridMetadata["gridWidth"] = gridWidth;
					liveGridMetadata["gridHeight"] = gridHeight;
					liveGridMetadata["outputType"] = outputType;
					liveGridMetadata["segmentDurationInSeconds"] = segmentDurationInSeconds;
					liveGridMetadata["playlistEntriesNumber"] = playlistEntriesNumber;
					liveGridMetadata["srtURL"] = srtURL;
					liveGridMetadata["manifestDirectoryPath"] = manifestDirectoryPath;
					liveGridMetadata["manifestFileName"] = manifestFileName;
					*/

					{
						Json::StreamWriterBuilder wbuilder;

						body = Json::writeString(wbuilder, liveGridMetadata);
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

				// timeout consistent with nginx configuration (fastcgi_read_timeout)
				request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

				// if (_ffmpegEncoderProtocol == "https")
				string httpsPrefix("https");
				if (ffmpegEncoderURL.size() >= httpsPrefix.size()
					&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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

				_logger->info(__FILEREF__ + "Calling transcoder for LiveGrid media file"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", body: " + body
				);
				responseInitialized = true;
				request.perform();

				string sResponse = response.str();
				// LF and CR create problems to the json parser...
				while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
					sResponse.pop_back();

				Json::Value liveGridResponseRoot;
				try
				{
					Json::CharReaderBuilder builder;
					Json::CharReader* reader = builder.newCharReader();
					string errors;

					bool parsingSuccessful = reader->parse(sResponse.c_str(),
                        sResponse.c_str() + sResponse.size(), 
                        &liveGridResponseRoot, &errors);
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
					if (JSONUtils::isMetadataPresent(liveGridResponseRoot, field))
					{
						string error = liveGridResponseRoot.get(field, "XXX").asString();
                    
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
				_logger->info(__FILEREF__ + "LiveGrid. Selection of the transcoder. The transcoder is already saved (DB), the encoding should be already running"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encoderKey: " + to_string(_encodingItem->_encoderKey)
				);

				_currentUsedFFMpegEncoderHost = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
				// manifestFilePathName = _encodingItem->_stagingEncodedAssetPathName;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
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
				+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(
				_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			// encodingProgress: fixed to -1 (LIVE)
			{
				try
				{
					int encodingProgress = -1;

					_logger->info(__FILEREF__ + "updateEncodingJobProgress"
						+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingProgress: " + to_string(encodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress (
						_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			// string lastRecordedAssetFileName;
			chrono::system_clock::time_point startCheckingEncodingStatus = chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int encodingPid;
			int lastEncodingPid = 0;

			// 2020-11-28: the next while, it was added encodingStatusFailures condition because,
			//  in case the transcoder is down (once I had to upgrade his operative system),
			//  the engine has to select another encoder and not remain in the next loop indefinitely
            while(!(encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures))
            // while(!encodingFinished)
            {
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
						getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
						urlForbidden, urlNotFound, ignore, encodingPid) = encodingStatus;

					encoderNotReachableFailures = 0;

					// health check and retention is done by ffmpegEncoder.cpp

					if (encodingErrorMessage != "")
					{
						try
						{
							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, encodingErrorMessage);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError) // || chunksWereNotGenerated)
					{
						if (urlForbidden || urlNotFound)	// see my comment at the beginning of the while loop
						{
							string errorMessage =
								__FILEREF__ + "Encoding failed because of URL Forbidden or Not Found (look the Transcoder logs)"             
								+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingErrorMessage: " + encodingErrorMessage
								;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						currentAttemptsNumberInCaseOfErrors++;

						string errorMessage = __FILEREF__ + "Encoding failed"             
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", encodingErrorMessage: " + encodingErrorMessage
							// + ", chunksWereNotGenerated: " + to_string(chunksWereNotGenerated)
						;
						_logger->error(errorMessage);

						encodingStatusFailures++;

						// in this scenario encodingFinished is true

						_logger->info(__FILEREF__ + "Start waiting loop for the next call"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);

						chrono::system_clock::time_point startWaiting = chrono::system_clock::now();
						chrono::system_clock::time_point now;
						do
						{
							// update EncodingJob failures number to notify the GUI EncodingJob is failing
							try
							{
								_logger->info(__FILEREF__ + "check and update encodingJob FailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								long previousEncodingStatusFailures =
									_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures);
								if (previousEncodingStatusFailures < 0)
								{
									_logger->info(__FILEREF__ + "LiveGrid Killed by user during waiting loop"
										+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
										+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
									);

									// when previousEncodingStatusFailures is < 0 means:
									// 1. the live proxy is not starting (ffmpeg is generating continuously an error)
									// 2. User killed the encoding through MMS GUI or API
									// 3. the kill procedure (in API module) was not able to kill the ffmpeg process,
									//		because it does not exist the process and set the failuresNumber DB field
									//		to a negative value in order to communicate with this thread 
									// 4. This thread, when it finds a negative failuresNumber, knows the encoding
									//		was killed and exit from the loop
									encodingFinished = true;
									killedByUser = true;
								}
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}

							this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

							now = chrono::system_clock::now();
						}
						while (chrono::duration_cast<chrono::seconds>(now - startWaiting)
								< chrono::seconds(waitingSecondsBetweenAttemptsInCaseOfErrors)
								&& currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors
								&& !killedByUser);

						// if (chunksWereNotGenerated)
						// 	encodingFinished = true;

						throw runtime_error(errorMessage);
					}
					else
					{
						// ffmpeg is running successful, we will make sure currentAttemptsNumberInCaseOfErrors is reset
						currentAttemptsNumberInCaseOfErrors = 0;

						if (encodingStatusFailures > 0)
						{
							try
							{
								// update EncodingJob failures number to notify the GUI encodingJob is successful
								encodingStatusFailures = 0;

								_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										encodingStatusFailures
								);
							}
							catch(...)
							{
								_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
							}
						}
					}

					// encodingProgress/encodingPid
					{
						if (lastEncodingPid != encodingPid)
						{
							try
							{
								_logger->info(__FILEREF__ + "updateEncodingPid"
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingPid: " + to_string(encodingPid)
								);
								_mmsEngineDBFacade->updateEncodingPid (
									_encodingItem->_encodingJobKey, encodingPid);

								lastEncodingPid = encodingPid;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
								);
							}
						}
					}
                }
				catch(EncoderNotReachable e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS version
					//	2. this thread remained in this loop (while(!encodingFinished))
					//		and the channel did not work until the Encoder was working again
					//	In this scenario, so when the encoder is not reachable at all, the engine
					//	has to select a new encoder.
					//	For this reason we added this EncoderNotReachable catch
					//	and the encoderNotReachableFailures variable

					_logger->error(__FILEREF__ + "Transcoder is not reachable at all, let's select another encoder"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encoderNotReachableFailures: " + to_string(encoderNotReachableFailures)
						+ ", _maxEncoderNotReachableFailures: " + to_string(_maxEncoderNotReachableFailures)
						+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
					);
				}
                catch(...)
                {
					_logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					);
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			{
				_logger->error(__FILEREF__ + "LiveGrid media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
                    + ", killedByUser: " + to_string(killedByUser)
                    + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
                    + ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
				);
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
            
			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

            // throw e;
        }
        catch (runtime_error e)
        {
            _logger->error(__FILEREF__ + "Encoding URL failed/runtime_error"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL 
                + ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
            );

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

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

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					encodingStatusFailures);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

            // throw e;
        }
	}

	if (urlForbidden)
	{
		string errorMessage = __FILEREF__ + "LiveGrid: URL forbidden"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
			+ ", _encodingParameters: " + _encodingItem->_encodingParameters
            ;
		_logger->error(errorMessage);
        
		throw FFMpegURLForbidden();
	}
	else if (urlNotFound)
	{
		string errorMessage = __FILEREF__ + "LiveGrid: URL Not Found"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
			+ ", _encodingParameters: " + _encodingItem->_encodingParameters
            ;
		_logger->error(errorMessage);
        
		throw FFMpegURLNotFound();
	}
	else if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		string errorMessage = __FILEREF__ + "Reached the max number of attempts to the URL"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors) 
            + ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors) 
            ;
		_logger->error(errorMessage);
        
		throw EncoderError();
	}

    return killedByUser;
}

void EncoderVideoAudioProxy::processLiveGrid(bool killedByUser)
{
    try
    {
		// This method is never called because in both the scenarios below an exception
		// by EncoderVideoAudioProxy::liveGrid is raised:
		// - transcoding killed by the user 
		// - The max number of calls to the URL were all done and all failed
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processLiveGrid failed"
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
        _logger->error(__FILEREF__ + "processLiveGrid failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingParameters: " + _encodingItem->_encodingParameters
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

/*
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
				utcRecordingPeriodStart = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);

				field = "utcRecordingPeriodEnd";
				utcRecordingPeriodEnd = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, 0);
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
            
				// ffmpegEncoderURL = 
                //     _ffmpegEncoderProtocol
                //     + "://"
                //     + _currentUsedFFMpegEncoderHost + ":"
                //     + to_string(_ffmpegEncoderPort)
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
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

				// timeout consistent with nginx configuration (fastcgi_read_timeout)
				// request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

				// 2020-09-16: this is just getEncodingProgress, we do not have to lose much time
				// otherwise the loop inside ActiveEncodingsManager will not process
				// in a fast way the other encodings (processEncodingJob method)
				int encodingProgressTimeoutInSeconds = 2;
				request.setOpt(new curlpp::options::Timeout(encodingProgressTimeoutInSeconds));

				// if (_ffmpegEncoderProtocol == "https")
				string httpsPrefix("https");
				if (ffmpegEncoderURL.size() >= httpsPrefix.size()
					&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
				{
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

				chrono::system_clock::time_point startEncodingProgress = chrono::system_clock::now();

				_logger->info(__FILEREF__ + "getEncodingProgress"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
				);
				responseInitialized = true;
				request.perform();
				chrono::system_clock::time_point endEncodingProgress = chrono::system_clock::now();
				_logger->info(__FILEREF__ + "getEncodingProgress"
                        + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncodingProgress - startEncodingProgress).count()) + "@"
                    + ", response.str: " + response.str()
				);
            
				string sResponse = response.str();
				// LF and CR create problems to the json parser...
				while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
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
					if (JSONUtils::isMetadataPresent(encodeProgressResponse, field))
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
						if (JSONUtils::isMetadataPresent(encodeProgressResponse, field))
						{
							encodingProgress = JSONUtils::asInt(encodeProgressResponse, "encodingProgress", 0);
                        
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
*/

tuple<bool, bool, bool, string, bool, bool, int, int> EncoderVideoAudioProxy::getEncodingStatus()
{
    bool encodingFinished;
    bool killedByUser;
	bool completedWithError;
	string encoderErrorMessage;
	bool urlNotFound;
	bool urlForbidden;
	int encodingProgress;
	int pid;
    
    string ffmpegEncoderURL;
    ostringstream response;
	bool responseInitialized = false;
    try
    {
        // ffmpegEncoderURL = 
        //         _ffmpegEncoderProtocol
        //         + "://"                
        //         + _currentUsedFFMpegEncoderHost + ":"
        //         + to_string(_ffmpegEncoderPort)
        ffmpegEncoderURL =
			_currentUsedFFMpegEncoderHost
            + _ffmpegEncoderStatusURI
            + "/" + to_string(_encodingItem->_encodingJobKey)
        ;

        list<string> header;

        {
            string userPasswordEncoded = Convert::base64_encode(_ffmpegEncoderUser + ":"
					+ _ffmpegEncoderPassword);
            string basicAuthorization = string("Authorization: Basic ") + userPasswordEncoded;

            header.push_back(basicAuthorization);
        }

        curlpp::Cleanup cleaner;
        curlpp::Easy request;

        // Setting the URL to retrive.
        request.setOpt(new curlpp::options::Url(ffmpegEncoderURL));

		// timeout consistent with nginx configuration (fastcgi_read_timeout)
		request.setOpt(new curlpp::options::Timeout(_ffmpegEncoderTimeoutInSeconds));

        // if (_ffmpegEncoderProtocol == "https")
		string httpsPrefix("https");
		if (ffmpegEncoderURL.size() >= httpsPrefix.size()
			&& 0 == ffmpegEncoderURL.compare(0, httpsPrefix.size(), httpsPrefix))
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

        chrono::system_clock::time_point startEncodingStatus = chrono::system_clock::now();

        _logger->info(__FILEREF__ + "getEncodingStatus"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL
        );
		responseInitialized = true;
        request.perform();
        chrono::system_clock::time_point endEncodingStatus = chrono::system_clock::now();

        string sResponse = response.str();
        // LF and CR create problems to the json parser...
        while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
            sResponse.pop_back();

        _logger->info(__FILEREF__ + "getEncodingStatus"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                // + ", sResponse: " + sResponse
                + ", @MMS statistics@ - encodingDuration (secs): @" + to_string(
					chrono::duration_cast<chrono::seconds>(endEncodingStatus - startEncodingStatus).count()) + "@"
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
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				completedWithError = JSONUtils::asBool(encodeStatusResponse, field, false);
			else
				completedWithError = false;

			field = "errorMessage";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				encoderErrorMessage = encodeStatusResponse.get(field, "").asString();
			else
				encoderErrorMessage = "";

			field = "encodingFinished";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				encodingFinished = JSONUtils::asBool(encodeStatusResponse, field, false);
			else
				encodingFinished = false;

			field = "killedByUser";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				killedByUser = JSONUtils::asBool(encodeStatusResponse, field, false);
			else
				killedByUser = false;

			field = "urlForbidden";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				urlForbidden = JSONUtils::asBool(encodeStatusResponse, field, false);
			else
				urlForbidden = false;

			field = "urlNotFound";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				urlNotFound = JSONUtils::asBool(encodeStatusResponse, field, false);
			else
				urlNotFound = false;

			field = "encodingProgress";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				encodingProgress = JSONUtils::asInt(encodeStatusResponse, field, -1);
			else
				encodingProgress = 0;

			field = "pid";
			if (JSONUtils::isMetadataPresent(encodeStatusResponse, field))
				pid = JSONUtils::asInt(encodeStatusResponse, field, -1);
			else
				pid = -1;
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
		if (response.str().find("502 Bad Gateway") != string::npos)
		{
			_logger->error(__FILEREF__ + "Encoder is not reachable, is it down?"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
				+ ", exception: " + e.what()
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
			);

			throw EncoderNotReachable();
		}
		else
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

    return make_tuple(encodingFinished, killedByUser, completedWithError,
		encoderErrorMessage, urlForbidden, urlNotFound, encodingProgress, pid);
}

string EncoderVideoAudioProxy::generateMediaMetadataToIngest(
        int64_t ingestionJobKey,
        string fileFormat,
		int64_t faceOfVideoMediaItemKey,
        Json::Value parametersRoot
)
{
    string field = "FileFormat";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
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

void EncoderVideoAudioProxy::readingImageProfile(
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

    // FileFormat
    {
        field = "FileFormat";
        if (!JSONUtils::isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newFormat = encodingProfileRoot.get(field, "XXX").asString();

        encodingImageFormatValidation(newFormat);
    }

    Json::Value encodingProfileImageRoot;
    {
        field = "Image";
        if (!JSONUtils::isMetadataPresent(encodingProfileRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        encodingProfileImageRoot = encodingProfileRoot[field];
    }
    
    // Width
    {
        field = "Width";
        if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newWidth = JSONUtils::asInt(encodingProfileImageRoot, field, 0);
    }

    // Height
    {
        field = "Height";
        if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newHeight = JSONUtils::asInt(encodingProfileImageRoot, field, 0);
    }

    // Aspect
    {
        field = "AspectRatio";
        if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        newAspectRatio = JSONUtils::asBool(encodingProfileImageRoot, field, false);
    }

    // Interlace
    {
        field = "InterlaceType";
        if (!JSONUtils::isMetadataPresent(encodingProfileImageRoot, field))
        {
            string errorMessage = __FILEREF__ + "Field is not present or it is null"
                    + ", Field: " + field;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

        sNewInterlaceType = encodingProfileImageRoot.get(field, "XXX").asString();

        newInterlaceType = encodingImageInterlaceTypeValidation(sNewInterlaceType);
    }
}

void EncoderVideoAudioProxy::encodingImageFormatValidation(string newFormat)
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

Magick::InterlaceType EncoderVideoAudioProxy::encodingImageInterlaceTypeValidation(string sNewInterlaceType)
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

/*
// same method is duplicated in API_Encoding.cpp
void EncoderVideoAudioProxy::killEncodingJob(string transcoderHost, int64_t encodingJobKey)
{
	string ffmpegEncoderURL;
	ostringstream response;
	try
	{
		ffmpegEncoderURL = _ffmpegEncoderProtocol
			+ "://"
			+ transcoderHost + ":"
			+ to_string(_ffmpegEncoderPort)
			+ _ffmpegEncoderKillEncodingURI
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
		request.setOpt(new curlpp::options::CustomRequest("DELETE"));

		if (_ffmpegEncoderProtocol == "https")
		{
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSLCERTPASSWD> SslCertPasswd;                            
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEY> SslKey;                                          
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYTYPE> SslKeyType;                                  
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSLKEYPASSWD> SslKeyPasswd;                              
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSLENGINE> SslEngine;                                    
                  // typedef curlpp::NoValueOptionTrait<CURLOPT_SSLENGINE_DEFAULT> SslEngineDefault;                           
                  // typedef curlpp::OptionTrait<long, CURLOPT_SSLVERSION> SslVersion;                                         
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_CAINFO> CaInfo;                                          
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_CAPATH> CaPath;                                          
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_RANDOM_FILE> RandomFile;                                 
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_EGDSOCKET> EgdSocket;                                    
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_SSL_CIPHER_LIST> SslCipherList;                          
                  // typedef curlpp::OptionTrait<std::string, CURLOPT_KRB4LEVEL> Krb4Level;                                    
                                                                                                
              
			// cert is stored PEM coded in file... 
			// since PEM is default, we needn't set it for PEM 
			// curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM");
			// curlpp::OptionTrait<string, CURLOPT_SSLCERTTYPE> sslCertType("PEM");
			// equest.setOpt(sslCertType);

			// set the cert for client authentication
			// "testcert.pem"
			// curl_easy_setopt(curl, CURLOPT_SSLCERT, pCertFile);
			// curlpp::OptionTrait<string, CURLOPT_SSLCERT> sslCert("cert.pem");
			// request.setOpt(sslCert);

			// sorry, for engine we must set the passphrase
			//   (if the key has one...)
			// const char *pPassphrase = NULL;
			// if(pPassphrase)
			// curl_easy_setopt(curl, CURLOPT_KEYPASSWD, pPassphrase);

			// if we use a key stored in a crypto engine,
			//   we must set the key type to "ENG"
			// pKeyType  = "PEM";
			// curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, pKeyType);

			// set the private key (file or ID in engine)
			// pKeyName  = "testkey.pem";
			// curl_easy_setopt(curl, CURLOPT_SSLKEY, pKeyName);

			// set the file with the certs vaildating the server
			// *pCACertFile = "cacert.pem";
			// curl_easy_setopt(curl, CURLOPT_CAINFO, pCACertFile);
              
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

		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
		);
		request.perform();
		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
		_logger->info(__FILEREF__ + "killEncodingJob"
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
			+ ", response.str: " + response.str()
		);

		string sResponse = response.str();

		// LF and CR create problems to the json parser...
		while (sResponse.size() > 0 && (sResponse.back() == 10 || sResponse.back() == 13))
			sResponse.pop_back();

		{
			string message = __FILEREF__ + "Kill encoding response"
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sResponse: " + sResponse
			;
			_logger->info(message);
		}

		long responseCode = curlpp::infos::ResponseCode::get(request);                                        
		if (responseCode != 200)
		{
			string errorMessage = __FILEREF__ + "Kill encoding URL failed"                                       
				+ ", encodingJobKey: " + to_string(encodingJobKey)
				+ ", sResponse: " + sResponse                                                                 
				+ ", responseCode: " + to_string(responseCode)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (curlpp::LogicError & e) 
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (LogicError)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);
            
		throw e;
	}
	catch (curlpp::RuntimeError & e) 
	{ 
		string errorMessage = string("killEncoding URL failed (RuntimeError)")
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
		_logger->error(__FILEREF__ + "killEncoding URL failed (runtime_error)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "killEncoding URL failed (exception)"
			+ ", encodingJobKey: " + to_string(encodingJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
			+ ", response.str(): " + response.str()
		);

		throw e;
	}
}
*/

