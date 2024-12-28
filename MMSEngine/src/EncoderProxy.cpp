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
#include "catralibraries/DateTime.h"
#include "catralibraries/System.h"
#include "spdlog/spdlog.h"
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
		else if (_encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::LiveProxy ||
				 _encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::VODProxy ||
				 _encodingItem->_encodingType == MMSEngineDBFacade::EncodingType::Countdown)
			killedByUser = liveProxy(_encodingItem->_encodingType);
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

bool EncoderProxy::waitingLiveProxyOrLiveRecorder(
	MMSEngineDBFacade::EncodingType encodingType, string ffmpegURI, bool timePeriod, time_t utcPeriodStart, time_t utcPeriodEnd,
	uint32_t maxAttemptsNumberInCaseOfErrors, string ipPushStreamConfigurationLabel
)
{
	bool killedByUser = false;
	bool urlForbidden = false;
	bool urlNotFound = false;

	SPDLOG_INFO(
		"check maxAttemptsNumberInCaseOfErrors"
		", ingestionJobKey: {}"
		", encodingJobKey: {}"
		", maxAttemptsNumberInCaseOfErrors: {}",
		_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, maxAttemptsNumberInCaseOfErrors
	);

	// viene incrementato se completedWithError, EncoderNotFound, Encoding URL failed
	// viene resettato a 0 se !completedWithError
	long currentAttemptsNumberInCaseOfErrors = 0;

	try
	{
		SPDLOG_INFO(
			"updateEncodingJobFailuresNumber"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", currentAttemptsNumberInCaseOfErrors: {}",
			_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
		);

		killedByUser = _mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
	}
	catch (...)
	{
		SPDLOG_ERROR(
			"updateEncodingJobFailuresNumber FAILED"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", currentAttemptsNumberInCaseOfErrors: {}",
			_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
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
	// 2021-05-29: LiveProxy has to exit if:
	//	- was killed OR
	//	- if timePeriod true
	//		- no way to exit (we have to respect the timePeriod)
	//	- if timePeriod false
	//		- exit if too many error or urlForbidden or urlNotFound
	time_t utcNowCheckToExit = 0;
	while (! // while we are NOT in the exit condition
		   (
			   // exit condition
			   killedByUser ||
			   (!timePeriod && (urlForbidden || urlNotFound || currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)) ||
			   (timePeriod && utcNowCheckToExit >= utcPeriodEnd)
		   ))
	{
		if (timePeriod)
		{
			SPDLOG_INFO(
				"check to exit"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", _encodingItem->_encoderKey: {}"
				", still miss (secs): {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey, utcPeriodEnd - utcNowCheckToExit
			);
		}

		string ffmpegEncoderURL;
		ostringstream response;
		bool responseInitialized = false;
		try
		{
			_currentUsedFFMpegExternalEncoder = false;

			if (_encodingItem->_encoderKey == -1)
			{
				// 2021-12-14: we have to read again encodingParametersRoot because, in case the playlist (inputsRoot) is changed, the
				// updated inputsRoot is into DB
				{
					try
					{
						MMSEngineDBFacade::IngestionStatus ingestionJobStatus;

						// 2022-12-18: fromMaster true because the inputsRoot maybe was just updated (modifying the playlist)
						if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy || encodingType == MMSEngineDBFacade::EncodingType::VODProxy ||
							encodingType == MMSEngineDBFacade::EncodingType::Countdown)
							_encodingItem->_encodingParametersRoot =
								_mmsEngineDBFacade->encodingJob_columnAsJson("parameters", _encodingItem->_encodingJobKey, true);

						// 2024-12-01: ricarichiamo ingestedParameters perchè potrebbe essere stato modificato con un nuovo 'encodersDetails' (nello
						// scenario in cui si vuole eseguire lo switch di un ingestionjob su un nuovo encoder)
						tie(ingestionJobStatus, _encodingItem->_ingestedParametersRoot) = _mmsEngineDBFacade->ingestionJob_StatusMetadataContent(
							_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, true
						);

						// viene controllato lo status perchè nello scenario in cui l'IngestionJob sia terminato e all'interno
						// di questo if viene generata una eccezione (ad es. EncoderNotFound nella call
						// _encodersLoadBalancer->getEncoderURL), si prosegue nel catch che ci riporta a inizio while
						// Entriamo quindi in un loop (inizio-while, eccezione EncoderNotFound, inizio-while) quando l'IngestionJob è terminato
						string sIngestionJobStatus = MMSEngineDBFacade::toString(ingestionJobStatus);
						if (sIngestionJobStatus.starts_with("End_"))
						{
							SPDLOG_INFO(
								"IngestionJob is terminated"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", ingestionJobStatus: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, sIngestionJobStatus
							);
							killedByUser = true;
							continue;
						}
					}
					catch (DBRecordNotFound &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (runtime_error &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"encodingJob_Parameters failed"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", e.what(): {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
						);

						throw e;
					}
				}

				// IN ingestionJob->metadataParameters abbiamo già il campo encodersPool.
				// Aggiungiamo encoderKey nel caso di IP_PUSH in modo da avere un posto unico (ingestionJob->metadataParameters)
				// per questa informazione
				if (ipPushStreamConfigurationLabel != "")
				{
					// scenario:
					// 	- viene configurato uno Stream per un IP_PUSH su un encoder specifico
					// 	- questo encoder ha un fault e va giu
					// 	- finche questo encode non viene ripristinato (dipende da Hetzner) abbiamo un outage
					// 	- Per evitare l'outage, posso io cambiare encoder nella configurazione dello Stream
					// 	- La getStreamInputPushDetails sotto mi serve in questo loop per recuperare avere l'encoder aggiornato configurato nello
					// Stream 		altrimenti rimarremmo con l'encoder e l'url calcolata all'inizio e non potremmo evitare l'outage
					// 2024-06-25: In uno scenario di Broadcaster e Broadcast, il cambiamento descritto sopra
					// 		risolve il problema del broadcaster ma non quello del broadcast. Infatti il broadcast ha il campo udpUrl
					// 		nell'outputRoot che punta al transcoder iniziale. Questo campo udpUrl è stato inizializzato
					// 		in CatraMMSBroadcaster.java (method: addBroadcaster).

					int64_t updatedPushEncoderKey = -1;
					string updatedUrl;
					{
						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						/*
						if (encodersDetailsRoot == nullptr)
						{
							// quando elimino questo if, verifica se anche la funzione getStreamInputPushDetails possa essere eliminata
							// per essere sostituita da getStreamPushServerUrl
							tie(updatedPushEncoderKey, updatedUrl) = _mmsEngineDBFacade->getStreamInputPushDetails(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, streamConfigurationLabel
							);
						}
						else
						*/
						{
							// questo quello corretto, l'if sopra dovrebbe essere eliminato

							updatedPushEncoderKey = JSONUtils::asInt64(encodersDetailsRoot, "pushEncoderKey", static_cast<int64_t>(-1));
							if (updatedPushEncoderKey == -1)
							{
								string errorMessage = fmt::format(
									"Wrong pushEncoderKey"
									", _ingestionJobKey: {}"
									", _encodingJobKey: {}"
									", encodersDetailsRoot: {}",
									_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, JSONUtils::toString(encodersDetailsRoot)
								);
								SPDLOG_ERROR(errorMessage);

								throw runtime_error(errorMessage);
							}

							bool pushPublicEncoderName = JSONUtils::asBool(encodersDetailsRoot, "pushPublicEncoderName", false);

							updatedUrl = _mmsEngineDBFacade->getStreamPushServerUrl(
								_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, ipPushStreamConfigurationLabel,
								updatedPushEncoderKey, pushPublicEncoderName, true
							);
						}
					}

					if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy || encodingType == MMSEngineDBFacade::EncodingType::VODProxy ||
						encodingType == MMSEngineDBFacade::EncodingType::Countdown)
					{
						json inputsRoot = (_encodingItem->_encodingParametersRoot)["inputsRoot"];
						json firstInputRoot = inputsRoot[0];
						json proxyInputRoot;

						if (encodingType == MMSEngineDBFacade::EncodingType::VODProxy)
							proxyInputRoot = firstInputRoot["vodInput"];
						else if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy)
							proxyInputRoot = firstInputRoot["streamInput"];
						else if (encodingType == MMSEngineDBFacade::EncodingType::Countdown)
							proxyInputRoot = firstInputRoot["countdownInput"];

						proxyInputRoot["pushEncoderKey"] = updatedPushEncoderKey;
						proxyInputRoot["url"] = updatedUrl;
						inputsRoot[0]["streamInput"] = proxyInputRoot;
						(_encodingItem->_encodingParametersRoot)["inputsRoot"] = inputsRoot;
					}
					else // if (encodingType == MMSEngineDBFacade::EncodingType::liveRecorder)
					{
						_encodingItem->_encodingParametersRoot["pushEncoderKey"] = updatedPushEncoderKey;
						_encodingItem->_encodingParametersRoot["liveURL"] = updatedUrl;
					}

					_currentUsedFFMpegEncoderKey = updatedPushEncoderKey;
					// 2023-12-18: pushEncoderName è importante che sia usato
					// nella url rtmp
					//	dove il transcoder ascolta per il flusso di streaming
					//	ma non deve essere usato per decidere l'url con cui
					// l'engine deve comunicare 	con il transcoder. Questa url
					// dipende solamente dal fatto che il transcoder 	sia interno
					// o esterno
					tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_mmsEngineDBFacade->getEncoderURL(updatedPushEncoderKey); // , pushEncoderName);

					SPDLOG_INFO(
						"LiveProxy/Recording. Retrieved updated Stream info"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", updatedPushEncoderKey: {}"
						", updatedUrl: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, updatedPushEncoderKey, updatedUrl
					);
				}
				else
				{
					string encodersPool;
					if (encodingType == MMSEngineDBFacade::EncodingType::VODProxy || encodingType == MMSEngineDBFacade::EncodingType::Countdown)
					{
						// both vodProxy and countdownProxy work with VODs and
						// the encodersPool is defined by the ingestedParameters field
						encodersPool = JSONUtils::asString(_encodingItem->_ingestedParametersRoot, "encodersPool", "");
					}
					else // if (encodingType == MMSEngineDBFacade::EncodingType::LiveProxy || encodingType ==
						 // MMSEngineDBFacade::EncodingType::LiveRecorder)
					{
						json internalMMSRoot = JSONUtils::asJson(_encodingItem->_ingestedParametersRoot, "internalMMS", nullptr);
						json encodersDetailsRoot = JSONUtils::asJson(internalMMSRoot, "encodersDetails", nullptr);
						encodersPool = JSONUtils::asString(encodersDetailsRoot, "encodersPoolLabel", string());
					}

					int64_t encoderKeyToBeSkipped = -1;
					bool externalEncoderAllowed = true;
					tie(_currentUsedFFMpegEncoderKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) =
						_encodersLoadBalancer->getEncoderURL(
							_encodingItem->_ingestionJobKey, encodersPool, _encodingItem->_workspace, encoderKeyToBeSkipped, externalEncoderAllowed
						);
				}

				SPDLOG_INFO(
					"Configuration item"
					", _proxyIdentifier: {}"
					", _currentUsedFFMpegEncoderHost: {}"
					", _currentUsedFFMpegEncoderKey: {}",
					_proxyIdentifier, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
				);
				ffmpegEncoderURL = fmt::format(
					"{}{}/{}/{}", _currentUsedFFMpegEncoderHost, ffmpegURI, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);

				string body;
				{
					json bodyRoot;

					// 2023-03-21: rimuovere il parametro ingestionJobKey se il
					// trascoder deployed è > 1.0.5315
					bodyRoot["ingestionJobKey"] = _encodingItem->_ingestionJobKey;
					bodyRoot["externalEncoder"] = _currentUsedFFMpegExternalEncoder;
					// non sembra il campo url serva al liveProxy
					// bodyRoot["liveURL"] = streamUrl;
					bodyRoot["ingestedParametersRoot"] = _encodingItem->_ingestedParametersRoot;
					bodyRoot["encodingParametersRoot"] = _encodingItem->_encodingParametersRoot;

					body = JSONUtils::toString(bodyRoot);
				}

				SPDLOG_INFO(
					"LiveProxy/Recording. Selection of the transcoder. The transcoder is selected by load balancer"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", transcoder: {}"
					", _currentUsedFFMpegEncoderKey: {}"
					", body: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost,
					_currentUsedFFMpegEncoderKey, body
				);

				vector<string> otherHeaders;
				json liveProxyContentResponse;
				try
				{
					liveProxyContentResponse = MMSCURL::httpPostStringAndGetJson(
						_logger, _encodingItem->_ingestionJobKey, ffmpegEncoderURL, _ffmpegEncoderTimeoutInSeconds, _ffmpegEncoderUser,
						_ffmpegEncoderPassword, body,
						"application/json", // contentType
						otherHeaders
					);
				}
				catch (runtime_error &e)
				{
					string error = e.what();
					if (error.find(EncodingIsAlreadyRunning().what()) != string::npos)
					{
						// 2023-03-26:
						// Questo scenario indica che per il DB "l'encoding è da eseguire" mentre abbiamo un Encoder che lo sta già
						// eseguendo Si tratta di una inconsistenza che non dovrebbe mai accadere. Oggi pero' ho visto questo
						// scenario e l'ho risolto facendo ripartire sia l'encoder che gli engines Gestire questo scenario
						// rende il sistema piu' robusto e recupera facilmente una situazione che altrimenti richiederebbe una
						// gestione manuale Inoltre senza guardare nel log, non si riuscirebbe a capire che siamo in questo scenario.

						// La gestione di questo scenario consiste nell'ignorare questa eccezione facendo andare avanti la procedura,
						// come se non avesse generato alcun errore
						SPDLOG_ERROR(
							"inconsistency: DB says the encoding has to be executed but the Encoder is already executing it. We will manage it"
							", _proxyIdentifier: {}"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", body: {}"
							", e.what: {}",
							_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, regex_replace(body, regex("\n"), " "),
							e.what()
						);
					}
					else
						throw e;
				}
			}
			else
			{
				SPDLOG_INFO(
					"LiveProxy/Recording. Selection of the transcoder. The transcoder is already saved (DB), the encoding is already running"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", encoderKey: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_encoderKey
				);

				tie(_currentUsedFFMpegEncoderHost, _currentUsedFFMpegExternalEncoder) = _mmsEngineDBFacade->getEncoderURL(_encodingItem->_encoderKey);
				_currentUsedFFMpegEncoderKey = _encodingItem->_encoderKey;

				// we have to reset _encodingItem->_encoderKey because in case we will come back in the above 'while' loop, we have to
				// select another encoder
				_encodingItem->_encoderKey = -1;

				ffmpegEncoderURL = fmt::format("{}{}/{}", _currentUsedFFMpegEncoderHost, ffmpegURI, _encodingItem->_encodingJobKey);
			}

			chrono::system_clock::time_point startEncoding = chrono::system_clock::now();

			{
				lock_guard<mutex> locker(*_mtEncodingJobs);

				*_status = EncodingJobStatus::Running;
			}

			SPDLOG_INFO(
				"Update EncodingJob"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", transcoder: {}"
				", _currentUsedFFMpegEncoderKey: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
			);
			_mmsEngineDBFacade->updateEncodingJobTranscoder(_encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderKey, "");

			if (!timePeriod)
			{
				try
				{
					double encodingProgress = -1.0; // it is a live

					SPDLOG_INFO(
						"updateEncodingJobProgress"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", encodingProgress: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingProgress
					);
					_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"updateEncodingJobProgress failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"updateEncodingJobProgress failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}

			// loop waiting the end of the encoding
			bool encodingFinished = false;
			bool completedWithError = false;
			string encodingErrorMessage;

			int encoderNotReachableFailures = 0;
			int lastEncodingPid = 0;
			long lastRealTimeFrameRate = 0;
			double lastRealTimeBitRate = 0;
			int encodingPid;
			long realTimeFrameRate;
			double realTimeBitRate;
			long lastNumberOfRestartBecauseOfFailure = 0;
			long numberOfRestartBecauseOfFailure;

			SPDLOG_INFO(
				"starting loop"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodingFinished: {}"
				", encoderNotReachableFailures: {}"
				", _maxEncoderNotReachableFailures: {}"
				", currentAttemptsNumberInCaseOfErrors: {}"
				", maxAttemptsNumberInCaseOfErrors: {}",
				_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingFinished, encoderNotReachableFailures,
				_maxEncoderNotReachableFailures, currentAttemptsNumberInCaseOfErrors, maxAttemptsNumberInCaseOfErrors
			);

			/*
			 Questo loop server per gestire e controllare l'encoding running nell'Encoder.
			 La condizione di uscita quindi dovrebbe basarsi solamente su encodingFinished ma ci sono dei casi come
			 ad esempio 'EncoderNotReachable' in cui è bene aspettare un po prima di abbandonare l'encoding per
			 essere sicuri che l'encoding sia effettivamente terminato nell'Encoder.
			 Non vogliamo quindi abbandonare l'encoding ed eventualmente attivare un nuovo encoding su un'altro
			 Encoder se non siamo sicurissimi che l'encoding attuale sia terminato.
			*/
			while (!(encodingFinished || encoderNotReachableFailures >= _maxEncoderNotReachableFailures))
			{
				SPDLOG_INFO(
					"sleep_for"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _intervalInSecondsToCheckEncodingFinished
				);
				this_thread::sleep_for(chrono::seconds(_intervalInSecondsToCheckEncodingFinished));

				try
				{
					tie(encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden, urlNotFound, ignore, encodingPid,
						realTimeFrameRate, realTimeBitRate, numberOfRestartBecauseOfFailure) = getEncodingStatus();
					SPDLOG_INFO(
						"getEncodingStatus"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", currentAttemptsNumberInCaseOfErrors: {}"
						", maxAttemptsNumberInCaseOfErrors: {}"
						", encodingFinished: {}"
						", killedByUser: {}"
						", completedWithError: {}"
						", encodingErrorMessage: {}"
						", urlForbidden: {}"
						", urlNotFound: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors,
						maxAttemptsNumberInCaseOfErrors, encodingFinished, killedByUser, completedWithError, encodingErrorMessage, urlForbidden,
						urlNotFound
					);
				}
				catch (EncoderNotReachable &e)
				{
					encoderNotReachableFailures++;

					// 2020-11-23. Scenario:
					//	1. I shutdown the encoder because I had to upgrade OS version
					//	2. this thread remained in this loop (while(!encodingFinished)) 		and the channel did not work
					// until the Encoder was working again 	In this scenario, so when the encoder is not reachable at all, the engine 	has
					// to select a new encoder. 	For this reason we added this EncoderNotReachable catch 	and the
					// encoderNotReachableFailures variable

					SPDLOG_ERROR(
						"Transcoder is not reachable at all, if continuing we will select another encoder"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", encoderNotReachableFailures: {}"
						", _maxEncoderNotReachableFailures: {}"
						", _currentUsedFFMpegEncoderHost: {}"
						", _currentUsedFFMpegEncoderKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encoderNotReachableFailures, _maxEncoderNotReachableFailures,
						_currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
					);

					continue;
				}
				catch (...)
				{
					encoderNotReachableFailures++;

					SPDLOG_ERROR(
						"getEncodingStatus failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", encoderNotReachableFailures: {}"
						", _maxEncoderNotReachableFailures: {}"
						", _currentUsedFFMpegEncoderHost: {}"
						", _currentUsedFFMpegEncoderKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encoderNotReachableFailures, _maxEncoderNotReachableFailures,
						_currentUsedFFMpegEncoderHost, _currentUsedFFMpegEncoderKey
					);

					continue;
				}

				try
				{
					// resetto encoderNotReachableFailures a 0. Continue 'exception' incrementano questa variabile
					// ma la prima volta in cui non abbiamo l'exception viene resettata a 0
					encoderNotReachableFailures = 0;

					// update encodingProgress/encodingPid/real time info
					{
						if (timePeriod)
						{
							time_t utcNow = chrono::system_clock::to_time_t(chrono::system_clock::now());

							double encodingProgress;

							if (utcNow < utcPeriodStart)
								encodingProgress = 0.0;
							else if (utcPeriodStart < utcNow && utcNow < utcPeriodEnd)
							{
								double elapsed = utcNow - utcPeriodStart;
								double proxyPeriod = utcPeriodEnd - utcPeriodStart;
								encodingProgress = (elapsed * 100) / proxyPeriod;
							}
							else
								encodingProgress = 100.0;

							SPDLOG_INFO(
								"updateEncodingJobProgress"
								", ingestionJobKey: {}"
								", encodingJobKey: {}"
								", encodingProgress: {}"
								", utcProxyPeriodStart: {}"
								", utcNow: {}"
								", utcProxyPeriodEnd: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, encodingProgress, utcPeriodStart, utcNow,
								utcPeriodEnd
							);
							_mmsEngineDBFacade->updateEncodingJobProgress(_encodingItem->_encodingJobKey, encodingProgress);
						}

						if (lastEncodingPid != encodingPid || lastRealTimeFrameRate != realTimeFrameRate || lastRealTimeBitRate != realTimeBitRate ||
							lastNumberOfRestartBecauseOfFailure != numberOfRestartBecauseOfFailure)
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
						else
						{
							SPDLOG_INFO(
								"encoderPid/bitrate/framerate check, not changed"
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

					if (encodingErrorMessage != "")
					{
						SPDLOG_ERROR(
							"Encoding failed (look the Transcoder logs)"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", _currentUsedFFMpegEncoderHost: {}"
							", completedWithError: {}"
							", encodingErrorMessage: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, completedWithError,
							encodingErrorMessage
						);

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

					// update currentAttemptsNumberInCaseOfErrors++
					{
						if (completedWithError)
							currentAttemptsNumberInCaseOfErrors++;
						else
							// ffmpeg is running successful, we will make sure currentAttemptsNumberInCaseOfErrors is reset
							currentAttemptsNumberInCaseOfErrors = 0;

						// update EncodingJob failures number to notify the GUI encodingJob is successful

						SPDLOG_INFO(
							"updateEncodingJobFailuresNumber"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", currentAttemptsNumberInCaseOfErrors: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
						);

						int64_t mediaItemKey = -1;
						int64_t encodedPhysicalPathKey = -1;
						_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
					}

					if (!killedByUser)
					{
						// secondo l'encoder l'encoding non è stato killato. Eseguo per essere sicuro anche
						// una verifica recuperando lo stato dell'IngestionJob
						string ingestionStatus = MMSEngineDBFacade::toString(
							_mmsEngineDBFacade->ingestionJob_Status(_encodingItem->_workspace->_workspaceKey, _encodingItem->_ingestionJobKey, true)
						);
						if (ingestionStatus.starts_with("End_"))
						{
							SPDLOG_INFO(
								"getEncodingStatus killedByUser is false but the ingestionJob is terminated"
								", _ingestionJobKey: {}"
								", _encodingJobKey: {}"
								", killedByUser: {}"
								", ingestionStatus: {}",
								_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, killedByUser, ingestionStatus
							);
							encodingFinished = true;
							killedByUser = true;
						}
					}
					if (!encodingFinished && (killedByUser || urlForbidden || urlNotFound))
						SPDLOG_ERROR(
							"Encoding was killedByUser or urlForbidden or urlNotFound but encodingFinished is false!!!"
							", _ingestionJobKey: {}"
							", _encodingJobKey: {}"
							", _currentUsedFFMpegEncoderHost: {}"
							", killedByUser: {}"
							", urlForbidden: {}"
							", urlNotFound: {}"
							", completedWithError: {}"
							", encodingErrorMessage: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _currentUsedFFMpegEncoderHost, killedByUser,
							urlForbidden, urlNotFound, completedWithError, encodingErrorMessage
						);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"management of the getEncodingJob result failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"management of the getEncodingJob result failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}

			chrono::system_clock::time_point endEncoding = chrono::system_clock::now();
			utcNowCheckToExit = chrono::system_clock::to_time_t(endEncoding);

			if (!timePeriod || (timePeriod && utcNowCheckToExit < utcPeriodEnd))
			{
				SPDLOG_ERROR(
					"LiveProxy/Recording media file completed unexpected"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", still remaining seconds (utcProxyPeriodEnd - utcNow): {}"
					", ffmpegEncoderURL: {}"
					", encodingFinished: {}"
					", killedByUser: {}"
					", @MMS statistics@ - encodingDuration (secs): @{}@"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcPeriodEnd - utcNowCheckToExit,
					ffmpegEncoderURL, encodingFinished, killedByUser, chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count(),
					_intervalInSecondsToCheckEncodingFinished
				);

				try
				{
					string errorMessage = DateTime::utcToLocalString(chrono::system_clock::to_time_t(chrono::system_clock::now())) +
										  " LiveProxy/Recording media file completed unexpected";

					string firstLineOfErrorMessage;
					{
						string firstLine;
						stringstream ss(errorMessage);
						if (getline(ss, firstLine))
							firstLineOfErrorMessage = firstLine;
						else
							firstLineOfErrorMessage = errorMessage;
					}

					_mmsEngineDBFacade->appendIngestionJobErrorMessage(_encodingItem->_ingestionJobKey, firstLineOfErrorMessage);
				}
				catch (runtime_error &e)
				{
					SPDLOG_ERROR(
						"appendIngestionJobErrorMessage failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
				catch (exception &e)
				{
					SPDLOG_ERROR(
						"appendIngestionJobErrorMessage failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", e.what(): {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
					);
				}
			}
			else
			{
				SPDLOG_INFO(
					"LiveProxy/Recording media file completed"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", ffmpegEncoderURL: {}"
					", encodingFinished: {}"
					", killedByUser: {}"
					", @MMS statistics@ - encodingDuration (secs): @{}@"
					", _intervalInSecondsToCheckEncodingFinished: {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL, encodingFinished,
					killedByUser, chrono::duration_cast<chrono::seconds>(endEncoding - startEncoding).count(),
					_intervalInSecondsToCheckEncodingFinished
				);

				if (encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
				{
					json recordingPeriodRoot = (_encodingItem->_ingestedParametersRoot)["schedule"];
					bool autoRenew = JSONUtils::asBool(recordingPeriodRoot, "autoRenew", false);
					if (autoRenew)
					{
						SPDLOG_INFO(
							"Renew Live Recording"
							", ingestionJobKey: {}",
							_encodingItem->_ingestionJobKey
						);

						time_t recordingPeriodInSeconds = utcPeriodEnd - utcPeriodStart;

						utcPeriodStart = utcPeriodEnd;
						utcPeriodEnd += recordingPeriodInSeconds;

						SPDLOG_INFO(
							"Update Encoding LiveRecording Period"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", utcRecordingPeriodStart: {}"
							", utcRecordingPeriodEnd: {}",
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcPeriodStart, utcPeriodEnd
						);
						_mmsEngineDBFacade->updateIngestionAndEncodingLiveRecordingPeriod(
							_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, utcPeriodStart, utcPeriodEnd
						);

						// next update is important because the JSON is used in the
						// getEncodingProgress method 2022-11-09: I do not call
						// anymore getEncodingProgress 2022-11-20: next update is
						// mandatory otherwise we will have the folloging error:
						//		FFMpeg.cpp:8679: LiveRecorder timing. Too late to
						// start the LiveRecorder
						{
							_encodingItem->_encodingParametersRoot["utcScheduleStart"] = utcPeriodStart;
							_encodingItem->_encodingParametersRoot["utcScheduleEnd"] = utcPeriodEnd;
						}
					}
				}
			}
		}
		catch (YouTubeURLNotRetrieved &e)
		{
			SPDLOG_ERROR(
				"YouTubeURLNotRetrieved"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, responseInitialized ? response.str() : "", e.what()
			);

			// in this case we will through the exception independently if the live streaming time (utcRecordingPeriodEnd)
			// is finished or not. This task will come back by the MMS system
			throw e;
		}
		catch (EncoderNotFound e)
		{
			SPDLOG_ERROR(
				"Encoder not found"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
				responseInitialized ? response.str() : "", e.what()
			);

			// update EncodingJob failures number to notify the GUI EncodingJob
			// is failing
			try
			{
				// 2021-02-12: scenario, encodersPool does not exist, a runtime_error is generated contiuosly. The task will never
				// exist from this loop because currentAttemptsNumberInCaseOfErrors always remain to 0 and
				// the main loop look currentAttemptsNumberInCaseOfErrors. So added currentAttemptsNumberInCaseOfErrors++
				currentAttemptsNumberInCaseOfErrors++;

				SPDLOG_INFO(
					"updateEncodingJobFailuresNumber"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", currentAttemptsNumberInCaseOfErrors: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				SPDLOG_ERROR(
					"updateEncodingJobFailuresNumber failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			utcNowCheckToExit = chrono::system_clock::to_time_t(chrono::system_clock::now());

			// throw e;
		}
		catch (MaxConcurrentJobsReached &e)
		{
			SPDLOG_WARN(
				"MaxConcurrentJobsReached"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, responseInitialized ? response.str() : "", e.what()
			);

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
				SPDLOG_WARN(
					"No Encodings available / MaxConcurrentJobsReached"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", e.what(): {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, e.what()
				);

				throw MaxConcurrentJobsReached();
			}
			else
			{
				SPDLOG_ERROR(
					"Encoding URL failed/runtime_error"
					", _proxyIdentifier: {}"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", ffmpegEncoderURL: {}"
					", response.str: {}"
					", e.what(): {}",
					_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
					responseInitialized ? response.str() : "", e.what()
				);

				// update EncodingJob failures number to notify the GUI EncodingJob is failing
				try
				{
					// 2021-02-12: scenario, encodersPool does not exist, a runtime_error is generated contiuosly. The task will
					// never exist from this loop because currentAttemptsNumberInCaseOfErrors always remain to 0
					// and the main loop look currentAttemptsNumberInCaseOfErrors. So added currentAttemptsNumberInCaseOfErrors++
					currentAttemptsNumberInCaseOfErrors++;

					SPDLOG_INFO(
						"updateEncodingJobFailuresNumber"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}"
						", currentAttemptsNumberInCaseOfErrors: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
					);

					int64_t mediaItemKey = -1;
					int64_t encodedPhysicalPathKey = -1;
					_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
				}
				catch (...)
				{
					SPDLOG_ERROR(
						"updateEncodingJobFailuresNumber failed"
						", _ingestionJobKey: {}"
						", _encodingJobKey: {}",
						_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
					);
				}

				// sleep a bit and try again
				int sleepTime = 30;
				this_thread::sleep_for(chrono::seconds(sleepTime));

				utcNowCheckToExit = chrono::system_clock::to_time_t(chrono::system_clock::now());

				// throw e;
			}
		}
		catch (exception e)
		{
			SPDLOG_ERROR(
				"Encoding URL failed (exception)"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", ffmpegEncoderURL: {}"
				", response.str: {}"
				", e.what(): {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, ffmpegEncoderURL,
				responseInitialized ? response.str() : "", e.what()
			);

			// update EncodingJob failures number to notify the GUI EncodingJob is failing
			try
			{
				currentAttemptsNumberInCaseOfErrors++;

				SPDLOG_INFO(
					"updateEncodingJobFailuresNumber"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}"
					", currentAttemptsNumberInCaseOfErrors: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors
				);

				int64_t mediaItemKey = -1;
				int64_t encodedPhysicalPathKey = -1;
				_mmsEngineDBFacade->updateEncodingJobFailuresNumber(_encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors);
			}
			catch (...)
			{
				SPDLOG_ERROR(
					"updateEncodingJobFailuresNumber failed"
					", _ingestionJobKey: {}"
					", _encodingJobKey: {}",
					_encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
				);
			}

			// sleep a bit and try again
			int sleepTime = 30;
			this_thread::sleep_for(chrono::seconds(sleepTime));

			utcNowCheckToExit = chrono::system_clock::to_time_t(chrono::system_clock::now());

			// throw e;
		}
	}

	if (urlForbidden)
	{
		SPDLOG_ERROR(
			"LiveProxy/Recording: URL forbidden"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
		);

		throw FFMpegURLForbidden();
	}
	else if (urlNotFound)
	{
		SPDLOG_ERROR(
			"LiveProxy/Recording: URL Not Found"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey
		);

		throw FFMpegURLNotFound();
	}
	else if (currentAttemptsNumberInCaseOfErrors >= maxAttemptsNumberInCaseOfErrors)
	{
		SPDLOG_ERROR(
			"Reached the max number of attempts to the URL"
			", _proxyIdentifier: {}"
			", _ingestionJobKey: {}"
			", _encodingJobKey: {}"
			", currentAttemptsNumberInCaseOfErrors: {}"
			", maxAttemptsNumberInCaseOfErrors: {}",
			_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, currentAttemptsNumberInCaseOfErrors,
			maxAttemptsNumberInCaseOfErrors
		);

		throw EncoderError();
	}
	else if (encodingType == MMSEngineDBFacade::EncodingType::LiveRecorder)
	{
		long ingestionJobOutputsCount = _mmsEngineDBFacade->getIngestionJobOutputsCount(
			_encodingItem->_ingestionJobKey,
			// 2022-12-18: true because IngestionJobOutputs was updated
			// recently
			true
		);
		if (ingestionJobOutputsCount <= 0)
		{
			string errorMessage = fmt::format(
				"LiveRecorder: no chunks were generated"
				", _proxyIdentifier: {}"
				", _ingestionJobKey: {}"
				", _encodingJobKey: {}"
				", _workspace->_directoryName: {}"
				", ingestionJobOutputsCount: {}",
				_proxyIdentifier, _encodingItem->_ingestionJobKey, _encodingItem->_encodingJobKey, _encodingItem->_workspace->_directoryName,
				ingestionJobOutputsCount
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	return killedByUser;
}
