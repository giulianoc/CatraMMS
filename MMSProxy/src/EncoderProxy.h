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

#ifndef EncoderProxy_h
#define EncoderProxy_h
#include "FFMpegWrapper.h"

#ifdef __LOCALENCODER__
#include "FFMpeg.h"
#endif
#include "EncodersLoadBalancer.h"
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "Magick++.h"
#include "MultiEventsSet.h"
#include "spdlog/spdlog.h"

#define ENCODERPROXY "EncoderProxy"
#define MMSENGINEPROCESSORNAME "MMSEngineProcessor"

struct EncoderError : public exception
{
	char const *what() const throw() { return "Encoder error"; };
};

struct EncodingKilledByUser : public exception
{
	char const *what() const throw() { return "Encoding was killed by the User"; };
};

struct EncoderNotReachable : public exception
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
			throw runtime_error(string("Wrong encodingJobStatus"));
		}
	}

	EncoderProxy();

	virtual ~EncoderProxy();

	void init(
		int proxyIdentifier, mutex *mtEncodingJobs, json configuration, shared_ptr<MultiEventsSet> multiEventsSet,
		shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage, shared_ptr<EncodersLoadBalancer> encodersLoadBalancer,
		shared_ptr<long> faceRecognitionNumber,
		int maxFaceRecognitionNumber
#ifdef __LOCALENCODER__
		int *pRunningEncodingsNumber,
#else
#endif
	);

	void setEncodingData(EncodingJobStatus *status, shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem);

	void operator()();

	// int getEncodingProgress();

  private:
	int _proxyIdentifier;
	json _configuration;
	mutex *_mtEncodingJobs;
	EncodingJobStatus *_status;
	shared_ptr<MultiEventsSet> _multiEventsSet;
	shared_ptr<MMSEngineDBFacade> _mmsEngineDBFacade;
	shared_ptr<MMSStorage> _mmsStorage;
	shared_ptr<EncodersLoadBalancer> _encodersLoadBalancer;
	shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
	string _hostName;

	shared_ptr<long> _faceRecognitionNumber;
	int _maxFaceRecognitionNumber;

	int _maxSecondsToWaitUpdateEncodingJobLock;

	string _mp4Encoder;
	string _mpeg2TSEncoder;
	int _intervalInSecondsToCheckEncodingFinished;

	string _ffmpegEncoderUser;
	string _ffmpegEncoderPassword;
	int _ffmpegEncoderTimeoutInSeconds;
	string _ffmpegEncoderProgressURI;
	string _ffmpegEncoderStatusURI;
	string _ffmpegEncoderKillEncodingURI;
	string _ffmpegEncodeURI;
	string _ffmpegOverlayImageOnVideoURI;
	string _ffmpegOverlayTextOnVideoURI;
	string _ffmpegGenerateFramesURI;
	string _ffmpegSlideShowURI;
	string _ffmpegLiveRecorderURI;
	string _ffmpegLiveProxyURI;
	string _ffmpegLiveGridURI;
	string _ffmpegVideoSpeedURI;
	string _ffmpegAddSilentAudioURI;
	string _ffmpegPictureInPictureURI;
	string _ffmpegIntroOutroOverlayURI;
	string _ffmpegCutFrameAccurateURI;

	int _timeBeforeToPrepareResourcesInMinutes;

	int _waitingNFSSync_maxMillisecondsToWait;
	int _waitingNFSSync_milliSecondsWaitingBetweenChecks;

	long _retrieveStreamingYouTubeURLPeriodInHours;
	int _maxEncoderNotReachableFailures;

	string _keyPairId;
	string _privateKeyPEMPathName;

#ifdef __LOCALENCODER__
	shared_ptr<FFMpeg> _ffmpeg;
	int *_pRunningEncodingsNumber;
	int _ffmpegMaxCapacity;
#else
	string _currentUsedFFMpegEncoderHost;
	int64_t _currentUsedFFMpegEncoderKey;
	bool _currentUsedFFMpegExternalEncoder;
#endif

	// used only in case of face recognition/identification video generation
	double _localEncodingProgress;

	string _computerVisionCascadePath;
	double _computerVisionDefaultScale;
	int _computerVisionDefaultMinNeighbors;
	bool _computerVisionDefaultTryFlip;

	void encodeContentImage();
	void processEncodedImage();
	tuple<string, int, int, bool, int, int, Magick::InterlaceType> readingImageProfile(json encodingProfileRoot);
	void encodingImageFormatValidation(string newFormat);
	Magick::InterlaceType encodingImageInterlaceTypeValidation(string sNewInterlaceType);

	void encodeContentVideoAudio(string ffmpegURI, int maxConsecutiveEncodingStatusFailures);
	FFMpegWrapper::KillType encodeContent_VideoAudio_through_ffmpeg(string ffmpegURI, int maxConsecutiveEncodingStatusFailures);
	void processEncodedContentVideoAudio();

	void processOverlayedImageOnVideo(FFMpegWrapper::KillType killTypeReceived);

	void processOverlayedTextOnVideo(FFMpegWrapper::KillType killTypeReceived);

	void processGeneratedFrames(FFMpegWrapper::KillType killTypeReceived);

	void processSlideShow();

	string faceRecognition();
	void processFaceRecognition(string stagingEncodedAssetPathName);

	string faceIdentification();
	void processFaceIdentification(string stagingEncodedAssetPathName);

	FFMpegWrapper::KillType liveRecorder();
	FFMpegWrapper::KillType liveRecorder_through_ffmpeg();
	void processLiveRecorder(FFMpegWrapper::KillType killTypeReceived);

	FFMpegWrapper::KillType liveProxy(MMSEngineDBFacade::EncodingType encodingType);
	FFMpegWrapper::KillType liveProxy_through_ffmpeg(MMSEngineDBFacade::EncodingType encodingType);
	void processLiveProxy(FFMpegWrapper::KillType killTypeReceived);

	void processLiveGrid(FFMpegWrapper::KillType killTypeReceived);

	void processVideoSpeed(FFMpegWrapper::KillType killTypeReceived);

	void processAddSilentAudio(FFMpegWrapper::KillType killTypeReceived);

	void processPictureInPicture(FFMpegWrapper::KillType killTypeReceived);

	void processIntroOutroOverlay();

	void processCutFrameAccurate();

	tuple<bool, FFMpegWrapper::KillType, bool, json, bool, bool, optional<double>, int, json, long> getEncodingStatus();

	string generateMediaMetadataToIngest(
		int64_t ingestionJobKey, string fileFormat, int64_t faceOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey, double startTimeInSeconds,
		double endTimeInSeconds, vector<int64_t> slideShowOfImageMediaItemKeys, vector<int64_t> slideShowOfAudioMediaItemKeys, json parametersRoot
	);

	// void killEncodingJob(string transcoderHost, int64_t encodingJobKey);
	void awsStartChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted);

	void awsStopChannel(int64_t ingestionJobKey, string awsChannelIdToBeStarted);

	FFMpegWrapper::KillType waitingEncoding(int maxConsecutiveEncodingStatusFailures);
	FFMpegWrapper::KillType waitingLiveProxyOrLiveRecorder(
		MMSEngineDBFacade::EncodingType encodingType, string ffmpegURI, bool timePeriod, time_t utcPeriodStart, time_t utcPeriodEnd,
		uint32_t maxAttemptsNumberInCaseOfErrors, string ipPushStreamConfigurationLabel
	);
};

#endif
