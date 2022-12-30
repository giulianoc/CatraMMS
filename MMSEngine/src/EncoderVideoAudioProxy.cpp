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
#include "MMSCURL.h"                                                                                          
#include "AWSSigner.h"
#include <fstream>
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
#include "catralibraries/DateTime.h"
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

#include <aws/core/Aws.h>
#include <aws/medialive/MediaLiveClient.h>
#include <aws/medialive/model/StartChannelRequest.h>
#include <aws/medialive/model/StopChannelRequest.h>
#include <aws/medialive/model/DescribeChannelRequest.h>
#include <aws/medialive/model/DescribeChannelResult.h>


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

    _mp4Encoder             = JSONUtils::asString(_configuration["encoding"], "mp4Encoder", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", encoding->mp4Encoder: " + _mp4Encoder
    );
    _mpeg2TSEncoder         = JSONUtils::asString(_configuration["encoding"], "mpeg2TSEncoder", "");
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

	/*
	_liveRecorderVirtualVODImageLabel = _configuration["ffmpeg"].get("liveRecorderVirtualVODImageLabel",
		"").asString();
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", ffmpeg->liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel
	);
	*/

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
    _ffmpegEncoderUser = JSONUtils::asString(_configuration["ffmpeg"], "encoderUser", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser
    );
    _ffmpegEncoderPassword = JSONUtils::asString(_configuration["ffmpeg"], "encoderPassword", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderPassword: " + "..."
    );
    _ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(_configuration["ffmpeg"],
		"encoderTimeoutInSeconds", 120);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderTimeoutInSeconds: " + to_string(_ffmpegEncoderTimeoutInSeconds)
    );
    _ffmpegEncoderProgressURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderProgressURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderProgressURI: " + _ffmpegEncoderProgressURI
    );
    _ffmpegEncoderStatusURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderStatusURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI
    );
    _ffmpegEncoderKillEncodingURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderKillEncodingURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encoderKillEncodingURI: " + _ffmpegEncoderKillEncodingURI
    );
    _ffmpegEncodeURI = JSONUtils::asString(_configuration["ffmpeg"], "encodeURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->encodeURI: " + _ffmpegEncodeURI
    );
    _ffmpegOverlayImageOnVideoURI = JSONUtils::asString(_configuration["ffmpeg"], "overlayImageOnVideoURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->overlayImageOnVideoURI: " + _ffmpegOverlayImageOnVideoURI
    );
    _ffmpegOverlayTextOnVideoURI = JSONUtils::asString(_configuration["ffmpeg"], "overlayTextOnVideoURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->overlayTextOnVideoURI: " + _ffmpegOverlayTextOnVideoURI
    );
    _ffmpegGenerateFramesURI = JSONUtils::asString(_configuration["ffmpeg"], "generateFramesURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->generateFramesURI: " + _ffmpegGenerateFramesURI
    );
    _ffmpegSlideShowURI = JSONUtils::asString(_configuration["ffmpeg"], "slideShowURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->slideShowURI: " + _ffmpegSlideShowURI
    );
    _ffmpegLiveRecorderURI = JSONUtils::asString(_configuration["ffmpeg"], "liveRecorderURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveRecorderURI: " + _ffmpegLiveRecorderURI
    );
    _ffmpegLiveProxyURI = JSONUtils::asString(_configuration["ffmpeg"], "liveProxyURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveProxyURI: " + _ffmpegLiveProxyURI
    );
    _ffmpegLiveGridURI = JSONUtils::asString(_configuration["ffmpeg"], "liveGridURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->liveGridURI: " + _ffmpegLiveGridURI
    );
    _ffmpegVideoSpeedURI = JSONUtils::asString(_configuration["ffmpeg"], "videoSpeedURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->videoSpeedURI: " + _ffmpegVideoSpeedURI
    );
    _ffmpegPictureInPictureURI = JSONUtils::asString(_configuration["ffmpeg"], "pictureInPictureURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->pictureInPictureURI: " + _ffmpegPictureInPictureURI
    );
    _ffmpegIntroOutroOverlayURI = JSONUtils::asString(_configuration["ffmpeg"], "introOutroOverlayURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->ffmpegIntroOutroOverlayURI: " + _ffmpegIntroOutroOverlayURI
    );
    _ffmpegCutFrameAccurateURI = JSONUtils::asString(_configuration["ffmpeg"], "cutFrameAccurateURI", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", ffmpeg->ffmpegCutFrameAccurateURI: " + _ffmpegCutFrameAccurateURI
    );


    _computerVisionCascadePath             = JSONUtils::asString(_configuration["computerVision"], "cascadePath", "");
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->cascadePath: " + _computerVisionCascadePath
    );
	if (_computerVisionCascadePath.size() > 0 && _computerVisionCascadePath.back() == '/')
		_computerVisionCascadePath.pop_back();
    _computerVisionDefaultScale				= JSONUtils::asDouble(_configuration["computerVision"], "defaultScale", 1.1);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultScale: " + to_string(_computerVisionDefaultScale)
    );
    _computerVisionDefaultMinNeighbors		= JSONUtils::asInt(_configuration["computerVision"],
			"defaultMinNeighbors", 2);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultMinNeighbors: " + to_string(_computerVisionDefaultMinNeighbors)
    );
    _computerVisionDefaultTryFlip		= JSONUtils::asBool(_configuration["computerVision"], "defaultTryFlip", false);
    _logger->info(__FILEREF__ + "Configuration item"
        + ", computerVision->defaultTryFlip: " + to_string(_computerVisionDefaultTryFlip)
    );

	_timeBeforeToPrepareResourcesInMinutes		= JSONUtils::asInt(_configuration["mms"],
			"liveRecording_timeBeforeToPrepareResourcesInMinutes", 2);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", mms->liveRecording_timeBeforeToPrepareResourcesInMinutes: " + to_string(_timeBeforeToPrepareResourcesInMinutes)
	);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(_configuration["storage"],
		"waitingNFSSync_maxMillisecondsToWait", 60000);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", storage->_waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks = JSONUtils::asInt(_configuration["storage"],
		"waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: "
		+ to_string(_waitingNFSSync_milliSecondsWaitingBetweenChecks)
	);

	_keyPairId =  JSONUtils::asString(_configuration["aws"], "keyPairId", "");
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->keyPairId: " + _keyPairId
	);
	_privateKeyPEMPathName =  JSONUtils::asString(_configuration["aws"], "privateKeyPEMPathName", "");
	_logger->info(__FILEREF__ + "Configuration item"
		+ ", aws->privateKeyPEMPathName: " + _privateKeyPEMPathName
	);

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
        + ", _encodingParametersRoot: " + JSONUtils::toString(_encodingItem->_encodingParametersRoot)
        + ", _ingestedParametersRoot: " + JSONUtils::toString(_encodingItem->_ingestedParametersRoot)
    );

    string stagingEncodedAssetPathName;
	bool killedByUser;
    try
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeImage)
        {
			encodeContentImage();
        }
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegEncodeURI, maxConsecutiveEncodingStatusFailures);
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegOverlayImageOnVideoURI, maxConsecutiveEncodingStatusFailures);
			// killedByUser = overlayImageOnVideo();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegOverlayTextOnVideoURI, maxConsecutiveEncodingStatusFailures);
			// killedByUser = overlayTextOnVideo();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegGenerateFramesURI, maxConsecutiveEncodingStatusFailures);
            // killedByUser = generateFrames();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::SlideShow)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegSlideShowURI, maxConsecutiveEncodingStatusFailures);
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
			bool killedByUser = liveRecorder();
			// tie(killedByUser, main) = killedByUserAndMain;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
        {
			string proxyType = "liveProxy";
			killedByUser = liveProxy(proxyType);
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VODProxy)
        {
			string proxyType = "vodProxy";
			killedByUser = liveProxy(proxyType);
			// killedByUser = vodProxy();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::Countdown)
        {
			string proxyType = "countdownProxy";
			killedByUser = liveProxy(proxyType);
			// killedByUser = awaitingTheBeginning();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveGrid)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegLiveGridURI, maxConsecutiveEncodingStatusFailures);
			// killedByUser = liveGrid();
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VideoSpeed)
        {
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegVideoSpeedURI, maxConsecutiveEncodingStatusFailures);
			// killedByUser = videoSpeed();
        }
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegPictureInPictureURI, maxConsecutiveEncodingStatusFailures);
			// pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = pictureInPicture();
			// tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::IntroOutroOverlay)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegIntroOutroOverlayURI, maxConsecutiveEncodingStatusFailures);
			// tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::CutFrameAccurate)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegCutFrameAccurateURI, maxConsecutiveEncodingStatusFailures);
			// pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser = cutFrameAccurate();
			// tie(stagingEncodedAssetPathName, killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
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
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			_mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::MaxCapacityReached, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey);
                // main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
				+ ", e.what(): " + e.what()
			);
		}
		catch(exception e)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
        );

        // throw e;
        return;
    }
    catch(EncoderNotFound e)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// 'no update is done'
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (_encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::KilledByUser, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what());
                // main ? _encodingItem->_ingestionJobKey : -1, e.what());
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob KilledByUser FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
        );

        // throw e;
        return;
    }

    bool isIngestionJobCompleted;

    try
    {
        if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeImage)
        {
			processEncodedImage();

			isIngestionJobCompleted = true;
        }
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::EncodeVideoAudio)
        {
			processEncodedContentVideoAudio();

			// isIngestionJobCompleted is true because ingestionJob has to be updated
			//		because the content was ingested.
			// It would be false in case LocalAssetIngestionEvent was used
			//		but this is not the case for EncodeVideoAudio
			isIngestionJobCompleted = true;
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayImageOnVideo)
        {
            processOverlayedImageOnVideo(killedByUser);

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
        {
            processOverlayedTextOnVideo(killedByUser);     

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
        {
            processGeneratedFrames(killedByUser);     
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::SlideShow)
        {
            processSlideShow();

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceRecognition)
        {
            processFaceRecognition(stagingEncodedAssetPathName);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceIdentification)
        {
            processFaceIdentification(stagingEncodedAssetPathName);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
        {
            processLiveRecorder(killedByUser);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
        {
            processLiveProxy(killedByUser);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VODProxy)
        {
            processLiveProxy(killedByUser);
            // processVODProxy(killedByUser);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
		}
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::Countdown)
        {
            processLiveProxy(killedByUser);
            // processAwaitingTheBeginning(killedByUser);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveGrid)
        {
            processLiveGrid(killedByUser);
            
			isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VideoSpeed)
        {
            processVideoSpeed(killedByUser);     
            
			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
        {
			processPictureInPicture(killedByUser);
            
			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::IntroOutroOverlay)
        {
            processIntroOutroOverlay();

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
        }
        else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::CutFrameAccurate)
        {
            processCutFrameAccurate();
            
			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false;	// file has still to be ingested
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

        if (stagingEncodedAssetPathName != "" &&
				(FileIO::fileExisting(stagingEncodedAssetPathName) || 
                FileIO::directoryExisting(stagingEncodedAssetPathName))
		)
        {
            FileIO::DirectoryEntryType_t detSourceFileType = FileIO::getDirectoryEntryType(
				stagingEncodedAssetPathName);

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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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

        if (stagingEncodedAssetPathName != "" &&
				(FileIO::fileExisting(stagingEncodedAssetPathName) || 
                FileIO::directoryExisting(stagingEncodedAssetPathName))
		)
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
				+ ", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main should update the ingestionJob status
			// This because, if also the 'backup' liverecording updates the ingestionJob, it will generate an erro
			// PunctualError is used because, in case it always happens, the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob (
                _encodingItem->_encodingJobKey, 
                MMSEngineDBFacade::EncodingError::PunctualError,    // ErrorBeforeEncoding, 
                false,	// isIngestionJobFinished: this field is not used by updateEncodingJob
                _encodingItem->_ingestionJobKey, e.what(),
                // main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJob PunctualError FAILED"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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
			+ ", isIngestionJobCompleted: " + to_string(isIngestionJobCompleted)
			+ ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

        _mmsEngineDBFacade->updateEncodingJob (
            _encodingItem->_encodingJobKey, 
            MMSEngineDBFacade::EncodingError::NoError,
			isIngestionJobCompleted,
			_encodingItem->_ingestionJobKey);
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
        + ", _ingestedParametersRoot: " + JSONUtils::toString(_encodingItem->_ingestedParametersRoot)
    );
}

void EncoderVideoAudioProxy::encodeContentImage()
{
	int64_t encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
		"encodingProfileKey", 0);

	Json::Value sourcesToBeEncodedRoot
		= _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];

	for(int sourceIndex = 0; sourceIndex < sourcesToBeEncodedRoot.size(); sourceIndex++)
	{
		Json::Value sourceToBeEncodedRoot = sourcesToBeEncodedRoot[sourceIndex];

		bool stopIfReferenceProcessingError = JSONUtils::asBool(sourceToBeEncodedRoot,
			"stopIfReferenceProcessingError", false);

		string          stagingEncodedAssetPathName;

		try
		{
			string sourceFileName = JSONUtils::asString(sourceToBeEncodedRoot, "sourceFileName", "");
			string sourceRelativePath = JSONUtils::asString(sourceToBeEncodedRoot, "sourceRelativePath", "");
			string sourceFileExtension = JSONUtils::asString(sourceToBeEncodedRoot, "sourceFileExtension", "");
			string mmsSourceAssetPathName = JSONUtils::asString(sourceToBeEncodedRoot, "mmsSourceAssetPathName", "");
			Json::Value encodingProfileDetailsRoot
				= _encodingItem->_encodingParametersRoot["encodingProfileDetails"];

			string encodedFileName;
			{
				size_t extensionIndex = sourceFileName.find_last_of(".");
				if (extensionIndex == string::npos)
				{
					string errorMessage = __FILEREF__ + "No extension find in the asset file name"
						+ ", sourceFileName: " + sourceFileName;
					_logger->error(errorMessage);

					throw runtime_error(errorMessage);
				}                                                               
				encodedFileName = sourceFileName.substr(0, extensionIndex) + "_"
					+ to_string(encodingProfileKey);
			}

			string                      newImageFormat;
			int                         newWidth;
			int                         newHeight;
			bool                        newAspectRatio;
			string                      sNewInterlaceType;
			Magick::InterlaceType       newInterlaceType;
			{
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

				readingImageProfile(encodingProfileDetailsRoot, newImageFormat, newWidth,
					newHeight, newAspectRatio, sNewInterlaceType, newInterlaceType);
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

				encodedFileName.append(sourceFileExtension);

				bool removeLinuxPathIfExist = true;
				bool neededForTranscoder = false;
				stagingEncodedAssetPathName = _mmsStorage->getStagingAssetPathName(
					neededForTranscoder,
					_encodingItem->_workspace->_directoryName,
					to_string(_encodingItem->_encodingJobKey),
					sourceRelativePath,
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

			sourceToBeEncodedRoot["out_stagingEncodedAssetPathName"] = stagingEncodedAssetPathName;
			sourcesToBeEncodedRoot[sourceIndex] = sourceToBeEncodedRoot;
			_encodingItem->_encodingParametersRoot["sourcesToBeEncoded"] = sourcesToBeEncodedRoot;
		}
		catch (Magick::Error &e)
		{
			_logger->info(__FILEREF__ + "ImageMagick exception"
				+ ", e.what(): " + e.what()
				+ ", encodingItem->_encodingJobKey: "
					+ to_string(_encodingItem->_encodingJobKey)
				+ ", encodingItem->_ingestionJobKey: "
					+ to_string(_encodingItem->_ingestionJobKey)
			);
        
			if (sourcesToBeEncodedRoot.size() == 1 || stopIfReferenceProcessingError)
			{
				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName
								= stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
							+ ", exception: " + e.what()
						);
					}
				}

				throw runtime_error(e.what());
			}
		}
		catch (exception e)
		{
			_logger->info(__FILEREF__ + "ImageMagick exception"
				+ ", e.what(): " + e.what()
				+ ", encodingItem->_encodingJobKey: "
					+ to_string(_encodingItem->_encodingJobKey)
				+ ", encodingItem->_ingestionJobKey: "
					+ to_string(_encodingItem->_ingestionJobKey)
			);
        
			if (sourcesToBeEncodedRoot.size() == 1 || stopIfReferenceProcessingError)
			{
				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
						if (endOfDirectoryIndex != string::npos)
						{
							directoryPathName
								= stagingEncodedAssetPathName.substr(0, endOfDirectoryIndex);

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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
							+ ", exception: " + e.what()
						);
					}
				}

				throw e;
			}
		}
	}
}

