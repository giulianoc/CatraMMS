/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   EnodingsManager.h
 * Author: giuliano
 *
 * Created on February 4, 2018, 7:18 PM
 */

#pragma once

#include
#include
#include "FFMpegWrapper.h"

#ifdef __LOCALENCODER__
#include "FFMpeg.h"
#endif
#include "EncodersLoadBalancer.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "Magick++.h"
#include "MultiEventsSet.h"
#include "spdlog/spdlog.h"

#define ENCODERPROXY "EncoderProxy"
#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

struct EncoderError : public std::exception
{
	char const *what() const throw() { return "Encoder error"; };
};

struct EncodingKilledByUser : public std::exception
{
	char const *what() const throw() { return "Encoding was killed by the User"; };
};

struct EncoderNotReachable : public std::exception
{
	char const *what() const throw() { return "Encoder not reachable"; };
};

/*
struct EncodingStatusNotAvailable: public exception {
	char const* what() const throw()
	{
		return "Encoding status not available";
	};
};
*/

class EncoderProxy
{
  public:
	enum class EncodingJobStatus
	{
		Free,
		ToBeRun,
		GoingToRun, // EncoderProxy thread created but still not confirmed by Encoder process (ffmpegEncoder.fcgi)
		Running
	};
	static const char *toString(const EncodingJobStatus &encodingJobStatus)
	{
		switch (encodingJobStatus)
		{
		case EncodingJobStatus::Free:
			return "Free";
		case EncodingJobStatus::ToBeRun:
			return "ToBeRun";
		case EncodingJobStatus::GoingToRun:
			return "GoingToRun";
		case EncodingJobStatus::Running:
			return "Running";
		default:
			throw std::runtime_error(std::string("Wrong encodingJobStatus"));
		}
	}

	EncoderProxy();

	virtual ~EncoderProxy();

	void init(
		int proxyIdentifier, std::mutex *mtEncodingJobs, const nlohmann::json &configuration, const std::shared_ptr<MultiEventsSet> &multiEventsSet,
		const std::shared_ptr<MMSEngineDBFacade> &mmsEngineDBFacade, const std::shared_ptr<MMSStorage> &mmsStorage,
		const std::shared_ptr<EncodersLoadBalancer> &encodersLoadBalancer, const std::shared_ptr<long> &faceRecognitionNumber,
		int maxFaceRecognitionNumber
#ifdef __LOCALENCODER__
		int *pRunningEncodingsNumber,
#else
#endif
	);

	void setEncodingData(EncodingJobStatus *status, const std::shared_ptr<MMSEngineDBFacade::EncodingItem> &encodingItem);

	void operator()();

	// int getEncodingProgress();

  private:
	int _proxyIdentifier;
	nlohmann::json _configuration;
	std::mutex *_mtEncodingJobs;
	EncodingJobStatus *_status;
	std::shared_ptr<MultiEventsSet> _multiEventsSet;
	std::shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	std::shared_ptr<MMSStorage> _mmsStorage;
	std::shared_ptr<EncodersLoadBalancer> _encodersLoadBalancer;
	std::shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
	std::string _hostName;

	std::shared_ptr<long> _faceRecognitionNumber;
	int _maxFaceRecognitionNumber;

	int _maxSecondsToWaitUpdateEncodingJobLock;

	std::string _mp4Encoder;
	std::string _mpeg2TSEncoder;
	int _intervalInSecondsToCheckEncodingFinished;

	std::string _ffmpegEncoderUser;
	std::string _ffmpegEncoderPassword;
	int _ffmpegEncoderTimeoutInSeconds;
	std::string _ffmpegEncoderProgressURI;
	std::string _ffmpegEncoderStatusURI;
	std::string _ffmpegEncoderKillEncodingURI;
	std::string _ffmpegEncodeURI;
	std::string _ffmpegOverlayImageOnVideoURI;
	std::string _ffmpegOverlayTextOnVideoURI;
	std::string _ffmpegGenerateFramesURI;
	std::string _ffmpegSlideShowURI;
	std::string _ffmpegLiveRecorderURI;
	std::string _ffmpegLiveProxyURI;
	std::string _ffmpegLiveGridURI;
	std::string _ffmpegVideoSpeedURI;
	std::string _ffmpegAddSilentAudioURI;
	std::string _ffmpegPictureInPictureURI;
	std::string _ffmpegIntroOutroOverlayURI;
	std::string _ffmpegCutFrameAccurateURI;

