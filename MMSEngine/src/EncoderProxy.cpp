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

#include "EncoderProxy.h"
#include "FFMpeg.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "catralibraries/System.h"
#include <regex>

EncoderProxy::EncoderProxy() {}

EncoderProxy::~EncoderProxy() {}

void EncoderProxy::init(
	int proxyIdentifier, mutex *mtEncodingJobs, json configuration, shared_ptr<MultiEventsSet> multiEventsSet,
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage, shared_ptr<EncodersLoadBalancer> encodersLoadBalancer,
	shared_ptr<long> faceRecognitionNumber, int maxFaceRecognitionNumber,
#ifdef __LOCALENCODER__
	int *pRunningEncodingsNumber,
#endif
	shared_ptr<spdlog::logger> logger
)
{
	_proxyIdentifier = proxyIdentifier;

	_mtEncodingJobs = mtEncodingJobs;

	_logger = logger;
	_configuration = configuration;

	_multiEventsSet = multiEventsSet;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_mmsStorage = mmsStorage;
	_encodersLoadBalancer = encodersLoadBalancer;

	_faceRecognitionNumber = faceRecognitionNumber;
	_maxFaceRecognitionNumber = maxFaceRecognitionNumber;

	_hostName = System::getHostName();

	_mp4Encoder = JSONUtils::asString(_configuration["encoding"], "mp4Encoder", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->mp4Encoder: " + _mp4Encoder);
	_mpeg2TSEncoder = JSONUtils::asString(_configuration["encoding"], "mpeg2TSEncoder", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", encoding->mpeg2TSEncoder: " + _mpeg2TSEncoder);

	_intervalInSecondsToCheckEncodingFinished = JSONUtils::asInt(_configuration["encoding"], "intervalInSecondsToCheckEncodingFinished", 0);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", encoding->intervalInSecondsToCheckEncodingFinished: " + to_string(_intervalInSecondsToCheckEncodingFinished)
	);
	_maxSecondsToWaitUpdateEncodingJobLock = JSONUtils::asInt(_configuration["mms"]["locks"], "maxSecondsToWaitUpdateEncodingJobLock", 30);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", encoding->maxSecondsToWaitUpdateEncodingJobLock: " + to_string(_maxSecondsToWaitUpdateEncodingJobLock)
	);

	_ffmpegEncoderUser = JSONUtils::asString(_configuration["ffmpeg"], "encoderUser", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderUser: " + _ffmpegEncoderUser);
	_ffmpegEncoderPassword = JSONUtils::asString(_configuration["ffmpeg"], "encoderPassword", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderPassword: " + "...");
	_ffmpegEncoderTimeoutInSeconds = JSONUtils::asInt(_configuration["ffmpeg"], "encoderTimeoutInSeconds", 120);
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderTimeoutInSeconds: " + to_string(_ffmpegEncoderTimeoutInSeconds));
	_ffmpegEncoderProgressURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderProgressURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderProgressURI: " + _ffmpegEncoderProgressURI);
	_ffmpegEncoderStatusURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderStatusURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderStatusURI: " + _ffmpegEncoderStatusURI);
	_ffmpegEncoderKillEncodingURI = JSONUtils::asString(_configuration["ffmpeg"], "encoderKillEncodingURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderKillEncodingURI: " + _ffmpegEncoderKillEncodingURI);
	_ffmpegEncodeURI = JSONUtils::asString(_configuration["ffmpeg"], "encodeURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encodeURI: " + _ffmpegEncodeURI);
	_ffmpegOverlayImageOnVideoURI = JSONUtils::asString(_configuration["ffmpeg"], "overlayImageOnVideoURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->overlayImageOnVideoURI: " + _ffmpegOverlayImageOnVideoURI);
	_ffmpegOverlayTextOnVideoURI = JSONUtils::asString(_configuration["ffmpeg"], "overlayTextOnVideoURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->overlayTextOnVideoURI: " + _ffmpegOverlayTextOnVideoURI);
	_ffmpegGenerateFramesURI = JSONUtils::asString(_configuration["ffmpeg"], "generateFramesURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->generateFramesURI: " + _ffmpegGenerateFramesURI);
	_ffmpegSlideShowURI = JSONUtils::asString(_configuration["ffmpeg"], "slideShowURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->slideShowURI: " + _ffmpegSlideShowURI);
	_ffmpegLiveRecorderURI = JSONUtils::asString(_configuration["ffmpeg"], "liveRecorderURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->liveRecorderURI: " + _ffmpegLiveRecorderURI);
	_ffmpegLiveProxyURI = JSONUtils::asString(_configuration["ffmpeg"], "liveProxyURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->liveProxyURI: " + _ffmpegLiveProxyURI);
	_ffmpegLiveGridURI = JSONUtils::asString(_configuration["ffmpeg"], "liveGridURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->liveGridURI: " + _ffmpegLiveGridURI);
	_ffmpegVideoSpeedURI = JSONUtils::asString(_configuration["ffmpeg"], "videoSpeedURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->videoSpeedURI: " + _ffmpegVideoSpeedURI);
	_ffmpegAddSilentAudioURI = JSONUtils::asString(_configuration["ffmpeg"], "addSilentAudioURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->addSilentAudioURI: " + _ffmpegAddSilentAudioURI);
	_ffmpegPictureInPictureURI = JSONUtils::asString(_configuration["ffmpeg"], "pictureInPictureURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->pictureInPictureURI: " + _ffmpegPictureInPictureURI);
	_ffmpegIntroOutroOverlayURI = JSONUtils::asString(_configuration["ffmpeg"], "introOutroOverlayURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->ffmpegIntroOutroOverlayURI: " + _ffmpegIntroOutroOverlayURI);
	_ffmpegCutFrameAccurateURI = JSONUtils::asString(_configuration["ffmpeg"], "cutFrameAccurateURI", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->ffmpegCutFrameAccurateURI: " + _ffmpegCutFrameAccurateURI);

	_computerVisionCascadePath = JSONUtils::asString(_configuration["computerVision"], "cascadePath", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", computerVision->cascadePath: " + _computerVisionCascadePath);
	if (_computerVisionCascadePath.size() > 0 && _computerVisionCascadePath.back() == '/')
		_computerVisionCascadePath.pop_back();
	_computerVisionDefaultScale = JSONUtils::asDouble(_configuration["computerVision"], "defaultScale", 1.1);
	_logger->info(__FILEREF__ + "Configuration item" + ", computerVision->defaultScale: " + to_string(_computerVisionDefaultScale));
	_computerVisionDefaultMinNeighbors = JSONUtils::asInt(_configuration["computerVision"], "defaultMinNeighbors", 2);
	_logger->info(__FILEREF__ + "Configuration item" + ", computerVision->defaultMinNeighbors: " + to_string(_computerVisionDefaultMinNeighbors));
	_computerVisionDefaultTryFlip = JSONUtils::asBool(_configuration["computerVision"], "defaultTryFlip", false);
	_logger->info(__FILEREF__ + "Configuration item" + ", computerVision->defaultTryFlip: " + to_string(_computerVisionDefaultTryFlip));

	_timeBeforeToPrepareResourcesInMinutes = JSONUtils::asInt(_configuration["mms"], "liveRecording_timeBeforeToPrepareResourcesInMinutes", 2);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", mms->liveRecording_timeBeforeToPrepareResourcesInMinutes: " + to_string(_timeBeforeToPrepareResourcesInMinutes)
	);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(_configuration["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", storage->_waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt(_configuration["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: " + to_string(_waitingNFSSync_milliSecondsWaitingBetweenChecks)
	);

	_keyPairId = JSONUtils::asString(_configuration["aws"], "keyPairId", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->keyPairId: " + _keyPairId);
	_privateKeyPEMPathName = JSONUtils::asString(_configuration["aws"], "privateKeyPEMPathName", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", aws->privateKeyPEMPathName: " + _privateKeyPEMPathName);

	_retrieveStreamingYouTubeURLPeriodInHours = 5; // 5 hours

	_maxEncoderNotReachableFailures = 10; // consecutive errors

#ifdef __LOCALENCODER__
	_ffmpegMaxCapacity = 1;

	_pRunningEncodingsNumber = pRunningEncodingsNumber;

	_ffmpeg = make_shared<FFMpeg>(configuration, logger);
#endif
}

void EncoderProxy::setEncodingData(EncodingJobStatus *status, shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem)
{
	_status = status;

	_encodingItem = encodingItem;
}

void EncoderProxy::operator()()
{

	_logger->info(
		__FILEREF__ + "Running EncoderProxy..." + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
		", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
		", _encodingParametersRoot: " + JSONUtils::toString(_encodingItem->_encodingParametersRoot) +
		", _ingestedParametersRoot: " + JSONUtils::toString(_encodingItem->_ingestedParametersRoot)
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
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::AddSilentAudio)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegAddSilentAudioURI, maxConsecutiveEncodingStatusFailures);
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegPictureInPictureURI, maxConsecutiveEncodingStatusFailures);
			// pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser =
			// pictureInPicture(); tie(stagingEncodedAssetPathName,
			// killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::IntroOutroOverlay)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegIntroOutroOverlayURI, maxConsecutiveEncodingStatusFailures);
			// tie(stagingEncodedAssetPathName, killedByUser) =
			// stagingEncodedAssetPathNameAndKilledByUser;
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::CutFrameAccurate)
		{
			int maxConsecutiveEncodingStatusFailures = 1;
			encodeContentVideoAudio(_ffmpegCutFrameAccurateURI, maxConsecutiveEncodingStatusFailures);
			// pair<string, bool> stagingEncodedAssetPathNameAndKilledByUser =
			// cutFrameAccurate(); tie(stagingEncodedAssetPathName,
			// killedByUser) = stagingEncodedAssetPathNameAndKilledByUser;
		}
		else
		{
			string errorMessage = string("Wrong EncodingType") + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType);

			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (MaxConcurrentJobsReached &e)
	{
		_logger->warn(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier)
		);

		try
		{
			_logger->info(
				__FILEREF__ + "updateEncodingJob MaxCapacityReached" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro
			_mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::MaxCapacityReached,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey
			);
			// main ? _encodingItem->_ingestionJobKey : -1);
		}
		catch (runtime_error &e)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ", e.what(): " + e.what()
			);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ", e.what(): " + e.what()
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob MaxCapacityReached FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (YouTubeURLNotRetrieved &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			// 2020-09-17: in case of YouTubeURLNotRetrieved there is no retries
			//	just a failure of the ingestion job
			bool forceEncodingToBeFailed = true;

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::PunctualError,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (EncoderError &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro 'no update is done'
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::PunctualError,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (EncoderNotFound &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro 'no update is done'
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::PunctualError,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (EncodingKilledByUser &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			_logger->info(
				__FILEREF__ + "updateEncodingJob KilledByUser" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::KilledByUser,
				false, // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what()
			);
			// main ? _encodingItem->_ingestionJobKey : -1, e.what());
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob KilledByUser FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (FFMpegURLForbidden &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy ||
				_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: channel cielo, the URL return FORBIDDEN and it
				// has to be generated again
				//		because it will have an expired timestamp. For this
				// reason we have to stop this request 		in order the crontab
				// script will generate a new URL
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (FFMpegURLNotFound &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder ||
				_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				// 2020-10-25: Added also LiveProxy to be here, in case of
				// LiveProxy and URL not found error
				//	does not have sense to retry, we need the generation of a
				// new URL (restream-auto case)
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished (url not found)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
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

			// isIngestionJobCompleted is true because ingestionJob has to be
			// updated
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
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::OverlayTextOnVideo)
		{
			processOverlayedTextOnVideo(killedByUser);

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::GenerateFrames)
		{
			processGeneratedFrames(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::SlideShow)
		{
			processSlideShow();

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceRecognition)
		{
			processFaceRecognition(stagingEncodedAssetPathName);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::FaceIdentification)
		{
			processFaceIdentification(stagingEncodedAssetPathName);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
		{
			processLiveRecorder(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
		{
			processLiveProxy(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VODProxy)
		{
			processLiveProxy(killedByUser);
			// processVODProxy(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::Countdown)
		{
			processLiveProxy(killedByUser);
			// processAwaitingTheBeginning(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveGrid)
		{
			processLiveGrid(killedByUser);

			isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VideoSpeed)
		{
			processVideoSpeed(killedByUser);

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::AddSilentAudio)
		{
			processAddSilentAudio(killedByUser);

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::PictureInPicture)
		{
			processPictureInPicture(killedByUser);

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::IntroOutroOverlay)
		{
			processIntroOutroOverlay();

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::CutFrameAccurate)
		{
			processCutFrameAccurate();

			if (_currentUsedFFMpegExternalEncoder)
				isIngestionJobCompleted = true;
			else
				isIngestionJobCompleted = false; // file has still to be ingested
		}
		else
		{
			string errorMessage = string("Wrong EncodingType") + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
								  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
								  ", EncodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType);

			_logger->error(__FILEREF__ + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (runtime_error &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		if (stagingEncodedAssetPathName != "" && fs::exists(stagingEncodedAssetPathName))
		{
			_logger->error(
				__FILEREF__ + "Remove" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			try
			{
				_logger->info(__FILEREF__ + "remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
				fs::remove_all(stagingEncodedAssetPathName);
			}
			catch (runtime_error &er)
			{
				_logger->error(
					__FILEREF__ + "remove FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " +
					to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
					", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
					", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName + ", er.what(): " + er.what()
				);
			}
		}

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + MMSEngineDBFacade::toString(_encodingItem->_encodingType) + ": " + e.what() +
			", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		if (stagingEncodedAssetPathName != "" && fs::exists(stagingEncodedAssetPathName))
		{
			_logger->error(
				__FILEREF__ + "Remove" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName
			);

			_logger->info(__FILEREF__ + "remove" + ", stagingEncodedAssetPathName: " + stagingEncodedAssetPathName);
			fs::remove_all(stagingEncodedAssetPathName);
		}

		try
		{
			bool forceEncodingToBeFailed;
			if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
			{
				// 2020-05-26: in case of LiveRecorder there is no more retries
				// since it already run up to the end of the recording
				forceEncodingToBeFailed = true;
			}
			else
			{
				forceEncodingToBeFailed = false;
			}

			_logger->info(
				__FILEREF__ + "updateEncodingJob PunctualError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
				", forceEncodingToBeFailed: " + to_string(forceEncodingToBeFailed)
			);

			// in case of HighAvailability of the liveRecording, only the main
			// should update the ingestionJob status This because, if also the
			// 'backup' liverecording updates the ingestionJob, it will generate
			// an erro PunctualError is used because, in case it always happens,
			// the encoding will never reach a final state
			int encodingFailureNumber = _mmsEngineDBFacade->updateEncodingJob(
				_encodingItem->_encodingJobKey,
				MMSEngineDBFacade::EncodingError::PunctualError, // ErrorBeforeEncoding,
				false,											 // isIngestionJobFinished: this field is not used by
					   // updateEncodingJob
				_encodingItem->_ingestionJobKey, e.what(),
				// main ? _encodingItem->_ingestionJobKey : -1, e.what(),
				forceEncodingToBeFailed
			);
		}
		catch (...)
		{
			_logger->error(
				__FILEREF__ + "updateEncodingJob PunctualError FAILED" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
				", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " +
				to_string(_encodingItem->_encodingJobKey) + ", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
			);
		}

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}

	try
	{
		_logger->info(
			__FILEREF__ + "updateEncodingJob NoError" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", isIngestionJobCompleted: " + to_string(isIngestionJobCompleted) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		_mmsEngineDBFacade->updateEncodingJob(
			_encodingItem->_encodingJobKey, MMSEngineDBFacade::EncodingError::NoError, isIngestionJobCompleted, _encodingItem->_ingestionJobKey
		);
	}
	catch (exception &e)
	{
		_logger->error(
			__FILEREF__ + "updateEncodingJob failed: " + e.what() + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) + ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
		);

		{
			lock_guard<mutex> locker(*_mtEncodingJobs);

			*_status = EncodingJobStatus::Free;
		}

		_logger->info(
			__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
			", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType)
		);

		// throw e;
		return;
	}

	{
		lock_guard<mutex> locker(*_mtEncodingJobs);

		*_status = EncodingJobStatus::Free;
	}

	_logger->info(
		__FILEREF__ + "EncoderProxy finished" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
		", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
		", _encodingType: " + MMSEngineDBFacade::toString(_encodingItem->_encodingType) +
		", _ingestedParametersRoot: " + JSONUtils::toString(_encodingItem->_ingestedParametersRoot)
	);
}

tuple<bool, bool, bool, string, bool, bool, double, int, long, double, long> EncoderProxy::getEncodingStatus()
{
	bool encodingFinished;
	bool killedByUser;
	bool completedWithError;
	string encoderErrorMessage;
	bool urlNotFound;
	bool urlForbidden;
	double encodingProgress;
	int pid;
	long realTimeFrameRate;
	double realTimeBitRate;
	long numberOfRestartBecauseOfFailure;

	string ffmpegEncoderURL;
	// ostringstream response;
	bool responseInitialized = false;
	try
	{
		ffmpegEncoderURL = _currentUsedFFMpegEncoderHost + _ffmpegEncoderStatusURI + "/" + to_string(_encodingItem->_ingestionJobKey) + "/" +
						   to_string(_encodingItem->_encodingJobKey);

		vector<string> otherHeaders;
		json encodeStatusResponse = MMSCURL::httpGetJson(
			_logger, _encodingItem->_ingestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser, _ffmpegEncoderPassword,
			otherHeaders
		);

		SPDLOG_INFO(
			"getEncodingStatus"
			", _proxyIdentifier: {}"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", response: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey,
			regex_replace(JSONUtils::toString(encodeStatusResponse), regex("\n"), " ")
		);

		try
		{
			// json encodeStatusResponse = JSONUtils::toJson(-1, -1, sResponse);

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
			encodingProgress = JSONUtils::asDouble(encodeStatusResponse, field, 0.0);

			field = "pid";
			pid = JSONUtils::asInt(encodeStatusResponse, field, -1);

			field = "realTimeFrameRate";
			realTimeFrameRate = JSONUtils::asInt(encodeStatusResponse, field, -1);

			field = "realTimeBitRate";
			realTimeBitRate = JSONUtils::asDouble(encodeStatusResponse, field, -1.0);

			field = "numberOfRestartBecauseOfFailure";
			numberOfRestartBecauseOfFailure = JSONUtils::asInt(encodeStatusResponse, field, -1);
		}
		catch (...)
		{
			string errorMessage = fmt::format(
				"getEncodingStatus. Response Body json is not well format"
				", _proxyIdentifier: {}"
				", ingestionJobKey: {}"
				", encodingJobKey: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	catch (ServerNotReachable e)
	{
		_logger->error(
			__FILEREF__ + "Encoder is not reachable, is it down?" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
			// + ", response.str(): " + (responseInitialized ? response.str() :
			// "")
		);

		throw EncoderNotReachable();
	}
	catch (runtime_error e)
	{
		_logger->error(
			__FILEREF__ + "Status URL failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
			// + ", response.str(): " + (responseInitialized ? response.str() :
			// "")
		);

		throw e;
	}
	catch (exception e)
	{
		_logger->error(
			__FILEREF__ + "Status URL failed (exception)" + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
			", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", ffmpegEncoderURL: " + ffmpegEncoderURL + ", exception: " + e.what()
			// + ", response.str(): " + (responseInitialized ? response.str() :
			// "")
		);

		throw e;
	}

	return make_tuple(
		encodingFinished, killedByUser, completedWithError, encoderErrorMessage, urlForbidden, urlNotFound, encodingProgress, pid, realTimeFrameRate,
		realTimeBitRate, numberOfRestartBecauseOfFailure
	);
}

string EncoderProxy::generateMediaMetadataToIngest(
	int64_t ingestionJobKey, string fileFormat, int64_t faceOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey, double startTimeInSeconds,
	double endTimeInSeconds, vector<int64_t> slideShowOfImageMediaItemKeys, vector<int64_t> slideShowOfAudioMediaItemKeys, json parametersRoot
)
{
	string field = "fileFormat";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string fileFormatSpecifiedByUser = JSONUtils::asString(parametersRoot, field, "");
		if (fileFormatSpecifiedByUser != fileFormat)
		{
			string errorMessage = string("Wrong fileFormat") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", fileFormatSpecifiedByUser: " + fileFormatSpecifiedByUser +
								  ", fileFormat: " + fileFormat;
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
		json crossReferencesRoot = json::array();
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::FaceOfVideo;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = faceOfVideoMediaItemKey;

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}
	else if (slideShowOfImageMediaItemKeys.size() > 0 || slideShowOfAudioMediaItemKeys.size() > 0)
	{
		json crossReferencesRoot = json::array();
		for (int64_t slideShowOfImageMediaItemKey : slideShowOfImageMediaItemKeys)
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::SlideShowOfImage;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = slideShowOfImageMediaItemKey;

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		for (int64_t slideShowOfAudioMediaItemKey : slideShowOfAudioMediaItemKeys)
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::SlideShowOfAudio;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = slideShowOfAudioMediaItemKey;

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}
	else if (cutOfVideoMediaItemKey != -1)
	{
		json crossReferencesRoot = json::array();
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::CutOfVideo;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = cutOfVideoMediaItemKey;

			json crossReferenceParametersRoot;
			{
				field = "startTimeInSeconds";
				crossReferenceParametersRoot[field] = startTimeInSeconds;

				field = "EndTimeInSeconds";
				crossReferenceParametersRoot[field] = endTimeInSeconds;

				field = "parameters";
				crossReferenceRoot[field] = crossReferenceParametersRoot;
			}

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}

	string mediaMetadata;
	{
		mediaMetadata = JSONUtils::toString(parametersRoot);
	}

	_logger->info(
		__FILEREF__ + "Media metadata generated" + ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaMetadata: " + mediaMetadata
	);

	return mediaMetadata;
}

void EncoderProxy::encodingImageFormatValidation(string newFormat)
{
	auto logger = spdlog::get("mmsEngineService");
	if (newFormat != "JPG" && newFormat != "GIF" && newFormat != "PNG")
	{
		string errorMessage = __FILEREF__ + "newFormat is wrong" + ", newFormat: " + newFormat;

		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}
}

Magick::InterlaceType EncoderProxy::encodingImageInterlaceTypeValidation(string sNewInterlaceType)
{
	auto logger = spdlog::get("mmsEngineService");
	Magick::InterlaceType interlaceType;

	if (sNewInterlaceType == "NoInterlace")
		interlaceType = Magick::NoInterlace;
	else if (sNewInterlaceType == "LineInterlace")
		interlaceType = Magick::LineInterlace;
	else if (sNewInterlaceType == "PlaneInterlace")
		interlaceType = Magick::PlaneInterlace;
	else if (sNewInterlaceType == "PartitionInterlace")
		interlaceType = Magick::PartitionInterlace;
	else
	{
		string errorMessage = __FILEREF__ + "sNewInterlaceType is wrong" + ", sNewInterlaceType: " + sNewInterlaceType;

		logger->error(errorMessage);

		throw runtime_error(errorMessage);
	}

	return interlaceType;
}

bool EncoderProxy::waitingEncoding(int maxConsecutiveEncodingStatusFailures)
{
	bool killedByUser = false;

	bool encodingFinished = false;
	int encodingStatusFailures = 0;
	int lastEncodingPid = 0;
	long lastRealTimeFrameRate = 0;
	long lastRealTimeBitRate = 0;
	long lastNumberOfRestartBecauseOfFailure = 0;
	while (!(encodingFinished || encodingStatusFailures >= maxConsecutiveEncodingStatusFailures))
	{
		this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

		try
		{
			bool completedWithError = false;
			string encodingErrorMessage;
			bool urlForbidden = false;
			bool urlNotFound = false;
			double encodingProgress = 0.0;
			int encodingPid;
			long realTimeFrameRate;
			long realTimeBitRate;
			long numberOfRestartBecauseOfFailure;

			// tuple<bool, bool, bool, string, bool, bool, double, int>
			// encodingStatus =
			// getEncodingStatus(/* _encodingItem->_encodingJobKey */);
			tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden, urlNotFound, encodingProgress, encodingPid,
				realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure) = getEncodingStatus();

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

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfEncodingErrorMessage);
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "appendIngestionJobErrorMessage failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
					);
				}
				catch (exception &e)
				{
					_logger->error(
						__FILEREF__ + "appendIngestionJobErrorMessage failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}
			}

			if (completedWithError)
			{
				string errorMessage = __FILEREF__ + "Encoding failed (look the Transcoder logs)" +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									  ", _currentUsedFFMpegEncoderHost: " + _currentUsedFFMpegEncoderHost +
									  ", encodingErrorMessage: " + regex_replace(encodingErrorMessage, regex("\n"), " ");
				_logger->error(errorMessage);

				throw runtime_error(errorMessage);
			}

			// encodingProgress/encodingPid
			{
				try
				{
					_logger->info(
						__FILEREF__ + "updateEncodingJobProgress" + ", encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
						", encodingProgress: " + to_string(encodingProgress)
					);
					_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch (runtime_error &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", e.what(): " + e.what()
					);
				}
				catch (exception &e)
				{
					_logger->error(
						__FILEREF__ + "updateEncodingJobProgress failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
						", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey)
					);
				}

				if (lastEncodingPid != encodingPid || lastRealTimeFrameRate != realTimeFrameRate || lastRealTimeBitRate != realTimeBitRate ||
					lastNumberOfRestartBecauseOfFailure != numberOfRestartBecauseOfFailure)
				{
					try
					{
						SPDLOG_INFO(
							"updateEncodingRealTimeInfo"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", encodingPid: {}"
							", realTimeFrameRate: {}"
							", realTimeBitRate: {}"
							", numberOfRestartBecauseOfFailure: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
							numberOfRestartBecauseOfFailure
						);
						_mmsEngineDBFacade->updateEncodingRealTimeInfo(
							_encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure
						);

						lastEncodingPid = encodingPid;
						lastRealTimeFrameRate = realTimeFrameRate;
						lastRealTimeBitRate = realTimeBitRate;
						lastNumberOfRestartBecauseOfFailure = numberOfRestartBecauseOfFailure;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"updateEncodingRealTimeInfo failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", encodingPid: {}"
							", realTimeFrameRate: {}"
							", realTimeBitRate: {}"
							", numberOfRestartBecauseOfFailure: {}"
							", e.what: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
							numberOfRestartBecauseOfFailure, e.what()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"updateEncodingRealTimeInfo failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", encodingPid: {}"
							", realTimeFrameRate: {}"
							", realTimeBitRate: {}"
							", numberOfRestartBecauseOfFailure: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingPid, realTimeFrameRate, realTimeBitRate,
							numberOfRestartBecauseOfFailure
						);
					}
				}
			}

			// 2020-06-10: encodingStatusFailures is reset since
			// getEncodingStatus was successful.
			//	Scenario:
			//		1. only sometimes (about once every two hours) an encoder
			//(deployed on centos) running a LiveRecorder continuously, 			returns
			//'timeout'. 			Really the encoder was working fine, ffmpeg was also
			// running fine, 			just FastCGIAccept was not getting the request
			//		2. these errors was increasing encodingStatusFailures and at
			// the end, it reached the max failures 			and this thread terminates,
			// even if the encoder and ffmpeg was working fine. 		This scenario
			// creates problems and non-consistency between engine and encoder.
			//		For this reason, if the getEncodingStatus is successful,
			// encodingStatusFailures is reset.
			encodingStatusFailures = 0;
		}
		catch (...)
		{
			encodingStatusFailures++;

			_logger->error(
				__FILEREF__ + "getEncodingStatus failed" + ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
				", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) + ", encodingStatusFailures: " + to_string(encodingStatusFailures) +
				", maxConsecutiveEncodingStatusFailures: " + to_string(maxConsecutiveEncodingStatusFailures)
			);

			if (encodingStatusFailures >= maxConsecutiveEncodingStatusFailures)
			{
				string errorMessage = string("getEncodingStatus too many failures") + ", _proxyIdentifier: " + to_string(_proxyIdentifier) +
									  ", _ingestionJobKey: " + to_string(_encodingItem->_ingestionJobKey) +
									  ", _encodingJobKey: " + to_string(_encodingItem->_encodingJobKey) +
									  ", encodingFinished: " + to_string(encodingFinished) +
									  ", encodingStatusFailures: " + to_string(encodingStatusFailures) +
									  ", maxConsecutiveEncodingStatusFailures: " + to_string(maxConsecutiveEncodingStatusFailures);
				_logger->error(__FILEREF__ + errorMessage);

				throw runtime_error(errorMessage);
			}
		}
	}

	return killedByUser;
}