void EncoderVideoAudioProxy::processEncodedImage()
{
	int64_t encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
		"encodingProfileKey", 0);

	int64_t physicalItemRetentionInMinutes = -1;
	{
		string field = "PhysicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
		{
			string retention = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, field, "1d");
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
	}

	Json::Value sourcesToBeEncodedRoot
		= _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];

	for(int sourceIndex = 0; sourceIndex < sourcesToBeEncodedRoot.size(); sourceIndex++)
	{
		Json::Value sourceToBeEncodedRoot = sourcesToBeEncodedRoot[sourceIndex];

		// bool stopIfReferenceProcessingError = JSONUtils::asBool(sourceToBeEncodedRoot,
		// 	"stopIfReferenceProcessingError", false);

		try
		{
			string stagingEncodedAssetPathName
				= JSONUtils::asString(sourceToBeEncodedRoot, "out_stagingEncodedAssetPathName", "");

			string sourceRelativePath
				= JSONUtils::asString(sourceToBeEncodedRoot, "sourceRelativePath", "");

			int64_t sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot,
				"sourceMediaItemKey", 0);


			if (stagingEncodedAssetPathName == "")
				continue;

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
					+ ", encodingItem->_encodingJobKey: "
						+ to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: "
						+ to_string(_encodingItem->_ingestionJobKey)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
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
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
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
					+ ", encodingItem->_encodingJobKey: "
						+ to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: "
						+ to_string(_encodingItem->_ingestionJobKey)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
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
					+ ", encodingItem->_encodingJobKey: "
						+ to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: "
						+ to_string(_encodingItem->_ingestionJobKey)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
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
					+ ", encodingItem->_encodingJobKey: "
						+ to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: "
						+ to_string(_encodingItem->_ingestionJobKey)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
					+ ", e.what(): " + e.what()
				);

				if (stagingEncodedAssetPathName != "")
				{
					string directoryPathName;
					try
					{
						size_t endOfDirectoryIndex
							= stagingEncodedAssetPathName.find_last_of("/");
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
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", directoryPathName: " + directoryPathName
							+ ", exception: " + e.what()
						);
					}
				}

				throw e;
			}
    
			string encodedFileName;
			string mmsAssetPathName;
			unsigned long mmsPartitionIndexUsed;
			FileIO::DirectoryEntryType_t sourceFileType;
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

				bool deliveryRepositoriesToo = true;

				mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
					_encodingItem->_ingestionJobKey,
						stagingEncodedAssetPathName,
					_encodingItem->_workspace->_directoryName,
					encodedFileName,
					sourceRelativePath,
	
					&mmsPartitionIndexUsed, // OUT
					&sourceFileType,

					deliveryRepositoriesToo,
					_encodingItem->_workspace->_territories
				);
			}
			catch(runtime_error e)
			{
				_logger->error(__FILEREF__ + "_mmsStorage->moveAssetInMMSRepository failed"
					+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
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
				int64_t encodedPhysicalPathKey = _mmsEngineDBFacade->saveVariantContentMetadata(
					_encodingItem->_workspace->_workspaceKey,
					_encodingItem->_ingestionJobKey,
					liveRecordingIngestionJobKey,
					sourceMediaItemKey,
					externalReadOnlyStorage,
					externalDeliveryTechnology,
					externalDeliveryURL,
					encodedFileName,
					sourceRelativePath,
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
        
				sourceToBeEncodedRoot["out_encodedPhysicalPathKey"] = encodedPhysicalPathKey;
				sourcesToBeEncodedRoot[sourceIndex] = sourceToBeEncodedRoot;
				_encodingItem->_encodingParametersRoot["sourcesToBeEncoded"] = sourcesToBeEncodedRoot;

				_logger->info(__FILEREF__ + "Saved the Encoded content"
					+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
				);
			}
			catch(exception e)
			{
				_logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed"
					+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
				);

				if (sourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
				{
					_logger->info(__FILEREF__ + "Remove directory"
						+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", mmsAssetPathName: " + mmsAssetPathName
					);

					Boolean_t bRemoveRecursively = true;
					FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
				}
				else
				{
					bool exceptionInCaseOfErr = false;
					_logger->info(__FILEREF__ + "Remove"
						+ ", mmsAssetPathName: " + mmsAssetPathName
					);
					FileIO::remove(mmsAssetPathName, exceptionInCaseOfErr);
				}

				throw e;
			}
		}
		catch(runtime_error e)
		{
			_logger->error(__FILEREF__ + "process media input failed"
				+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", exception: " + e.what()
			);

			if (sourcesToBeEncodedRoot.size() == 1)
				throw e;
		}
		catch(exception e)
		{
			_logger->error(__FILEREF__ + "process media input failed"
				+ ", encodingItem->_encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodingItem->_ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", exception: " + e.what()
			);

			if (sourcesToBeEncodedRoot.size() == 1)
				throw e;
		}
	}
}

void EncoderVideoAudioProxy::encodeContentVideoAudio(
	string ffmpegURI,
	int maxConsecutiveEncodingStatusFailures)
{
	_logger->info(__FILEREF__ + "Creating encoderVideoAudioProxy thread"
		+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
		+ ", ffmpegURI: " + ffmpegURI
		+ ", _mp4Encoder: " + _mp4Encoder
	);

	{
		bool killedByUser = encodeContent_VideoAudio_through_ffmpeg(ffmpegURI,
			maxConsecutiveEncodingStatusFailures);
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
	}
}