	int _timeBeforeToPrepareResourcesInMinutes;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks;

	long _retrieveStreamingYouTubeURLPeriodInHours;
	int _maxEncoderNotReachableFailures;

	std::string _keyPairId;
	std::string _privateKeyPEMPathName;

#ifdef __LOCALENCODER__
	shared_ptr<FFMpeg> _ffmpeg;
	int *_pRunningEncodingsNumber;
	int _ffmpegMaxCapacity;
#else
	std::string _currentUsedFFMpegEncoderHost;
	int64_t _currentUsedFFMpegEncoderKey;
	bool _currentUsedFFMpegExternalEncoder;
#endif

	// used only in case of face recognition/identification video generation
	double _localEncodingProgress;

	std::string _computerVisionCascadePath;
	double _computerVisionDefaultScale;
	int _computerVisionDefaultMinNeighbors;
	bool _computerVisionDefaultTryFlip;

	void encodeContentImage();
	void processEncodedImage();
	std::tuple<std::string, int, int, bool, int, int, Magick::InterlaceType> readingImageProfile(nlohmann::json encodingProfileRoot);
	static void encodingImageFormatValidation(const std::string &newFormat);
	static Magick::InterlaceType encodingImageInterlaceTypeValidation(const std::string &sNewInterlaceType);

	void encodeContentVideoAudio(std::string ffmpegURI, int maxConsecutiveEncodingStatusFailures);
	bool encodeContent_VideoAudio_through_ffmpeg(std::string ffmpegURI, int maxConsecutiveEncodingStatusFailures);
	void processEncodedContentVideoAudio();

	void processOverlayedImageOnVideo(bool killed);

	void processOverlayedTextOnVideo(bool killed);

	void processGeneratedFrames(bool killed);

	void processSlideShow();

	std::string faceRecognition();
	void processFaceRecognition(std::string stagingEncodedAssetPathName);

	std::string faceIdentification();
	void processFaceIdentification(std::string stagingEncodedAssetPathName);

	bool liveRecorder();
	bool liveRecorder_through_ffmpeg();
	void processLiveRecorder(bool killed);

	bool liveProxy(MMSEngineDBFacade::EncodingType encodingType);
	bool liveProxy_through_ffmpeg(MMSEngineDBFacade::EncodingType encodingType);
	void processLiveProxy(bool killed);

	void processLiveGrid(bool killed);

	void processVideoSpeed(bool killed);

	void processAddSilentAudio(bool killed);

	void processPictureInPicture(bool killed);

	void processIntroOutroOverlay();

	void processCutFrameAccurate();

	std::tuple<bool, bool, FFMpegWrapper::KillType, bool, nlohmann::json, bool, bool, std::optional<double>, int, nlohmann::json, long> getEncodingStatus();

	std::string generateMediaMetadataToIngest(
		int64_t ingestionJobKey, std::string fileFormat, int64_t faceOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey, double startTimeInSeconds,
		double endTimeInSeconds, std::vector<long long> slideShowOfImageMediaItemKeys, std::vector<int64_t> slideShowOfAudioMediaItemKeys,
		nlohmann::json parametersRoot
	);

	// void killEncodingJob(std::string transcoderHost, int64_t encodingJobKey);
	void awsStartChannel(int64_t ingestionJobKey, std::string awsChannelIdToBeStarted);

	void awsStopChannel(int64_t ingestionJobKey, std::string awsChannelIdToBeStarted);

	bool waitingEncoding(int maxConsecutiveEncodingStatusFailures);
	bool waitingLiveProxyOrLiveRecorder(
		MMSEngineDBFacade::EncodingType encodingType, std::string ffmpegURI, bool timePeriod, time_t utcPeriodStart, time_t utcPeriodEnd,
		uint32_t maxAttemptsNumberInCaseOfErrors, std::string ipPushStreamConfigurationLabel
	);
};