bool EncoderVideoAudioProxy::encodeContent_VideoAudio_through_ffmpeg(
	string ffmpegURI,
	int maxConsecutiveEncodingStatusFailures)
{
	string encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "EncodersPool", "");

	string ffmpegEncoderURL;
	// string ffmpegURI = _ffmpegEncodeURI;
	try
	{
		_currentUsedFFMpegExternalEncoder = false;

		if (_encodingItem->_encoderKey == -1)
		{
			int64_t encoderKeyToBeSkipped = -1;
			bool externalEncoderAllowed = true;
			tuple<int64_t, string, bool> encoderDetails =
				_encodersLoadBalancer->getEncoderURL(
					_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace,
					encoderKeyToBeSkipped, externalEncoderAllowed);
			tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost,
				_currentUsedFFMpegExternalEncoder) = encoderDetails;

            _logger->info(__FILEREF__ + "getEncoderHost"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                + ", encodersPool: " + encodersPool
                + ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
                + ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
            );
            ffmpegEncoderURL =
				_currentUsedFFMpegEncoderHost
				+ ffmpegURI
				+ "/" + to_string(_encodingItem->_encodingJobKey)
			;
            string body;
            {
				_logger->info(__FILEREF__ + "building body for encoder 1"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _directoryName: " + _encodingItem->_workspace->_directoryName
				);

                Json::Value encodingMedatada;

                encodingMedatada["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
                encodingMedatada["ingestionJobKey"] = (Json::LargestUInt) (_encodingItem->_ingestionJobKey);
                encodingMedatada["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;
                encodingMedatada["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;

				body = JSONUtils::toString(encodingMedatada);
            }

			Json::Value encodeContentResponse = MMSCURL::httpPostStringAndGetJson(
				_logger,
				_encodingItem->_ingestionJobKey,
				ffmpegEncoderURL,
				_ffmpegEncoderTimeoutInSeconds,
				_ffmpegEncoderUser,
				_ffmpegEncoderPassword,
				body,
				"application/json" // contentType
			);

            {
                string field = "error";
                if (JSONUtils::isMetadataPresent(encodeContentResponse, field))
                {
                    string error = JSONUtils::asString(encodeContentResponse, field, "");

					string errorMessage = string("FFMPEGEncoder error")
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", error: " + error
					;
					_logger->error(__FILEREF__ + errorMessage);

					throw runtime_error(errorMessage);
                }
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

			pair<string, bool> encoderDetails =
				_mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
			tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
			_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

			// we have to reset _encodingItem->_encoderKey because in case we will come back
			// in the above 'while' loop, we have to select another encoder
			_encodingItem->_encoderKey	= -1;

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
			_currentUsedFFMpegEncoderKey, "");	// stagingEncodedAssetPathName);

		// int maxConsecutiveEncodingStatusFailures = 1;
		bool killedByUser = waitingEncoding(maxConsecutiveEncodingStatusFailures);

		chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

		_logger->info(__FILEREF__ + "Encoded media file"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
			+ ", @MMS statistics@ - encodingDuration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count()) + "@"
			+ ", _intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
		);

		return killedByUser;
	}
	catch (EncoderNotFound e)
	{
		_logger->error(__FILEREF__ + "Encoder not found"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch(MaxConcurrentJobsReached e)
	{
		string errorMessage = string("MaxConcurrentJobsReached")
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
			+ ", e.what(): " + e.what()
		;
		_logger->warn(__FILEREF__ + errorMessage);

		throw e;
	}
	catch (runtime_error e)
	{
		string error = e.what();
		if (error.find(NoEncodingAvailable().what()) != string::npos)
		{
			string errorMessage = string("No Encodings available / MaxConcurrentJobsReached")
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", error: " + error 
			;
			_logger->warn(__FILEREF__ + errorMessage);

			throw MaxConcurrentJobsReached();
		}
		else
		{
			_logger->error(__FILEREF__ + "Encoding URL failed (runtime_error)"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
				+ ", exception: " + e.what()
			);

			throw e;
		}
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Encoding URL failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", ffmpegEncoderURL: " + ffmpegEncoderURL 
			+ ", exception: " + e.what()
		);

		throw e;
	}
}

void EncoderVideoAudioProxy::processEncodedContentVideoAudio()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processEncodedContentVideoAudio has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	Json::Value sourcesToBeEncodedRoot;
	Json::Value sourceToBeEncodedRoot;
	string encodedNFSStagingAssetPathName;
	Json::Value encodingProfileDetailsRoot;
    int64_t sourceMediaItemKey;
    int64_t encodingProfileKey;    
	string sourceRelativePath;
	int64_t physicalItemRetentionInMinutes = -1;
	try
    {
		sourcesToBeEncodedRoot = _encodingItem->_encodingParametersRoot["sourcesToBeEncoded"];
		if (sourcesToBeEncodedRoot.size() == 0)
		{
			string errorMessage = __FILEREF__ + "No sourceToBeEncoded found"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		sourceToBeEncodedRoot = sourcesToBeEncodedRoot[0];


		encodedNFSStagingAssetPathName
			= JSONUtils::asString(sourceToBeEncodedRoot, "encodedNFSStagingAssetPathName", "");
		if (encodedNFSStagingAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		encodingProfileDetailsRoot = _encodingItem->_encodingParametersRoot["encodingProfileDetails"];

        string field = "sourceRelativePath";
        sourceRelativePath = JSONUtils::asString(sourceToBeEncodedRoot, field, "");

        field = "sourceMediaItemKey";
        sourceMediaItemKey = JSONUtils::asInt64(sourceToBeEncodedRoot, field, 0);

		field = "encodingProfileKey";
		encodingProfileKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot,
			field, -1);

		field = "PhysicalItemRetention";
		if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
		{
			string retention = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, field, "1d");
			physicalItemRetentionInMinutes = MMSEngineDBFacade::parseRetention(retention);
		}
    }
	catch (runtime_error e)
	{
		_logger->error(__FILEREF__ + "Initialization encoding variables error"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", exception: " + e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(__FILEREF__ + "Initialization encoding variables error"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", exception: " + e.what()
		);

		throw e;
	}

	string fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "FileFormat", "");
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
            + ", fileFormatLowerCase: " + fileFormatLowerCase
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
        );
		bool isMMSAssetPathName = true;
        FFMpeg ffmpeg (_configuration, _logger);
		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(_encodingItem->_ingestionJobKey,
				isMMSAssetPathName, encodedNFSStagingAssetPathName + "/" + manifestFileName,
				videoTracks, audioTracks);
		}
		else
		{
			mediaInfoDetails = ffmpeg.getMediaInfo(_encodingItem->_ingestionJobKey,
				isMMSAssetPathName, encodedNFSStagingAssetPathName,
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
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName =
						encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", sourceRelativePath: " + sourceRelativePath
        );

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
	FileIO::DirectoryEntryType_t sourceFileType;
    try
    {
        size_t fileNameIndex = encodedNFSStagingAssetPathName.find_last_of("/");
        if (fileNameIndex == string::npos)
        {
            string errorMessage = __FILEREF__ + "No fileName find in the asset path name"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName;
            _logger->error(errorMessage);

            throw runtime_error(errorMessage);
        }

		encodedFileName = encodedNFSStagingAssetPathName.substr(fileNameIndex + 1);

        bool deliveryRepositoriesToo = true;

        mmsAssetPathName = _mmsStorage->moveAssetInMMSRepository(
			_encodingItem->_ingestionJobKey,
            encodedNFSStagingAssetPathName,
            _encodingItem->_workspace->_directoryName,
            encodedFileName,
            sourceRelativePath,

            &mmsPartitionIndexUsed, // OUT
			&sourceFileType,

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
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", sourceRelativePath: " + sourceRelativePath
            + ", e.what(): " + e.what()
        );

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", sourceRelativePath: " + sourceRelativePath
        );

		if (encodedNFSStagingAssetPathName != "")
		{
			string directoryPathName;
			try
			{
				size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
				if (endOfDirectoryIndex != string::npos)
				{
					directoryPathName = encodedNFSStagingAssetPathName.substr(0, endOfDirectoryIndex);

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
			size_t endOfDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
			if (endOfDirectoryIndex != string::npos)
			{
				directoryPathName = encodedNFSStagingAssetPathName.substr(0,
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
				+ ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
				+ ", directoryPathName: " + directoryPathName
				+ ", exception: " + e.what()
			);
		}
	}

    try
    {
        unsigned long long mmsAssetSizeInBytes;
        {
            if (sourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
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

		string newSourceRelativePath = sourceRelativePath;

		if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
		{
			size_t segmentsDirectoryIndex = encodedNFSStagingAssetPathName.find_last_of("/");
			if (segmentsDirectoryIndex == string::npos)
			{
				string errorMessage = __FILEREF__ + "No segmentsDirectory find in the asset path name"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName;
				_logger->error(errorMessage);

	            throw runtime_error(errorMessage);
			}

			// in case of MPEG2_TS next 'stagingEncodedAssetPathName.substr' extract the directory name
			// containing manifest and ts files. So relativePath has to be extended with this directory
			newSourceRelativePath += (encodedNFSStagingAssetPathName.substr(segmentsDirectoryIndex + 1) + "/");

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
            sourceMediaItemKey,
			externalReadOnlyStorage,
			externalDeliveryTechnology,
			externalDeliveryURL,
            encodedFileName,
            newSourceRelativePath,
            mmsPartitionIndexUsed,
            mmsAssetSizeInBytes,
            encodingProfileKey,
			physicalItemRetentionInMinutes,
                
			mediaInfoDetails,
			videoTracks,
			audioTracks,

            imageWidth,
            imageHeight,
            imageFormat,
            imageQuality
                );
        
        _logger->info(__FILEREF__ + "Saved the Encoded content"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", encodedPhysicalPathKey: " + to_string(encodedPhysicalPathKey)
            + ", newSourceRelativePath: " + newSourceRelativePath
            + ", encodedFileName: " + encodedFileName
        );
    }
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "_mmsEngineDBFacade->saveVariantContentMetadata failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
			+ ", e.what(): " + e.what()
        );

		// file in case of .3gp content OR directory in case of IPhone content
		if (sourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
		{
			if (FileIO::directoryExisting(mmsAssetPathName))
			{
				_logger->info(__FILEREF__ + "removeDirectory"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", mmsAssetPathName: " + mmsAssetPathName
				);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
			}
		}
		else if (sourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
		{
			if (FileIO::fileExisting(mmsAssetPathName))
			{
				_logger->info(__FILEREF__ + "remove"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
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
            + ", encodedNFSStagingAssetPathName: " + encodedNFSStagingAssetPathName
        );

		// file in case of .3gp content OR directory in case of IPhone content
		if (sourceFileType == FileIO::TOOLS_FILEIO_DIRECTORY)
		{
			if (FileIO::directoryExisting(mmsAssetPathName))
			{
				_logger->info(__FILEREF__ + "removeDirectory"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", mmsAssetPathName: " + mmsAssetPathName
				);
				Boolean_t bRemoveRecursively = true;
				FileIO::removeDirectory(mmsAssetPathName, bRemoveRecursively);
			}
		}
		else if (sourceFileType == FileIO::TOOLS_FILEIO_REGULARFILE) 
		{
			if (FileIO::fileExisting(mmsAssetPathName))
			{
				_logger->info(__FILEREF__ + "remove"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", mmsAssetPathName: " + mmsAssetPathName
				);
				FileIO::remove(mmsAssetPathName);
			}
		}

        throw e;
    }

}

void EncoderVideoAudioProxy::processOverlayedImageOnVideo(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processOverlayedImageOnVideo has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
}

void EncoderVideoAudioProxy::processOverlayedTextOnVideo(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processOverlayedTextOnVideo has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

void EncoderVideoAudioProxy::processVideoSpeed(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processVideoSpeed has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

void EncoderVideoAudioProxy::processPictureInPicture(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processPictureInPicture has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
}

void EncoderVideoAudioProxy::processIntroOutroOverlay()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processIntroOutroOverlay has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
        _logger->error(__FILEREF__ + "processIntroOutroOverlay failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processIntroOutroOverlay failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
}

void EncoderVideoAudioProxy::processCutFrameAccurate()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processCutFrameAccurate has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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

		string field = "sourceVideoMediaItemKey";
        int64_t sourceVideoMediaItemKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "newUtcStartTimeInMilliSecs";
        int64_t newUtcStartTimeInMilliSecs = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		field = "newUtcEndTimeInMilliSecs";
        int64_t newUtcEndTimeInMilliSecs = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

		if (newUtcStartTimeInMilliSecs != -1 && newUtcEndTimeInMilliSecs != -1)
		{
			Json::Value destUserDataRoot;

			field = "UserData";
			if (JSONUtils::isMetadataPresent(_encodingItem->_ingestedParametersRoot, field))
				destUserDataRoot = _encodingItem->_ingestedParametersRoot[field];

			Json::Value destMmsDataRoot;

			field = "mmsData";
			if (JSONUtils::isMetadataPresent(destUserDataRoot, field))
				destMmsDataRoot = destUserDataRoot[field];

			field = "utcStartTimeInMilliSecs";
			if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
				destMmsDataRoot.removeMember(field);
			destMmsDataRoot[field] = newUtcStartTimeInMilliSecs;

			field = "utcEndTimeInMilliSecs";
			if (JSONUtils::isMetadataPresent(destMmsDataRoot, field))
				destMmsDataRoot.removeMember(field);
			destMmsDataRoot[field] = newUtcEndTimeInMilliSecs;

			field = "mmsData";
			destUserDataRoot[field] = destMmsDataRoot;

			field = "UserData";
			_encodingItem->_ingestedParametersRoot[field] = destUserDataRoot;
		}

		int64_t faceOfVideoMediaItemKey = -1;
        string mediaMetaDataContent = generateMediaMetadataToIngest(_encodingItem->_ingestionJobKey,
            fileFormat, faceOfVideoMediaItemKey,
			sourceVideoMediaItemKey, newUtcStartTimeInMilliSecs / 1000, newUtcEndTimeInMilliSecs / 1000,
			_encodingItem->_ingestedParametersRoot);
    
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
        _logger->error(__FILEREF__ + "processIntroOutroOverlay failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
            + ", e.what(): " + e.what()
        );
                
        throw e;
    }
    catch(exception e)
    {
        _logger->error(__FILEREF__ + "processIntroOutroOverlay failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
       throw e;
    }
}

void EncoderVideoAudioProxy::processGeneratedFrames(bool killedByUser)
{
	if (_currentUsedFFMpegExternalEncoder)
	{
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
		multiLocalAssetIngestionEvent->setParametersRoot(_encodingItem->_ingestedParametersRoot);

		shared_ptr<Event2>    event = dynamic_pointer_cast<Event2>(multiLocalAssetIngestionEvent);
		_multiEventsSet->addEvent(event);

		_logger->info(__FILEREF__ + "addEvent: EVENT_TYPE (MULTIINGESTASSETEVENT)"
			+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", getEventKey().first: " + to_string(event->getEventKey().first)
			+ ", getEventKey().second: " + to_string(event->getEventKey().second));
	}
}

void EncoderVideoAudioProxy::processSlideShow()
{
	if (_currentUsedFFMpegExternalEncoder)
	{
        _logger->info(__FILEREF__ + "The encoder selected is external, processSlideShow has nothing to do"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _currentUsedFFMpegExternalEncoder: " + to_string(_currentUsedFFMpegExternalEncoder)
        );

		return;
	}

	string stagingEncodedAssetPathName;
	try
    {
		stagingEncodedAssetPathName = JSONUtils::asString(_encodingItem->_encodingParametersRoot,
			"encodedNFSStagingAssetPathName", "");
		if (stagingEncodedAssetPathName == "")
		{
			string errorMessage = __FILEREF__ + "encodedNFSStagingAssetPathName cannot be empty"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
		int outputFrameRate;  
		string field = "outputFrameRate";
		outputFrameRate = JSONUtils::asInt(_encodingItem->_encodingParametersRoot, field, 25);

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
		faceRecognitionCascadeName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		field = "sourcePhysicalPath";
		sourcePhysicalPath = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

		// VideoWithHighlightedFaces, ImagesToBeUsedInDeepLearnedModel or FrameContainingFace
		field = "faceRecognitionOutput";
		faceRecognitionOutput = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

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

	// sometimes the file was created by another MMSEngine and it is not found
	// just because of nfs delay. For this reason we implemented a retry mechanism
	if (!FileIO::fileExisting(sourcePhysicalPath,
		_waitingNFSSync_maxMillisecondsToWait, _waitingNFSSync_milliSecondsWaitingBetweenChecks))
	{
		string errorMessage = __FILEREF__ + "Media Source file does not exist"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	cv::VideoCapture capture;
	capture.open(sourcePhysicalPath, cv::CAP_FFMPEG);
	if (!capture.isOpened())
	{
		string errorMessage = __FILEREF__ + "Capture could not be opened"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

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
							-1, -1, -1, // cutOfVideoMediaItemKey
							_encodingItem->_ingestedParametersRoot);
    
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
					-1, -1, -1, // cutOfVideoMediaItemKey
					_encodingItem->_ingestedParametersRoot);
  
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
				-1, -1, -1, // cutOfVideoMediaItemKey
				_encodingItem->_ingestedParametersRoot);
  
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
			faceRecognitionOutput = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
		stringstream ssDeepLearnedModelTagsCommaSeparated (JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, ""));
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
		// bool startAndEndIngestionDatePresent = false;
		string startIngestionDate;
		string endIngestionDate;
		string title;
		int liveRecordingChunk = -1;
		int64 deliveryCode = -1;
		int64_t utcCutPeriodStartTimeInMilliSeconds = -1;
		int64_t utcCutPeriodEndTimeInMilliSecondsPlusOneSecond = -1;
		string jsonCondition;
		string orderBy;
		string jsonOrderBy;
		Json::Value responseFields = Json::nullValue;
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
				// startAndEndIngestionDatePresent,
				startIngestionDate, endIngestionDate,
				title, liveRecordingChunk,
				deliveryCode,
				utcCutPeriodStartTimeInMilliSeconds, utcCutPeriodEndTimeInMilliSecondsPlusOneSecond,
				jsonCondition,
				deepLearnedModelTags, tagsNotIn, orderBy, jsonOrderBy,
				responseFields, admin,
				// 2022-12-18: MIKs dovrebbero essere stati aggiunti da un po
				false);

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
				string tags = JSONUtils::asString(mediaItemRoot, field, "");
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
					_mmsStorage->getPhysicalPathDetails(physicalPathKey,
						// 2022-12-18: MIK dovrebbe essere aggiunto da un po
						false);
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
		faceIdentificationCascadeName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, 0);

		field = "sourcePhysicalPath";
		sourcePhysicalPath = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
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

	// sometimes the file was created by another MMSEngine and it is not found
	// just because of nfs delay. For this reason we implemented a retry mechanism
	if (!FileIO::fileExisting(sourcePhysicalPath,
		_waitingNFSSync_maxMillisecondsToWait, _waitingNFSSync_milliSecondsWaitingBetweenChecks))
	{
		string errorMessage = __FILEREF__ + "Media Source file does not exist"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	cv::VideoCapture capture;
	capture.open(sourcePhysicalPath, cv::CAP_FFMPEG);
	if (!capture.isOpened())
	{
		string errorMessage = __FILEREF__ + "Capture could not be opened"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
			+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
			+ ", sourcePhysicalPath: " + sourcePhysicalPath;
		_logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

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
			-1, -1, -1, // cutOfVideoMediaItemKey
			_encodingItem->_ingestedParametersRoot);
    
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
            + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

bool EncoderVideoAudioProxy::liveRecorder()
{

	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		string field = "schedule";
		Json::Value recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);

		string segmenterType = "hlsSegmenter";
		// string segmenterType = "streamSegmenter";
		if (segmenterType == "streamSegmenter")
		{
			// since the first chunk is discarded, we will start recording before the period of the chunk
			// 2021-07-09: commented because we do not have monitorVirtualVODSegmentDurationInSeconds anymore
			//	(since it is inside outputsRoot)
			// utcRecordingPeriodStart -= monitorVirtualVODSegmentDurationInSeconds;
		}
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

	{
		string field = "outputsRoot";
		Json::Value outputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		bool killedByUser = false;
		try
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string awsChannelConfigurationLabel
						= JSONUtils::asString(outputRoot, "awsChannelConfigurationLabel", "");
					bool awsSignedURL = JSONUtils::asBool(outputRoot,
						"awsSignedURL", false);
					int awsExpirationInMinutes = JSONUtils::asInt(outputRoot,
						"awsExpirationInMinutes", 1440);

					string awsChannelType;
					if (awsChannelConfigurationLabel == "")
						awsChannelType = "SHARED";
					else
						awsChannelType = "DEDICATED";

					// reserveAWSChannel ritorna exception se non ci sono piu canali
					// liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta
					// già attiva, ritornerebbe le stesse info associate
					// a ingestionJobKey (senza exception)
					tuple<string, string, string, bool> awsChannelDetails
						= _mmsEngineDBFacade->reserveAWSChannel(
							_encodingItem->_workspace->_workspaceKey,
							awsChannelConfigurationLabel, awsChannelType,
							_encodingItem->_ingestionJobKey);

					string awsChannelId;
					string rtmpURL;
					string playURL;
					bool channelAlreadyReserved;
					tie(awsChannelId, rtmpURL, playURL,
						channelAlreadyReserved) = awsChannelDetails;

					if (awsSignedURL)
					{
						try
						{
							playURL = getAWSSignedURL(playURL, awsExpirationInMinutes);
						}
						catch(exception ex)
						{
							_logger->error(__FILEREF__
								+ "getAWSSignedURL failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", playURL: " + playURL
							);

							// throw e;
						}
					}

					// update outputsRoot with the new details
					{
						field = "awsChannelConfigurationLabel";
						outputRoot[field] = awsChannelConfigurationLabel;

						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						field = "playUrl";
						outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							// string encodingParameters = JSONUtils::toString(
							// 	_encodingItem->_encodingParametersRoot);

							_logger->info(__FILEREF__ + "updateOutputRtmpAndPlaURL"
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", workspaceKey: "
									+ to_string(_encodingItem->_workspace->_workspaceKey) 
								+ ", ingestionJobKey: "
									+ to_string(_encodingItem->_ingestionJobKey) 
								+ ", encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey) 
								+ ", awsChannelConfigurationLabel: "
									+ awsChannelConfigurationLabel 
								+ ", awsChannelType: " + awsChannelType 
								+ ", awsChannelId: " + awsChannelId 
								+ ", rtmpURL: " + rtmpURL 
								+ ", playURL: " + playURL 
								+ ", channelAlreadyReserved: "
									+ to_string(channelAlreadyReserved) 
								// + ", encodingParameters: " + encodingParameters 
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL (
								_encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey,
								outputIndex, rtmpURL, playURL);
							// _mmsEngineDBFacade->updateEncodingJobParameters(
							// 	_encodingItem->_encodingJobKey,
							// 	encodingParameters);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__
								+ "updateEncodingJobParameters failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__
								+ "updateEncodingJobParameters failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);

							// throw e;
						}
					}

					// channelAlreadyReserved true means the channel was already reserved,
					// so it is supposed is already started
					// Maybe just start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
						awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
				}
			}

			killedByUser = liveRecorder_through_ffmpeg();
			if (killedByUser)	// KilledByUser
			{
				string errorMessage = __FILEREF__ + "Encoding killed by the User"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
					;
				_logger->warn(errorMessage);
        
				throw EncodingKilledByUser();
			}

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
							_encodingItem->_workspace->_workspaceKey,
							_encodingItem->_ingestionJobKey);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch(...)
					{
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
							;
						_logger->error(errorMessage);
					}
				}
			}
		}
		catch(...)
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
								_encodingItem->_workspace->_workspaceKey,
								_encodingItem->_ingestionJobKey);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch(...)
					{
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
							;
						_logger->error(errorMessage);
					}
				}
			}

			// throw the same received exception
			throw;
		}

		return killedByUser;
	}
}

bool EncoderVideoAudioProxy::liveRecorder_through_ffmpeg()
{
	bool exitInCaseOfUrlNotFoundOrForbidden;
	string streamSourceType;
	int64_t pushEncoderKey;
	string pushServerName;
	string encodersPool;
	{
        string field = "exitInCaseOfUrlNotFoundOrForbidden";
        exitInCaseOfUrlNotFoundOrForbidden = JSONUtils::asBool(_encodingItem->_ingestedParametersRoot,
			field, false);

		field = "pushEncoderKey";
		pushEncoderKey = JSONUtils::asInt64(_encodingItem->_encodingParametersRoot, field, -1);

        field = "pushServerName";
        pushServerName = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

        field = "streamSourceType";
        streamSourceType = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");

        field = "encodersPoolLabel";
        encodersPool = JSONUtils::asString(_encodingItem->_encodingParametersRoot, field, "");
	}

	time_t utcRecordingPeriodStart;
	time_t utcRecordingPeriodEnd;
	bool autoRenew;
	{
		string field = "schedule";
		Json::Value recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)[field];

		field = "start";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodStart = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodStart = DateTime::sDateSecondsToUtc(recordingPeriodStart);

		field = "end";
		if (!JSONUtils::isMetadataPresent(recordingPeriodRoot, field))
		{
			string errorMessage = __FILEREF__ + "Field is not present or it is null"
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
				+ ", Field: " + field;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		string recordingPeriodEnd = JSONUtils::asString(recordingPeriodRoot, field, "");
		utcRecordingPeriodEnd = DateTime::sDateSecondsToUtc(recordingPeriodEnd);

		field = "autoRenew";
		autoRenew = JSONUtils::asBool(recordingPeriodRoot, field, false);
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	time_t utcNowToCheckExit = 0;
	while (!killedByUser // && !urlForbidden && !urlNotFound
		&& utcNowToCheckExit < utcRecordingPeriodEnd)
	{
		if (urlForbidden || urlNotFound)
		{
			if (exitInCaseOfUrlNotFoundOrForbidden)
			{
				_logger->warn(__FILEREF__ + "url not found or forbidden, terminate the LiveRecorder task"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", exitInCaseOfUrlNotFoundOrForbidden: " + to_string(exitInCaseOfUrlNotFoundOrForbidden)
					+ ", urlForbidden: " + to_string(urlForbidden)
					+ ", urlNotFound: " + to_string(urlNotFound)
				);

				break;
			}
			else
			{
				int waitingInSeconsBeforeTryingAgain = 30;

				_logger->warn(__FILEREF__ + "url not found or forbidden, wait a bit and try again"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", exitInCaseOfUrlNotFoundOrForbidden: " + to_string(exitInCaseOfUrlNotFoundOrForbidden)
					+ ", urlForbidden: " + to_string(urlForbidden)
					+ ", urlNotFound: " + to_string(urlNotFound)
					+ ", waitingInSeconsBeforeTryingAgain: " + to_string(waitingInSeconsBeforeTryingAgain)
				);

				this_thread::sleep_for(chrono::seconds(waitingInSeconsBeforeTryingAgain));
			}
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveRecorderURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				if (streamSourceType == "IP_PUSH" && pushEncoderKey != -1)
				{
					_currentUsedFFMpegEncoderKey = pushEncoderKey;
					pair<string, bool> encoderDetails =
						_mmsEngineDBFacade->getEncoderURL(pushEncoderKey, pushServerName);
					tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				}
				else
				{
					_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						);

					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tuple<int64_t, string, bool> encoderDetails =
						_encodersLoadBalancer->getEncoderURL(
							_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace,
							encoderKeyToBeSkipped, externalEncoderAllowed);
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost,
						_currentUsedFFMpegExternalEncoder) = encoderDetails;
				}

				_logger->info(__FILEREF__ + "LiveRecorder. Selection of the transcoder"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
                    + ffmpegURI
                    + "/" + to_string(_encodingItem->_encodingJobKey)
				;

				string body;
				{
					Json::Value liveRecorderMedatada;

					liveRecorderMedatada["ingestionJobKey"] = (Json::LargestUInt) (
						_encodingItem->_ingestionJobKey);
					liveRecorderMedatada["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					liveRecorderMedatada["encodingParametersRoot"] =
						_encodingItem->_encodingParametersRoot;
					liveRecorderMedatada["ingestedParametersRoot"] =
						_encodingItem->_ingestedParametersRoot;

					body = JSONUtils::toString(liveRecorderMedatada);
				}

				Json::Value liveRecorderContentResponse = MMSCURL::httpPostStringAndGetJson(
					_logger,
					_encodingItem->_ingestionJobKey,
					ffmpegEncoderURL,
					_ffmpegEncoderTimeoutInSeconds,
					_ffmpegEncoderUser,
					_ffmpegEncoderPassword,
					body,
					"application/json" // contentType
				);

				{
					string field = "error";
					if (JSONUtils::isMetadataPresent(liveRecorderContentResponse, field))
					{
						string error = JSONUtils::asString(liveRecorderContentResponse, field, "");
                    
						string errorMessage = string("FFMPEGEncoder error")
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", error: " + error
                        ;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
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

				pair<string, bool> encoderDetails =
					_mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

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
							string firstLineOfEncodingErrorMessage;
							{
								string firstLine;
								stringstream ss(encodingErrorMessage);
								if (getline(ss, firstLine))
									firstLineOfEncodingErrorMessage = firstLine;
								else
									firstLineOfEncodingErrorMessage = encodingErrorMessage;
							}

							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
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
						string errorMessage = __FILEREF__
							+ "Encoding failed (look the Transcoder logs)"             
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", _currentUsedFFMpegEncoderHost: "
								+ _currentUsedFFMpegEncoderHost
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

							bool isKilled =
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
									_encodingItem->_encodingJobKey, 
									encodingStatusFailures);
							if (isKilled)
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
								+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
								+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
								+ ", encodingProgress: " + to_string(encodingProgress)
								+ ", utcNow: " + to_string(utcNow)
								+ ", utcRecordingPeriodStart: " + to_string(utcRecordingPeriodStart)
								+ ", utcRecordingPeriodEnd: " + to_string(utcRecordingPeriodEnd)
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
					/* 2022-09-03: Since we introdiced the exitInCaseOfUrlNotFoundOrForbidden flag,
						I guess the below urlNotFound management is not needed anymore

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
					*/
					{
						_logger->info(__FILEREF__ + "it is not a fake urlNotFound"
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", urlNotFound: " + to_string(urlNotFound)
							// + ", @MMS statistics@ - encodingDurationInMinutes: @" + to_string(encodingDurationInMinutes) + "@"
							// + ", urlNotFoundFakeAfterMinutes: " + to_string(urlNotFoundFakeAfterMinutes)
							+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
							+ ", maxEncodingStatusFailures: " + to_string(maxEncodingStatusFailures)
						);
					}

					// already incremented in above in if (completedWithError)
                    // encodingStatusFailures++;

					_logger->error(__FILEREF__ + "getEcodingStatus failed"
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
            
			utcNowToCheckExit = chrono::system_clock::to_time_t(endEncoding);

			if (utcNowToCheckExit < utcRecordingPeriodEnd)
			{
				_logger->error(__FILEREF__ + "LiveRecorder media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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

					utcNowToCheckExit			= 0;

					// let's select again the encoder
					_encodingItem->_encoderKey	= -1;

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
					// 2022-11-09: I do not call anymore getEncodingProgress
					// 2022-11-20: next update is mandatory otherwise we will have the folloging error:
					//		FFMpeg.cpp:8679: LiveRecorder timing. Too late to start the LiveRecorder
					{
						string field = "utcScheduleStart";
						_encodingItem->_encodingParametersRoot[field] = utcRecordingPeriodStart;

						field = "utcScheduleEnd";
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
        catch (runtime_error e)
        {
			string error = e.what();
			if (error.find(NoEncodingAvailable().what()) != string::npos)
			{
				string errorMessage = string("No Encodings available / MaxConcurrentJobsReached")
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
					+ ", error: " + error 
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw MaxConcurrentJobsReached();
			}
			else
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
	// if (main)
	{
		if (urlForbidden)
		{
			string errorMessage = __FILEREF__ + "LiveRecorder: URL forbidden"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
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
				+ ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
			;
			_logger->error(errorMessage);

			throw FFMpegURLNotFound();
		}
		else
		{
			long ingestionJobOutputsCount = _mmsEngineDBFacade->getIngestionJobOutputsCount(
				_encodingItem->_ingestionJobKey,
				// 2022-12-18: true because IngestionJobOutputs was updated recently
				true);
			if (ingestionJobOutputsCount <= 0)
			{
				string errorMessage = __FILEREF__ + "LiveRecorder: no chunks were generated"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
					+ ", ingestionJobOutputsCount: " + to_string(ingestionJobOutputsCount)
				;
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}

    // return make_tuple(killedByUser, main);
    return killedByUser;
}

void EncoderVideoAudioProxy::processLiveRecorder(bool killedByUser)
{
    try
    {
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
			_mmsEngineDBFacade->updateIngestionJob(
				_encodingItem->_ingestionJobKey, newIngestionStatus,
				errorMessage);
		}
	}
    catch(runtime_error e)
    {
        _logger->error(__FILEREF__ + "processLiveRecorder failed"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
            + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
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
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

bool EncoderVideoAudioProxy::liveProxy(string proxyType)
{

	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	{
		string field = "inputsRoot";
		Json::Value inputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		if (inputsRoot == Json::nullValue || inputsRoot.size() == 0)
		{
			string errorMessage = __FILEREF__ + "No inputsRoot are present"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", inputsRoot.size: " + to_string(inputsRoot.size())
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}

		{
			Json::Value firstInputRoot = inputsRoot[0];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(firstInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleStart";
				utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, field, -1);
			}

			Json::Value lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(lastInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleEnd";
				utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, field, -1);
			}
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
			if (utcProxyPeriodStart - utcNow >=
				_timeBeforeToPrepareResourcesInMinutes * 60)                  
			{
				_logger->info(__FILEREF__ + "Too early to allocate a thread for proxing"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", utcProyPeriodStart - utcNow: " + to_string(
						utcProxyPeriodStart - utcNow)        
					+ ", _timeBeforeToPrepareResourcesInSeconds: "
						+ to_string(_timeBeforeToPrepareResourcesInMinutes * 60)
				);

				// it is simulated a MaxConcurrentJobsReached to avoid
				// to increase the error counter              
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

	{
		string field = "outputsRoot";
		Json::Value outputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		bool killedByUser = false;
		try
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					// RtmpUrl and PlayUrl fields have to be initialized

					string awsChannelConfigurationLabel
						= JSONUtils::asString(outputRoot, "awsChannelConfigurationLabel", "");
					bool awsSignedURL = JSONUtils::asBool(outputRoot,
						"awsSignedURL", false);
					int awsExpirationInMinutes = JSONUtils::asInt(outputRoot,
						"awsExpirationInMinutes", 1440);

					string awsChannelType;
					if (awsChannelConfigurationLabel == "")
						awsChannelType = "SHARED";
					else
						awsChannelType = "DEDICATED";

					// reserveAWSChannel ritorna exception se non ci sono piu canali
					// liberi o quello dedicato è già occupato
					// In caso di ripartenza di mmsEngine, nel caso di richiesta
					// già attiva, ritornerebbe le stesse info associate
					// a ingestionJobKey (senza exception)
					tuple<string, string, string, bool> awsChannelDetails
						= _mmsEngineDBFacade->reserveAWSChannel(
							_encodingItem->_workspace->_workspaceKey,
							awsChannelConfigurationLabel, awsChannelType,
							_encodingItem->_ingestionJobKey);

					string awsChannelId;
					string rtmpURL;
					string playURL;
					bool channelAlreadyReserved;
					tie(awsChannelId, rtmpURL, playURL,
						channelAlreadyReserved) = awsChannelDetails;

					if (awsSignedURL)
					{
						try
						{
							playURL = getAWSSignedURL(playURL, awsExpirationInMinutes);
						}
						catch(exception ex)
						{
							_logger->error(__FILEREF__
								+ "getAWSSignedURL failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", playURL: " + playURL
							);

							// throw e;
						}
					}

					// update outputsRoot with the new details
					{
						field = "awsChannelConfigurationLabel";
						outputRoot[field] = awsChannelConfigurationLabel;

						field = "rtmpUrl";
						outputRoot[field] = rtmpURL;

						field = "playUrl";
						outputRoot[field] = playURL;

						outputsRoot[outputIndex] = outputRoot;

						field = "outputsRoot";
						(_encodingItem->_encodingParametersRoot)[field] = outputsRoot;

						try
						{
							// string encodingParameters = JSONUtils::toString(
							// 	_encodingItem->_encodingParametersRoot);

							_logger->info(__FILEREF__ + "updateOutputRtmpAndPlaURL"
								+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
								+ ", workspaceKey: "
									+ to_string(_encodingItem->_workspace->_workspaceKey) 
								+ ", ingestionJobKey: "
									+ to_string(_encodingItem->_ingestionJobKey) 
								+ ", encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey) 
								+ ", awsChannelConfigurationLabel: "
									+ awsChannelConfigurationLabel 
								+ ", awsChannelType: " + awsChannelType 
								+ ", awsChannelId: " + awsChannelId 
								+ ", rtmpURL: " + rtmpURL 
								+ ", playURL: " + playURL 
								+ ", channelAlreadyReserved: "
									+ to_string(channelAlreadyReserved) 
								// + ", encodingParameters: " + encodingParameters 
							);

							_mmsEngineDBFacade->updateOutputRtmpAndPlaURL (
								_encodingItem->_ingestionJobKey,
								_encodingItem->_encodingJobKey,
								outputIndex, rtmpURL, playURL);
							// _mmsEngineDBFacade->updateEncodingJobParameters(
							// 	_encodingItem->_encodingJobKey,
							// 	encodingParameters);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__
								+ "updateEncodingJobParameters failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);

							// throw e;
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__
								+ "updateEncodingJobParameters failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);

							// throw e;
						}
					}

					// channelAlreadyReserved true means the channel was already reserved,
					// so it is supposed is already started
					// Maybe just start again is not an issue!!! Let's see
					if (!channelAlreadyReserved)
						awsStartChannel(_encodingItem->_ingestionJobKey, awsChannelId);
				}
			}

			killedByUser = liveProxy_through_ffmpeg(proxyType);
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

			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
								_encodingItem->_workspace->_workspaceKey,
								_encodingItem->_ingestionJobKey);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch(...)
					{
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
							;
						_logger->error(errorMessage);
					}
				}
			}
		}
		catch(...)
		{
			for(int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
			{
				Json::Value outputRoot = outputsRoot[outputIndex];

				string outputType = JSONUtils::asString(outputRoot, "outputType", "");

				if (outputType == "AWS_CHANNEL")
				{
					try
					{
						// error in case do not find ingestionJobKey
						string awsChannelId = _mmsEngineDBFacade->releaseAWSChannel(
								_encodingItem->_workspace->_workspaceKey,
								_encodingItem->_ingestionJobKey);

						awsStopChannel(_encodingItem->_ingestionJobKey, awsChannelId);
					}
					catch(...)
					{
						string errorMessage = __FILEREF__ + "releaseAWSChannel failed"
							+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
							;
						_logger->error(errorMessage);
					}
				}
			}

			// throw the same received exception
			throw;
		}

		return killedByUser;
	}
}

bool EncoderVideoAudioProxy::liveProxy_through_ffmpeg(string proxyType)
{

	string encodersPool;
	int64_t liveURLConfKey = -1;
	string liveURL;
	long waitingSecondsBetweenAttemptsInCaseOfErrors;
	long maxAttemptsNumberInCaseOfErrors;
	bool timePeriod = false;
	time_t utcProxyPeriodStart = -1;
	time_t utcProxyPeriodEnd = -1;
	string streamSourceType;
	int64_t pushEncoderKey;
	string pushServerName;
	{
		string field = "inputsRoot";
		Json::Value inputsRoot = (_encodingItem->_encodingParametersRoot)[field];

		Json::Value firstInputRoot = inputsRoot[0];

		if (proxyType == "vodProxy")
			field = "vodInput";
		else if (proxyType == "liveProxy")
			field = "streamInput";
		else if (proxyType == "countdownProxy")
			field = "countdownInput";
		Json::Value streamInputRoot = firstInputRoot[field];

		if (proxyType == "vodProxy" || proxyType == "countdownProxy")
		{
			// both vodProxy and countdownProxy work with VOD
			field = "EncodersPool";
			encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, field, "");
		}
		else
		{
			field = "encodersPoolLabel";
			encodersPool = JSONUtils::asString(streamInputRoot, field, "");

	        field = "confKey";
			liveURLConfKey = JSONUtils::asInt64(streamInputRoot, field, 0);

			field = "url";
			liveURL = JSONUtils::asString(streamInputRoot, field, "");

			field = "pushEncoderKey";
			pushEncoderKey = JSONUtils::asInt64(streamInputRoot, field, -1);

			field = "pushServerName";
			pushServerName = JSONUtils::asString(streamInputRoot, field, "");

			field = "streamSourceType";
			streamSourceType = JSONUtils::asString(streamInputRoot, field, "");
		}

        field = "waitingSecondsBetweenAttemptsInCaseOfErrors";
        waitingSecondsBetweenAttemptsInCaseOfErrors = JSONUtils::asInt(
			_encodingItem->_encodingParametersRoot, field, 600);

        field = "MaxAttemptsNumberInCaseOfErrors";
        maxAttemptsNumberInCaseOfErrors = JSONUtils::asInt(
			_encodingItem->_ingestedParametersRoot, field, -1);

		{
			// Json::Value firstInputRoot = inputsRoot[0];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(firstInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleStart";
				utcProxyPeriodStart = JSONUtils::asInt64(firstInputRoot, field, -1);
			}

			Json::Value lastInputRoot = inputsRoot[inputsRoot.size() - 1];

			field = "timePeriod";
			timePeriod = JSONUtils::asBool(lastInputRoot, field, false);

			if (timePeriod)
			{
				field = "utcScheduleEnd";
				utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, field, -1);
				if (utcProxyPeriodEnd == -1)
				{
					field = "utcProxyPeriodEnd";
					utcProxyPeriodEnd = JSONUtils::asInt64(lastInputRoot, field, -1);
				}
			}
		}
	}

	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;


	_logger->info(__FILEREF__ + "check maxAttemptsNumberInCaseOfErrors"
		+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
		+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		+ ", maxAttemptsNumberInCaseOfErrors: " + to_string(maxAttemptsNumberInCaseOfErrors)
	);

	long currentAttemptsNumberInCaseOfErrors = 0;

	bool alwaysRetry = false;

	// long encodingStatusFailures = 0;
	if (maxAttemptsNumberInCaseOfErrors == -1)
	{
		// 2022-07-20: -1 means we always has to retry, so we will reset encodingStatusFailures to 0
		alwaysRetry = true;

		// 2022-07-20: this is to allow the next loop to exit after 2 errors
		maxAttemptsNumberInCaseOfErrors = 2;

		// 2020-04-19: Reset encodingStatusFailures into DB.
		// That because if we comes from an error/exception
		//	encodingStatusFailures is > than 0 but we consider here like
		//	it is 0 because our variable is set to 0
		try
		{
			_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				// + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				+ ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
			);

			killedByUser = _mmsEngineDBFacade->updateEncodingJobFailuresNumber (
				_encodingItem->_encodingJobKey, 
				// encodingStatusFailures
				currentAttemptsNumberInCaseOfErrors
			);
		}
		catch(...)
		{
			_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
				+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				// + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
				+ ", currentAttemptsNumberInCaseOfErrors: " + to_string(currentAttemptsNumberInCaseOfErrors)
			);
		}
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
	// 2021-05-29: LiveProxy has to exit if:
	//	- was killed OR
	//	- if timePeriod true
	//		- no way to exit (we have to respect the timePeriod)
	//	- if timePeriod false
	//		- exit if too many error or urlForbidden or urlNotFound
	time_t utcNowCheckToExit = 0;
	// while (!killedByUser && !urlForbidden && !urlNotFound
		// check on currentAttemptsNumberInCaseOfErrors is done only if there is no timePeriod
	// 	&& (timePeriod || currentAttemptsNumberInCaseOfErrors < maxAttemptsNumberInCaseOfErrors)
	// )
	while (!	// while we are NOT in the exit condition
		(
			// exit condition 
			killedByUser ||
			(!timePeriod && (urlForbidden || urlNotFound || currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors))
		)
	)
	{
		if (timePeriod)
		{
			if (utcNowCheckToExit >= utcProxyPeriodEnd)
				break;
			else
				_logger->info(__FILEREF__ + "check to exit"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", still miss (secs): " + to_string(utcProxyPeriodEnd - utcNowCheckToExit)
				);
		}

		string ffmpegEncoderURL;
		string ffmpegURI = _ffmpegLiveProxyURI;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				if (streamSourceType == "IP_PUSH" && pushEncoderKey != -1)
				{
					_currentUsedFFMpegEncoderKey = pushEncoderKey;
					pair<string, bool> encoderDetails =
						_mmsEngineDBFacade->getEncoderURL(pushEncoderKey, pushServerName);
					tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				}
				else
				{
					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tuple<int64_t, string, bool> encoderDetails =
						_encodersLoadBalancer->getEncoderURL(
							_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace,
							encoderKeyToBeSkipped, externalEncoderAllowed);
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost,
						_currentUsedFFMpegExternalEncoder) = encoderDetails;
				}

				_logger->info(__FILEREF__ + "Configuration item"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: "
						+ to_string(_currentUsedFFMpegEncoderKey)
				);
				ffmpegEncoderURL =
					_currentUsedFFMpegEncoderHost
					+ ffmpegURI
					+ "/" + to_string(_encodingItem->_encodingJobKey)
				;

				_logger->info(__FILEREF__ +
					"LiveProxy. Selection of the transcoder. The transcoder is selected by load balancer"
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", transcoder: " + _currentUsedFFMpegEncoderHost
					+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
				);

				string body;
				{
					// 2021-12-14: we have to read again encodingParametersRoot because,
					//	in case the playlist (inputsRoot) is changed, the updated inputsRoot
					//	is into DB
					{
						try
						{
							tuple<int64_t, string, int64_t, MMSEngineDBFacade::EncodingStatus,
								string> encodingJobDetails
								= _mmsEngineDBFacade->getEncodingJobDetails(
								_encodingItem->_encodingJobKey,
								// 2022-12-18: true because the inputsRoot maybe was just updated
								true);

							string encodingParameters;

							tie(ignore, ignore, ignore, ignore, encodingParameters)
								= encodingJobDetails;

							_encodingItem->_encodingParametersRoot = JSONUtils::toJson(
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey,
								encodingParameters);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__ + "getEncodingJobDetails failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);

							throw e;
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__ + "getEncodingJobDetails failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);

							throw e;
						}
					}

					Json::Value liveProxyMetadata;

					liveProxyMetadata["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					liveProxyMetadata["ingestionJobKey"] =
						(Json::LargestUInt) (_encodingItem->_ingestionJobKey);
					liveProxyMetadata["liveURL"] = liveURL;
					liveProxyMetadata["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;
					liveProxyMetadata["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;

					body = JSONUtils::toString(liveProxyMetadata);
				}

				Json::Value liveProxyContentResponse = MMSCURL::httpPostStringAndGetJson(
					_logger,
					_encodingItem->_ingestionJobKey,
					ffmpegEncoderURL,
					_ffmpegEncoderTimeoutInSeconds,
					_ffmpegEncoderUser,
					_ffmpegEncoderPassword,
					body,
					"application/json" // contentType
				);

				{
					string field = "error";
					if (JSONUtils::isMetadataPresent(liveProxyContentResponse, field))
					{
						string error = JSONUtils::asString(liveProxyContentResponse, field, "");
                    
						string errorMessage = string("FFMPEGEncoder error")
                            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
							+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
							+ ", error: " + error
                        ;
						_logger->error(__FILEREF__ + errorMessage);

						throw runtime_error(errorMessage);
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

				pair<string, bool> encoderDetails =
					_mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = encoderDetails;
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;
				// manifestFilePathName = _encodingItem->_stagingEncodedAssetPathName;

				// we have to reset _encodingItem->_encoderKey because in case we will come back
				// in the above 'while' loop, we have to select another encoder
				_encodingItem->_encoderKey	= -1;

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
				+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
				+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
				+ ", transcoder: " + _currentUsedFFMpegEncoderHost
				+ ", _currentUsedFFMpegEncoderKey: " + to_string(_currentUsedFFMpegEncoderKey)
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(
				_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			if (timePeriod)
				;
			else
			{
				// encodingProgress: fixed to -1 (LIVE)

				try
				{
					int encodingProgress = -1;

					_logger->info(__FILEREF__ + "updateEncodingJobProgress"
						+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encodingProgress: " + to_string(encodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress (
						_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobProgress failed"
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

            // loop waiting the end of the encoding
            bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;
			// string lastRecordedAssetFileName;
			chrono::system_clock::time_point startCheckingEncodingStatus
				= chrono::system_clock::now();

			int encoderNotReachableFailures = 0;
			int encodingPid;
			int lastEncodingPid = 0;

			// 2020-11-28: the next while, it was added encodingStatusFailures condition because,
			//  in case the transcoder is down (once I had to upgrade his operative system),
			//  the engine has to select another encoder and not remain in the next loop indefinitely
            while(!(encodingFinished || encoderNotReachableFailures >=
				_maxEncoderNotReachableFailures))
            // while(!encodingFinished)
            {
				_logger->info(__FILEREF__ + "sleep_for"
					+ ", _ingestionJobKey: "
						+ to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _intervalInSecondsToCheckEncodingFinished: "
						+ to_string(_intervalInSecondsToCheckEncodingFinished)
				);
				this_thread::sleep_for(chrono::seconds(
					_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
						getEncodingStatus(/* _encodingItem->_encodingJobKey */);
					tie(encodingFinished, killedByUser, completedWithError,
						encodingErrorMessage,
						urlForbidden, urlNotFound, ignore, encodingPid) = encodingStatus;
					_logger->info(__FILEREF__ + "getEncodingStatus"
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", currentAttemptsNumberInCaseOfErrors: "
							+ to_string(currentAttemptsNumberInCaseOfErrors)
						+ ", maxAttemptsNumberInCaseOfErrors: "
							+ to_string(maxAttemptsNumberInCaseOfErrors)
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
							string firstLineOfEncodingErrorMessage;
							{
								string firstLine;
								stringstream ss(encodingErrorMessage);
								if (getline(ss, firstLine))
									firstLineOfEncodingErrorMessage = firstLine;
								else
									firstLineOfEncodingErrorMessage = encodingErrorMessage;
							}

							_mmsEngineDBFacade->appendIngestionJobErrorMessage(
								_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
						}
						catch(runtime_error e)
						{
							_logger->error(__FILEREF__
								+ "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", e.what(): " + e.what()
							);
						}
						catch(exception e)
						{
							_logger->error(__FILEREF__
								+ "appendIngestionJobErrorMessage failed"
								+ ", _ingestionJobKey: " +
									to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);
						}
					}

					if (completedWithError) // || chunksWereNotGenerated)
					{
						if (urlForbidden || urlNotFound)
							// see my comment at the beginning of the while loop
						{
							string errorMessage =
								__FILEREF__ + "Encoding failed because of URL Forbidden or Not Found (look the Transcoder logs)"             
								+ ", _ingestionJobKey: "
									+ to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", _currentUsedFFMpegEncoderHost: "
									+ _currentUsedFFMpegEncoderHost
								+ ", encodingErrorMessage: " + encodingErrorMessage
								;
							_logger->error(errorMessage);

							throw runtime_error(errorMessage);
						}

						currentAttemptsNumberInCaseOfErrors++;

						string errorMessage = __FILEREF__ + "Encoding failed"             
							+ ", _ingestionJobKey: "
								+ to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", _currentUsedFFMpegEncoderHost: "
								+ _currentUsedFFMpegEncoderHost
							+ ", encodingErrorMessage: " + encodingErrorMessage
						;
						_logger->error(errorMessage);

						if (alwaysRetry)
						{
							// encodingStatusFailures++;

							// in this scenario encodingFinished is true

							_logger->info(__FILEREF__
								+ "Start waiting loop for the next call"
								+ ", _ingestionJobKey: "
									+ to_string(_encodingItem->_ingestionJobKey)
								+ ", _encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
							);

							chrono::system_clock::time_point startWaiting
								= chrono::system_clock::now();
							chrono::system_clock::time_point now;
							do
							{
								_logger->info(__FILEREF__ + "sleep_for"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", _intervalInSecondsToCheckEncodingFinished: "
										+ to_string(_intervalInSecondsToCheckEncodingFinished)
									+ ", currentAttemptsNumberInCaseOfErrors: "
										+ to_string(currentAttemptsNumberInCaseOfErrors)
									+ ", maxAttemptsNumberInCaseOfErrors: "
										+ to_string(maxAttemptsNumberInCaseOfErrors)
									// + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
								);
								// 2021-02-12: moved sleep here because, in this case, if the task was killed
								// during the sleep, it will check that.
								// Before the sleep was after the check, so when the sleep is finished,
								// the flow will go out of the loop and no check is done and Task remains up
								// even if user kiiled it.
								this_thread::sleep_for(chrono::seconds(
									_intervalInSecondsToCheckEncodingFinished));

								// update EncodingJob failures number to notify the GUI EncodingJob is failing
								try
								{
									_logger->info(__FILEREF__
										+ "check and update encodingJob FailuresNumber"
										+ ", _ingestionJobKey: "
											+ to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: "
											+ to_string(_encodingItem->_encodingJobKey)
										+ ", currentAttemptsNumberInCaseOfErrors: "
											+ to_string(currentAttemptsNumberInCaseOfErrors)
									);

									bool isKilled =
										_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
											_encodingItem->_encodingJobKey, 
											currentAttemptsNumberInCaseOfErrors
									);
									if (isKilled)
									{
										_logger->info(__FILEREF__
											+ "LiveProxy Killed by user during waiting loop"
											+ ", _ingestionJobKey: "
												+ to_string(_encodingItem->_ingestionJobKey)
											+ ", _encodingJobKey: "
												+ to_string(_encodingItem->_encodingJobKey)
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
									_logger->error(__FILEREF__
										+ "updateEncodingJobFailuresNumber FAILED"
										+ ", _ingestionJobKey: "
											+ to_string(_encodingItem->_ingestionJobKey)
										+ ", _encodingJobKey: "
											+ to_string(_encodingItem->_encodingJobKey)
									);
								}

								now = chrono::system_clock::now();
							}
							while (chrono::duration_cast<chrono::seconds>(now - startWaiting)
									< chrono::seconds(
										waitingSecondsBetweenAttemptsInCaseOfErrors)
									&& (timePeriod || currentAttemptsNumberInCaseOfErrors <
										maxAttemptsNumberInCaseOfErrors)
									&& !killedByUser);
						}

						// if (chunksWereNotGenerated)
						// 	encodingFinished = true;

						throw runtime_error(errorMessage);
					}
					else
					{
						// ffmpeg is running successful,
						// we will make sure currentAttemptsNumberInCaseOfErrors is reset
						currentAttemptsNumberInCaseOfErrors = 0;

						// if (encodingStatusFailures > 0)
						{
							try
							{
								// update EncodingJob failures number to notify
								// the GUI encodingJob is successful
								// encodingStatusFailures = 0;

								_logger->info(__FILEREF__
									+ "updateEncodingJobFailuresNumber"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", currentAttemptsNumberInCaseOfErrors: "
										+ to_string(currentAttemptsNumberInCaseOfErrors)
								);

								int64_t mediaItemKey = -1;
								int64_t encodedPhysicalPathKey = -1;
								_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
										_encodingItem->_encodingJobKey, 
										currentAttemptsNumberInCaseOfErrors
								);
							}
							catch(...)
							{
								_logger->error(__FILEREF__
									+ "updateEncodingJobFailuresNumber FAILED"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
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
								chrono::system_clock::time_point now
									= chrono::system_clock::now();           
								utcNow = chrono::system_clock::to_time_t(now);
							}

							int encodingProgress;

							if (utcNow < utcProxyPeriodStart)
								encodingProgress = 0;
							else if (utcProxyPeriodStart < utcNow
									&& utcNow < utcProxyPeriodEnd)      
								encodingProgress =
									((utcNow - utcProxyPeriodStart) * 100) /
									(utcProxyPeriodEnd - utcProxyPeriodStart);
							else
								encodingProgress = 100;

							try
							{
								_logger->info(__FILEREF__ + "updateEncodingJobProgress"
									+ ", ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
									+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: " + to_string(encodingProgress)
									+ ", utcProxyPeriodStart: " + to_string(utcProxyPeriodStart)
									+ ", utcNow: " + to_string(utcNow)
									+ ", utcProxyPeriodEnd: " + to_string(utcProxyPeriodEnd)
								);
								_mmsEngineDBFacade->updateEncodingJobProgress (
									_encodingItem->_encodingJobKey, encodingProgress);
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__
									+ "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: "
										+ to_string(encodingProgress)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__
									+ "updateEncodingJobProgress failed"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", encodingProgress: "
										+ to_string(encodingProgress)
								);
							}
						}

						if (lastEncodingPid != encodingPid)
						{
							try
							{
								_logger->info(__FILEREF__ + "encoderPid check, updateEncodingPid"
									+ ", ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", lastEncodingPid: " + to_string(lastEncodingPid)
									+ ", encodingPid: " + to_string(encodingPid)
								);
								_mmsEngineDBFacade->updateEncodingPid (
									_encodingItem->_encodingJobKey, encodingPid);

								lastEncodingPid = encodingPid;
							}
							catch(runtime_error e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
									+ ", e.what(): " + e.what()
								);
							}
							catch(exception e)
							{
								_logger->error(__FILEREF__ + "updateEncodingPid failed"
									+ ", _ingestionJobKey: "
										+ to_string(_encodingItem->_ingestionJobKey)
									+ ", _encodingJobKey: "
										+ to_string(_encodingItem->_encodingJobKey)
									+ ", _encodingPid: " + to_string(encodingPid)
								);
							}
						}
						else
						{
							_logger->info(__FILEREF__ + "encoderPid check, not changed"
								+ ", ingestionJobKey: "
									+ to_string(_encodingItem->_ingestionJobKey)
								+ ", encodingJobKey: "
									+ to_string(_encodingItem->_encodingJobKey)
								+ ", encodingPid: " + to_string(encodingPid)
							);
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
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encoderNotReachableFailures: "
							+ to_string(encoderNotReachableFailures)
						+ ", _maxEncoderNotReachableFailures: "
							+ to_string(_maxEncoderNotReachableFailures)
						+ ", _currentUsedFFMpegEncoderHost: "
							+ _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: "
							+ to_string(_currentUsedFFMpegEncoderKey)
					);
				}
                catch(...)
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

					_logger->error(__FILEREF__ + "getEncodingStatus failed"
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", encoderNotReachableFailures: "
							+ to_string(encoderNotReachableFailures)
						+ ", _maxEncoderNotReachableFailures: "
							+ to_string(_maxEncoderNotReachableFailures)
						+ ", _currentUsedFFMpegEncoderHost: "
							+ _currentUsedFFMpegEncoderHost
						+ ", _currentUsedFFMpegEncoderKey: "
							+ to_string(_currentUsedFFMpegEncoderKey)
					);
                }
            }
            
            chrono::system_clock::time_point endEncoding = chrono::system_clock::now();

			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			if (timePeriod)
			{
				if (utcNowCheckToExit < utcProxyPeriodEnd)
				{
					_logger->error(__FILEREF__
						+ "LiveProxy media file completed unexpected"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", still remaining seconds (utcProxyPeriodEnd - utcNow): "
							+ to_string(utcProxyPeriodEnd - utcNowCheckToExit)
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						// + ", encodingStatusFailures: " + to_string(encodingStatusFailures)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(
								endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: "
							+ to_string(_intervalInSecondsToCheckEncodingFinished)
					);

					try
					{
						char strDateTime [64];
						{
							time_t utcTime = chrono::system_clock::to_time_t(
								chrono::system_clock::now());
							tm tmDateTime;
							localtime_r (&utcTime, &tmDateTime);
							sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
								tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1,
								tmDateTime. tm_mday,
								tmDateTime. tm_hour, tmDateTime. tm_min,
								tmDateTime. tm_sec);
						}
						string errorMessage = string(strDateTime)
							+ " LiveProxy media file completed unexpected";

						string firstLineOfErrorMessage;
						{
							string firstLine;
							stringstream ss(errorMessage);
							if (getline(ss, firstLine))
								firstLineOfErrorMessage = firstLine;
							else
								firstLineOfErrorMessage = errorMessage;
						}

						_mmsEngineDBFacade->appendIngestionJobErrorMessage(
							_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
					}
					catch(runtime_error e)
					{
						_logger->error(__FILEREF__
							+ "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
							+ ", e.what(): " + e.what()
						);
					}
					catch(exception e)
					{
						_logger->error(__FILEREF__
							+ "appendIngestionJobErrorMessage failed"
							+ ", _ingestionJobKey: " +
								to_string(_encodingItem->_ingestionJobKey)
							+ ", _encodingJobKey: "
								+ to_string(_encodingItem->_encodingJobKey)
						);
					}
				}
				else
				{
					_logger->info(__FILEREF__ + "LiveProxy media file completed"
						+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
						+ ", _ingestionJobKey: "
							+ to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", ffmpegEncoderURL: " + ffmpegEncoderURL
						+ ", encodingFinished: " + to_string(encodingFinished)
						+ ", killedByUser: " + to_string(killedByUser)
						+ ", @MMS statistics@ - encodingDuration (secs): @"
							+ to_string(chrono::duration_cast<chrono::seconds>(
								endEncoding - startEncoding).count()) + "@"
						+ ", _intervalInSecondsToCheckEncodingFinished: "
							+ to_string(_intervalInSecondsToCheckEncodingFinished)
					);
				}
			}
			else
			{
				_logger->error(__FILEREF__ + "LiveProxy media file completed unexpected"
                    + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
                    + ", ffmpegEncoderURL: " + ffmpegEncoderURL
                    + ", encodingFinished: " + to_string(encodingFinished)
                    + ", killedByUser: " + to_string(killedByUser)
                    + ", @MMS statistics@ - encodingDuration (secs): @"
						+ to_string(chrono::duration_cast<chrono::seconds>(
							endEncoding - startEncoding).count()) + "@"
                    + ", _intervalInSecondsToCheckEncodingFinished: "
						+ to_string(_intervalInSecondsToCheckEncodingFinished)
				);

				try
				{
					char strDateTime [64];
					{
						time_t utcTime = chrono::system_clock::to_time_t(
							chrono::system_clock::now());
						tm tmDateTime;
						localtime_r (&utcTime, &tmDateTime);
						sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
							tmDateTime. tm_year + 1900, tmDateTime. tm_mon + 1,
							tmDateTime. tm_mday,
							tmDateTime. tm_hour, tmDateTime. tm_min, tmDateTime. tm_sec);
					}
					string errorMessage = string(strDateTime)
						+ " LiveProxy media file completed unexpected";

					string firstLineOfErrorMessage;
					{
						string firstLine;
						stringstream ss(errorMessage);
						if (getline(ss, firstLine))
							firstLineOfErrorMessage = firstLine;
						else
							firstLineOfErrorMessage = errorMessage;
					}

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(
						_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
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

			// in this case we will through the exception independently
			// if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
            throw e;
        }
        catch (EncoderNotFound e)
        {
            _logger->error(__FILEREF__ + "Encoder not found"
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

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", currentAttemptsNumberInCaseOfErrors: "
						+ to_string(currentAttemptsNumberInCaseOfErrors)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					currentAttemptsNumberInCaseOfErrors
				);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
		catch(MaxConcurrentJobsReached e)
		{
            string errorMessage = string("MaxConcurrentJobsReached")
                + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
                + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
				+ ", response.str(): " + (responseInitialized ? response.str() : "")
                + ", e.what(): " + e.what()
                ;
            _logger->warn(__FILEREF__ + errorMessage);

			// in this case we will through the exception independently
			// if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
            throw e;
        }
        catch (runtime_error e)
        {
			string error = e.what();
			if (error.find(NoEncodingAvailable().what()) != string::npos)
			{
				string errorMessage = string("No Encodings available / MaxConcurrentJobsReached")
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) 
					+ ", error: " + error 
				;
				_logger->warn(__FILEREF__ + errorMessage);

				throw MaxConcurrentJobsReached();
			}
			else
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

					_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", currentAttemptsNumberInCaseOfErrors: "
							+ to_string(currentAttemptsNumberInCaseOfErrors)
					);

					int64_t mediaItemKey = -1;
					int64_t encodedPhysicalPathKey = -1;
					_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
						_encodingItem->_encodingJobKey, 
						currentAttemptsNumberInCaseOfErrors
					);
				}
				catch(...)
				{
					_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
				currentAttemptsNumberInCaseOfErrors++;
				// encodingStatusFailures++;

				_logger->info(__FILEREF__ + "updateEncodingJobFailuresNumber"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", currentAttemptsNumberInCaseOfErrors: "
						+ to_string(currentAttemptsNumberInCaseOfErrors)
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber (
					_encodingItem->_encodingJobKey, 
					currentAttemptsNumberInCaseOfErrors
				);
			}
			catch(...)
			{
				_logger->error(__FILEREF__ + "updateEncodingJobFailuresNumber FAILED"
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
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
            + ", currentAttemptsNumberInCaseOfErrors: "
				+ to_string(currentAttemptsNumberInCaseOfErrors) 
            + ", maxAttemptsNumberInCaseOfErrors: "
				+ to_string(maxAttemptsNumberInCaseOfErrors) 
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
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
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
            + ", _workspace->_directoryName: " + _encodingItem->_workspace->_directoryName
        );
                
        throw e;
    }
}

tuple<bool, bool, bool, string, bool, bool, int, int>
	EncoderVideoAudioProxy::getEncodingStatus()
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
		ffmpegEncoderURL =
			_currentUsedFFMpegEncoderHost
			+ _ffmpegEncoderStatusURI
			+ "/" + to_string(_encodingItem->_ingestionJobKey)
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
            Json::Value encodeStatusResponse = JSONUtils::toJson(-1, -1, sResponse);

			string field = "completedWithError";
			completedWithError = JSONUtils::asBool(encodeStatusResponse, field, false);

			field = "errorMessage";
			encoderErrorMessage = JSONUtils::asString(encodeStatusResponse, field, "");

			field = "encodingFinished";
			encodingFinished = JSONUtils::asBool(encodeStatusResponse, field, false);

			field = "killedByUser";
			killedByUser = JSONUtils::asBool(encodeStatusResponse, field, false);

			field = "urlForbidden";
			urlForbidden = JSONUtils::asBool(encodeStatusResponse, field, false);

			field = "urlNotFound";
			urlNotFound = JSONUtils::asBool(encodeStatusResponse, field, false);

			field = "encodingProgress";
			encodingProgress = JSONUtils::asInt(encodeStatusResponse, field, 0);

			field = "pid";
			pid = JSONUtils::asInt(encodeStatusResponse, field, -1);
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
		int64_t cutOfVideoMediaItemKey, double startTimeInSeconds, double endTimeInSeconds,
        Json::Value parametersRoot
)
{
    string field = "FileFormat";
    if (JSONUtils::isMetadataPresent(parametersRoot, field))
    {
        string fileFormatSpecifiedByUser = JSONUtils::asString(parametersRoot, field, "");
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
	else if (cutOfVideoMediaItemKey != -1)
	{
		MMSEngineDBFacade::CrossReferenceType   crossReferenceType =
		MMSEngineDBFacade::CrossReferenceType::CutOfVideo;

		Json::Value crossReferenceRoot;

		field = "Type";
		crossReferenceRoot[field] =
			MMSEngineDBFacade::toString(crossReferenceType);

		field = "MediaItemKey";
		crossReferenceRoot[field] = cutOfVideoMediaItemKey;

		Json::Value crossReferenceParametersRoot;
		{
			field = "StartTimeInSeconds";
			crossReferenceParametersRoot[field] = startTimeInSeconds;

			field = "EndTimeInSeconds";
			crossReferenceParametersRoot[field] = endTimeInSeconds;

			field = "Parameters";
			crossReferenceRoot[field] = crossReferenceParametersRoot;
		}

		field = "CrossReference";
		parametersRoot[field] = crossReferenceRoot;
	}

    string mediaMetadata;
    {
        mediaMetadata = JSONUtils::toString(parametersRoot);
    }

    _logger->info(__FILEREF__ + "Media metadata generated"
        + ", ingestionJobKey: " + to_string(ingestionJobKey)
        + ", mediaMetadata: " + mediaMetadata
            );

    return mediaMetadata;
}

void EncoderVideoAudioProxy::readingImageProfile(
	Json::Value encodingProfileRoot,
	string& newFormat,
	int& newWidth,
	int& newHeight,
	bool& newAspectRatio,
	string& sNewInterlaceType,
	Magick::InterlaceType& newInterlaceType
)
{
    string field;

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

        newFormat = JSONUtils::asString(encodingProfileRoot, field, "");

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

        sNewInterlaceType = JSONUtils::asString(encodingProfileImageRoot, field, "");

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

void EncoderVideoAudioProxy::awsStartChannel(
	int64_t ingestionJobKey,
	string awsChannelIdToBeStarted)
{
	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StartChannelRequest startChannelRequest;
	startChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	_logger->info(__FILEREF__ + "mediaLive.StartChannel"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto startChannelOutcome = mediaLiveClient.StartChannel(startChannelRequest);
	if (!startChannelOutcome.IsSuccess())
	{
		string errorMessage = __FILEREF__ + "AWS Start Channel failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
			+ ", errorType: " + to_string((long) startChannelOutcome.GetError().GetErrorType())
			+ ", errorMessage: " + startChannelOutcome.GetError().GetMessage()
		;
		_logger->error(errorMessage);

		// liveproxy is not stopped in case of error
		// throw runtime_error(errorMessage);
	}

	bool commandFinished = false;
	int maxCommandDuration = 120;
	Aws::MediaLive::Model::ChannelState lastChannelState
		= Aws::MediaLive::Model::ChannelState::IDLE;
	int sleepInSecondsBetweenChecks = 15;
	while(!commandFinished
		&& chrono::system_clock::now() - commandTime <
		chrono::seconds(maxCommandDuration))
	{
		Aws::MediaLive::Model::DescribeChannelRequest describeChannelRequest;
		describeChannelRequest.SetChannelId(awsChannelIdToBeStarted);

		_logger->info(__FILEREF__ + "mediaLive.DescribeChannel"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(
			describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = __FILEREF__ + "AWS Describe Channel failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
				+ ", errorType: " + to_string((long) describeChannelOutcome.GetError().GetErrorType())
				+ ", errorMessage: " + describeChannelOutcome.GetError().GetMessage()
			;
			_logger->error(errorMessage);

			this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
		else
		{
			Aws::MediaLive::Model::DescribeChannelResult describeChannelResult
				= describeChannelOutcome.GetResult();
			lastChannelState = describeChannelResult.GetState();
			if (lastChannelState ==  Aws::MediaLive::Model::ChannelState::RUNNING)
				commandFinished = true;
			else
				this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
	}

	_logger->info(__FILEREF__ + "mediaLive.StartChannel finished"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		+ ", lastChannelState: " + to_string((long) lastChannelState)
		+ ", maxCommandDuration: " + to_string(maxCommandDuration)
		+ ", elapsed (secs): " + to_string(
			chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
			- commandTime).count())
	);
}

void EncoderVideoAudioProxy::awsStopChannel(
	int64_t ingestionJobKey, string awsChannelIdToBeStarted)
{
	chrono::system_clock::time_point start = chrono::system_clock::now();

	Aws::MediaLive::MediaLiveClient mediaLiveClient;

	Aws::MediaLive::Model::StopChannelRequest stopChannelRequest;
	stopChannelRequest.SetChannelId(awsChannelIdToBeStarted);

	_logger->info(__FILEREF__ + "mediaLive.StopChannel"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
	);

	chrono::system_clock::time_point commandTime = chrono::system_clock::now();

	auto stopChannelOutcome = mediaLiveClient.StopChannel(stopChannelRequest);
	if (!stopChannelOutcome.IsSuccess())
	{
		string errorMessage = __FILEREF__ + "AWS Stop Channel failed"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
			+ ", errorType: " + to_string((long) stopChannelOutcome.GetError().GetErrorType())
			+ ", errorMessage: " + stopChannelOutcome.GetError().GetMessage()
		;
		_logger->error(errorMessage);

		// liveproxy is not stopped in case of error
		// throw runtime_error(errorMessage);
	}

	bool commandFinished = false;
	int maxCommandDuration = 120;
	Aws::MediaLive::Model::ChannelState lastChannelState
		= Aws::MediaLive::Model::ChannelState::RUNNING;
	int sleepInSecondsBetweenChecks = 15;
	while(!commandFinished
		&& chrono::system_clock::now() - commandTime <
		chrono::seconds(maxCommandDuration))
	{
		Aws::MediaLive::Model::DescribeChannelRequest describeChannelRequest;
		describeChannelRequest.SetChannelId(awsChannelIdToBeStarted);

		_logger->info(__FILEREF__ + "mediaLive.DescribeChannel"
			+ ", ingestionJobKey: " + to_string(ingestionJobKey)
			+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		);

		auto describeChannelOutcome = mediaLiveClient.DescribeChannel(
			describeChannelRequest);
		if (!describeChannelOutcome.IsSuccess())
		{
			string errorMessage = __FILEREF__ + "AWS Describe Channel failed"
				+ ", ingestionJobKey: " + to_string(ingestionJobKey)
				+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
				+ ", errorType: " + to_string((long) describeChannelOutcome.GetError().GetErrorType())
				+ ", errorMessage: " + describeChannelOutcome.GetError().GetMessage()
			;
			_logger->error(errorMessage);

			this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
		else
		{
			Aws::MediaLive::Model::DescribeChannelResult describeChannelResult
				= describeChannelOutcome.GetResult();
			lastChannelState = describeChannelResult.GetState();
			if (lastChannelState ==  Aws::MediaLive::Model::ChannelState::IDLE)
				commandFinished = true;
			else
				this_thread::sleep_for(chrono::seconds(sleepInSecondsBetweenChecks));
		}
	}

	_logger->info(__FILEREF__ + "mediaLive.StopChannel finished"
		+ ", ingestionJobKey: " + to_string(ingestionJobKey)
		+ ", awsChannelIdToBeStarted: " + awsChannelIdToBeStarted
		+ ", lastChannelState: " + to_string((long) lastChannelState)
		+ ", maxCommandDuration: " + to_string(maxCommandDuration)
		+ ", elapsed (secs): " + to_string(
			chrono::duration_cast<chrono::seconds>(chrono::system_clock::now()
			- commandTime).count())
	);
}

string EncoderVideoAudioProxy::getAWSSignedURL(string playURL, int expirationInMinutes)
{
	string signedPlayURL;

	// string mmsGUIURL;
	// ostringstream response;
	// bool responseInitialized = false;
    try
    {
		// playURL is like: https://d1nue3l1x0sz90.cloudfront.net/out/v1/ca8fd629f9204ca38daf18f04187c694/index.m3u8
		string prefix ("https://");
		if (!(
			playURL.size() >= prefix.size()
			&& 0 == playURL.compare(0, prefix.size(), prefix)
			&& playURL.find("/", prefix.size()) != string::npos
			)
		)
		{
			string errorMessage = __FILEREF__
				+ "awsSignedURL. playURL wrong format"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", playURL: " + playURL
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
		size_t uriStartIndex = playURL.find("/", prefix.size());
		string cloudFrontHostName = playURL.substr(prefix.size(),
			uriStartIndex - prefix.size());
		string uriPath = playURL.substr(uriStartIndex + 1);

		AWSSigner awsSigner(_logger);                    
		string signedPlayURL = awsSigner.calculateSignedURL(
			cloudFrontHostName,
			uriPath,
			_keyPairId,
			_privateKeyPEMPathName,
			expirationInMinutes * 60
		);

		if (signedPlayURL == "")
		{
			string errorMessage = __FILEREF__
				+ "awsSignedURL. no signedPlayURL found"
				+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
				+ ", signedPlayURL: " + signedPlayURL
			;
			_logger->error(errorMessage);

			throw runtime_error(errorMessage);
		}
    }
    catch (runtime_error e)
    {
		_logger->error(__FILEREF__ + "awsSigner failed (exception)"
			+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
			+ ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
			+ ", exception: " + e.what()
		);

		throw e;
    }
    catch (exception e)
    {
        _logger->error(__FILEREF__ + "awsSigner failed (exception)"
            + ", _proxyIdentifier: " + to_string(_proxyIdentifier)
            + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) 
            + ", exception: " + e.what()
        );

        throw e;
    }

	return signedPlayURL;
}

bool EncoderVideoAudioProxy::waitingEncoding(
	int maxConsecutiveEncodingStatusFailures)
{
	bool killedByUser = false;

	bool encodingFinished = false;
	int encodingStatusFailures = 0;
	int lastEncodingPid = 0;
	while(!(encodingFinished || encodingStatusFailures >= maxConsecutiveEncodingStatusFailures))
	{
		this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));
               
		try
		{
			bool completedWithError = false;
			string encodingErrorMessage;
			bool urlForbidden = false;
			bool urlNotFound = false;
			int encodingProgress = 0;
			int encodingPid;

			tuple<bool, bool, bool, string, bool, bool, int, int> encodingStatus =
				getEncodingStatus(/* _encodingItem->_encodingJobKey */);
			tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage,
				urlForbidden, urlNotFound, encodingProgress, encodingPid) = encodingStatus;

			if (encodingErrorMessage != "")
			{
				try
				{
					string firstLineOfEncodingErrorMessage;
					{
						string firstLine;
						stringstream ss(encodingErrorMessage);
						if (getline(ss, firstLine))
							firstLineOfEncodingErrorMessage = firstLine;
						else
							firstLineOfEncodingErrorMessage = encodingErrorMessage;
					}

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(
						_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
				}
				catch(runtime_error e)
				{
					_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
						+ ", e.what(): " + e.what()
					);
				}
				catch(exception e)
				{
					_logger->error(__FILEREF__ + "appendIngestionJobErrorMessage failed"
						+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
						+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

			if (completedWithError)
			{
				string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)"             
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost
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
				+ ", maxConsecutiveEncodingStatusFailures: " + to_string(maxConsecutiveEncodingStatusFailures)
			);

			if(encodingStatusFailures >= maxConsecutiveEncodingStatusFailures)
			{
				string errorMessage = string("getEncodingStatus too many failures")
					+ ", _proxyIdentifier: " + to_string(_proxyIdentifier)
					+ ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey)
					+ ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					+ ", encodingFinished: " + to_string(encodingFinished)
					+ ", encodingStatusFailures: " + to_string(encodingStatusFailures)
					+ ", maxConsecutiveEncodingStatusFailures: " + to_string(maxConsecutiveEncodingStatusFailures)
				;
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}

	return killedByUser;
}

