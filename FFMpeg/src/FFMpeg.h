/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   FFMPEGEncoder.h
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#ifndef FFMpeg_h
#define FFMpeg_h

#include <string>
#include <filesystem>
#include <chrono>
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include "spdlog/spdlog.h"
#include "json/json.h"

#ifndef __FILEREF__
    #ifdef __APPLE__
        #define __FILEREF__ string("[") + string(__FILE__).substr(string(__FILE__).find_last_of("/") + 1) + ":" + to_string(__LINE__) + "] "
    #else
        #define __FILEREF__ string("[") + basename((char *) __FILE__) + ":" + to_string(__LINE__) + "] "
    #endif
#endif

using namespace std;
namespace fs = std::filesystem;

struct FFMpegEncodingStatusNotAvailable: public exception {
    char const* what() const throw() 
    {
        return "Encoding status not available";
    }; 
};

struct FFMpegFrameInfoNotAvailable: public exception {
    char const* what() const throw() 
    {
        return "Frame Info not available";
    }; 
};

struct FFMpegEncodingKilledByUser: public exception {
    char const* what() const throw() 
    {
        return "Encoding was killed by the User";
    }; 
};

struct FFMpegURLForbidden: public exception {
    char const* what() const throw() 
    {
        return "URL Forbidden";
    }; 
};

struct FFMpegURLNotFound: public exception {
    char const* what() const throw() 
    {
        return "URL Not Found";
    }; 
};

struct NoEncodingJobKeyFound: public exception {
    char const* what() const throw() 
    {
        return "No encoding job key found";
    }; 
};

struct NoEncodingAvailable: public exception {
    char const* what() const throw() 
    {
        return "No encoding available";
    }; 
};

struct MaxConcurrentJobsReached: public exception {
    char const* what() const throw() 
    {
        return "Encoder reached the max number of concurrent jobs";
    }; 
};

struct EncodingIsAlreadyRunning: public exception {
    char const* what() const throw() 
    {
        return "Encoding is already running";
    }; 
};

class FFMpeg {
public:
    FFMpeg(Json::Value configuration,
            shared_ptr<spdlog::logger> logger);
    
    ~FFMpeg();

    void encodeContent(
        string mmsSourceAssetPathName,
        int64_t durationInMilliSeconds,
        string stagingEncodedAssetPathName,
        Json::Value encodingProfileDetailsRoot,
        bool isVideo,   // if false it means is audio
		Json::Value videoTracksRoot,
		Json::Value audioTracksRoot,
		int videoTrackIndexToBeUsed, int audioTrackIndexToBeUsed,
        int64_t physicalPathKey,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);
    
    void overlayImageOnVideo(
		bool externalEncoder,
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,
        string mmsSourceImageAssetPathName,
        string imagePosition_X_InPixel,
        string imagePosition_Y_InPixel,
        // string encodedFileName,
        string stagingEncodedAssetPathName,
		Json::Value encodingProfileDetailsRoot,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);


	void overlayTextOnVideo(
		string mmsSourceVideoAssetPathName,
		int64_t videoDurationInMilliSeconds,

		Json::Value drawTextDetailsRoot,
		Json::Value encodingProfileDetailsRoot,
		string stagingEncodedAssetPathName,
		int64_t encodingJobKey,
		int64_t ingestionJobKey,
		pid_t* pChildPid);

	void videoSpeed(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,

        string videoSpeedType,
        int videoSpeedSize,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void pictureInPicture(
        string mmsMainVideoAssetPathName,
        int64_t mainVideoDurationInMilliSeconds,
        string mmsOverlayVideoAssetPathName,
        int64_t overlayVideoDurationInMilliSeconds,
        bool soundOfMain,
        string overlayPosition_X_InPixel,
        string overlayPosition_Y_InPixel,
        string overlay_Width_InPixel,
        string overlay_Height_InPixel,
		Json::Value encodingProfileDetailsRoot,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void introOutroOverlay(
        string introVideoAssetPathName,
        int64_t introVideoDurationInMilliSeconds,
        string mainVideoAssetPathName,
        int64_t mainVideoDurationInMilliSeconds,
        string outroVideoAssetPathName,
        int64_t outroVideoDurationInMilliSeconds,

		int64_t introOverlayDurationInSeconds,
		int64_t outroOverlayDurationInSeconds,

		bool muteIntroOverlay,
		bool muteOutroOverlay,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void introOverlay(
        string introVideoAssetPathName,
        int64_t introVideoDurationInMilliSeconds,
        string mainVideoAssetPathName,
        int64_t mainVideoDurationInMilliSeconds,

		int64_t introOverlayDurationInSeconds,

		bool muteIntroOverlay,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void outroOverlay(
        string mainVideoAssetPathName,
        int64_t mainVideoDurationInMilliSeconds,
        string outroVideoAssetPathName,
        int64_t outroVideoDurationInMilliSeconds,

		int64_t outroOverlayDurationInSeconds,

		bool muteOutroOverlay,

		Json::Value encodingProfileDetailsRoot,

        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void silentAudio(
		string videoAssetPathName,
		int64_t videoDurationInMilliSeconds,
		string addType,	// entireTrack, begin, end
		int seconds,
		Json::Value encodingProfileDetailsRoot,
		string stagingEncodedAssetPathName,
		int64_t encodingJobKey,
		int64_t ingestionJobKey,
		pid_t* pChildPid);

    double getEncodingProgress();

	bool nonMonotonousDTSInOutputLog();
	bool forbiddenErrorInOutputLog();
	bool isFrameIncreasing(int maxMilliSecondsToWait);

	tuple<int64_t, long, Json::Value> getMediaInfo(int64_t ingestionJobKey,
		bool isMMSAssetPathName, int timeoutInSeconds, string mediaSource,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks);

	string getNearestKeyFrameTime(
		int64_t ingestionJobKey,
		string mediaSource,
		string readIntervals,	// intervallo dove cercare il key frame piu vicino
		double keyFrameTime
	);

	int probeChannel(
		int64_t ingestionJobKey,
		string url);

	void muxAllFiles(int64_t ingestionJobKey, vector<string> sourcesPathName,
		string destinationPathName);

	void getLiveStreamingInfo(
		string liveURL,
		string userAgent,
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		vector<tuple<int, string, string, string, string, int, int>>& videoTracks,
		vector<tuple<int, string, string, string, int, bool>>& audioTracks
	);

	void generateFrameToIngest(
		int64_t ingestionJobKey,
		string mmsAssetPathName,
		int64_t videoDurationInMilliSeconds,
		double startTimeInSeconds,
		string frameAssetPathName,
		int imageWidth,
		int imageHeight,
		pid_t* pChildPid);

    void generateFramesToIngest(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        string imageDirectory,
        string imageBaseFileName,
        double startTimeInSeconds,
        int framesNumber,
        string videoFilter,
        int periodInSeconds,
        bool mjpeg,
        int imageWidth,
        int imageHeight,
        string mmsAssetPathName,
        int64_t videoDurationInMilliSeconds,
		pid_t* pChildPid);

    void concat(
        int64_t ingestionJobKey,
		bool isVideo,
        vector<string>& sourcePhysicalPaths,
        string concatenatedMediaPathName);

	void splitVideoInChunks(
		int64_t ingestionJobKey,
		string sourcePhysicalPath,
		long chunksDurationInSeconds,
		string chunksDirectory,
		string chunkBaseFileName);

	void slideShow(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		float durationOfEachSlideInSeconds, 
		string frameRateMode,
		Json::Value encodingProfileDetailsRoot,
		vector<string>& imagesSourcePhysicalPaths,
		vector<string>& audiosSourcePhysicalPaths,
		float shortestAudioDurationInSeconds,	// the shortest duration among the audios
		string encodedStagingAssetPathName,
		pid_t* pChildPid);

	void cutWithoutEncoding(
		int64_t ingestionJobKey,
		string sourcePhysicalPath,
		bool isVideo,
		int sourceFramesPerSecond,
		string cutType,	// KeyFrameSeeking (input seeking), FrameAccurateWithoutEncoding, KeyFrameSeekingInterval
		string startKeyFramesSeekingInterval,
		string endKeyFramesSeekingInterval,
		string startTime,
		string endTime,
		int framesNumber,
		string cutMediaPathName);

	void cutFrameAccurateWithEncoding(
		int64_t ingestionJobKey,
		string sourceVideoAssetPathName,
		// no keyFrameSeeking needs reencoding otherwise the key frame is always used
		// If you re-encode your video when you cut/trim, then you get a frame-accurate cut
		// because FFmpeg will re-encode the video and start with an I-frame.
		int sourceFramesPerSecond,
		int64_t encodingJobKey,
		Json::Value encodingProfileDetailsRoot,
		string startTime,
		string endTime,
		int framesNumber,
		string stagingEncodedAssetPathName,
		pid_t* pChildPid);

    void extractTrackMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        vector<pair<string,int>>& tracksToBeExtracted,
        string extractTrackMediaPathName);

	void liveRecorder(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
		bool externalEncoder,
		string segmentListPathName,
		string recordedFileNamePrefix,

		string otherInputOptions,

		string streamSourceType,
        string liveURL,
		int listenTimeoutInSeconds,
		int captureLive_videoDeviceNumber,
		string captureLive_videoInputFormat,
		int captureLive_frameRate,
		int captureLive_width,
		int captureLive_height,
		int captureLive_audioDeviceNumber,
		int captureLive_channelsNumber,

		string userAgent,
        time_t utcRecordingPeriodStart, 
        time_t utcRecordingPeriodEnd, 

        int segmentDurationInSeconds,
        string outputFileFormat,
		string segmenterType,

		Json::Value outputsRoot,

		Json::Value picturePathNamesToBeDetectedRoot,

		pid_t* pChildPid,
		chrono::system_clock::time_point* pRecordingStart);

	void liveRecorder2(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
		bool externalEncoder,
		string segmentListPathName,
		string recordedFileNamePrefix,

		string otherInputOptions,

		string streamSourceType,
        string liveURL,
		int listenTimeoutInSeconds,
		int captureLive_videoDeviceNumber,
		string captureLive_videoInputFormat,
		int captureLive_frameRate,
		int captureLive_width,
		int captureLive_height,
		int captureLive_audioDeviceNumber,
		int captureLive_channelsNumber,

		string userAgent,
        time_t utcRecordingPeriodStart, 
        time_t utcRecordingPeriodEnd, 

        int segmentDurationInSeconds,
        string outputFileFormat,
		string segmenterType,

		Json::Value outputsRoot,

		Json::Value picturePathNamesToBeDetectedRoot,

		pid_t* pChildPid,
		chrono::system_clock::time_point* pRecordingStart);

	void liveProxy2(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		bool externalEncoder,

		mutex* inputsRootMutex,
		Json::Value* inputsRoot,

		Json::Value outputsRoot,

		pid_t* pChildPid,
		chrono::system_clock::time_point* pProxyStart
	);

	void liveGrid(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		bool externalEncoder,
		string userAgent,
		Json::Value inputChannelsRoot,	// name,url
		int gridColumns,
		int gridWidth,	// i.e.: 1024
		int gridHeight, // i.e.: 578

		Json::Value outputsRoot,

		pid_t* pChildPid);

	void changeFileFormat(
		int64_t ingestionJobKey,
		int64_t physicalPathKey,
		string sourcePhysicalPath,
		vector<tuple<int64_t, int, int64_t, int, int, string, string, long,
			string>>& sourceVideoTracks,
		vector<tuple<int64_t, int, int64_t, long, string, long, int, string>>& sourceAudioTracks,
		string destinationPathName,
		string outputFileFormat);

	void streamingToFile(
		int64_t ingestionJobKey,
		bool regenerateTimestamps,
		string sourceReferenceURL,
		string destinationPathName);

    static void encodingVideoCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger);

	pair<string, string> retrieveStreamingYouTubeURL(
		int64_t ingestionJobKey,
		string youTubeURL);

	static bool isNumber(int64_t ingestionJobKey, string number);
	static double timeToSeconds(int64_t ingestionJobKey, string time,
		int framesPerSecond);
	static string secondsToTime(int64_t ingestionJobKey, double dSeconds,
		shared_ptr<spdlog::logger> logger);

private:
	enum class APIName {
		EncodeContent					= 0,
		OverlayImageOnVideo				= 1,
		OverlayTextOnVideo				= 2,
		VideoSpeed						= 3,
		PictureInPicture				= 4,
		IntroOutroOverlay				= 5,
		GetMediaInfo					= 6,
		ProbeChannel					= 7,
		MuxAllFiles						= 8,
		GenerateFrameToIngest			= 9,
		GenerateFramesToIngest			= 10,
		Concat							= 11,
		CutWithoutEncoding				= 12,
		CutFrameAccurateWithEncoding	= 13,
		SlideShow						= 14,
		ExtractTrackMediaToIngest		= 15,
		LiveRecorder					= 16,
		LiveProxy						= 17,
		LiveGrid						= 18,
		ChangeFileFormat				= 19,
		StreamingToFile					= 20,
		SilentAudio						= 21,
		IntroOverlay					= 22,
		OutroOverlay					= 23,
		NearestKeyFrameTime				= 24,
		SplitVideoInChunks				= 25
	};
	static const char* toString(const APIName& apiName)
	{
		switch (apiName)
		{
			case APIName::EncodeContent:
				return "EncodeContent";
			case APIName::OverlayImageOnVideo:
				return "OverlayImageOnVideo";
			case APIName::OverlayTextOnVideo:
				return "OverlayTextOnVideo";
			case APIName::VideoSpeed:
				return "VideoSpeed";
			case APIName::PictureInPicture:
				return "PictureInPicture";
			case APIName::IntroOutroOverlay:
				return "IntroOutroOverlay";
			case APIName::GetMediaInfo:
				return "GetMediaInfo";
			case APIName::ProbeChannel:
				return "ProbeChannel";
			case APIName::MuxAllFiles:
				return "MuxAllFiles";
			case APIName::GenerateFrameToIngest:
				return "GenerateFrameToIngest";
			case APIName::GenerateFramesToIngest:
				return "GenerateFramesToIngest";
			case APIName::Concat:
				return "Concat";
			case APIName::CutWithoutEncoding:
				return "CutWithoutEncoding";
			case APIName::CutFrameAccurateWithEncoding:
				return "CutFrameAccurateWithEncoding";
			case APIName::SlideShow:
				return "SlideShow";
			case APIName::ExtractTrackMediaToIngest:
				return "ExtractTrackMediaToIngest";
			case APIName::LiveRecorder:
				return "LiveRecorder";
			case APIName::LiveProxy:
				return "LiveProxy";
			case APIName::LiveGrid:
				return "LiveGrid";
			case APIName::ChangeFileFormat:
				return "ChangeFileFormat";
			case APIName::StreamingToFile:
				return "StreamingToFile";
			case APIName::SilentAudio:
				return "SilentAudio";
			case APIName::IntroOverlay:
				return "IntroOverlay";
			case APIName::OutroOverlay:
				return "OutroOverlay";
			case APIName::NearestKeyFrameTime:
				return "NearestKeyFrameTime";
			case APIName::SplitVideoInChunks:
				return "SplitVideoInChunks";
			default:
				throw runtime_error(string("Wrong APIName"));
		}
	}
	static APIName toAPIName(const string& apiName)
	{
		string lowerCase;
		lowerCase.resize(apiName.size());
		transform(apiName.begin(), apiName.end(), lowerCase.begin(), [](unsigned char c){return tolower(c); } );

		if (lowerCase == "encodecontent")
			return APIName::EncodeContent;
		else if (lowerCase == "overlayimageonvideo")
			return APIName::OverlayImageOnVideo;
		else if (lowerCase == "overlaytextonvideo")
			return APIName::OverlayTextOnVideo;
		else if (lowerCase == "videospeed")
			return APIName::VideoSpeed;
		else if (lowerCase == "pictureinpicture")
			return APIName::PictureInPicture;
		else if (lowerCase == "introoutrooverlay")
			return APIName::IntroOutroOverlay;
		else if (lowerCase == "getmediainfo")
			return APIName::GetMediaInfo;
		else if (lowerCase == "probechannel")
			return APIName::ProbeChannel;
		else if (lowerCase == "muxallfiles")
			return APIName::MuxAllFiles;
		else if (lowerCase == "generateframetoingest")
			return APIName::GenerateFrameToIngest;
		else if (lowerCase == "generateframestoingest")
			return APIName::GenerateFramesToIngest;
		else if (lowerCase == "concat")
			return APIName::Concat;
		else if (lowerCase == "cutwithoutencoding")
			return APIName::CutWithoutEncoding;
		else if (lowerCase == "cutframeaccuratewithencoding")
			return APIName::CutFrameAccurateWithEncoding;
		else if (lowerCase == "slideshow")
			return APIName::SlideShow;
		else if (lowerCase == "extracttrackmediatoingest")
			return APIName::ExtractTrackMediaToIngest;
		else if (lowerCase == "liverecorder")
			return APIName::LiveRecorder;
		else if (lowerCase == "liveproxy")
			return APIName::LiveProxy;
		else if (lowerCase == "livegrid")
			return APIName::LiveGrid;
		else if (lowerCase == "changefileformat")
			return APIName::ChangeFileFormat;
		else if (lowerCase == "streamingtofile")
			return APIName::StreamingToFile;
		else if (lowerCase == "silentaudio")
			return APIName::SilentAudio;
		else if (lowerCase == "introoverlay")
			return APIName::IntroOverlay;
		else if (lowerCase == "outrooverlay")
			return APIName::OutroOverlay;
		else if (lowerCase == "nearestkeyframetime")
			return APIName::NearestKeyFrameTime;
		else if (lowerCase == "splitvideoinchunks")
			return APIName::SplitVideoInChunks;
		else
			throw runtime_error(string("Wrong APIName")
				+ ", current apiName: " + apiName
			);
	}

    shared_ptr<spdlog::logger>  _logger;
    string          _ffmpegPath;
    string          _ffmpegTempDir;
    string			_ffmpegEndlessRecursivePlaylistDir;
    string          _ffmpegTtfFontDir;
    int             _charsToBeReadFromFfmpegErrorOutput;
    bool            _twoPasses;
    string          _outputFfmpegPathFileName;
    bool            _currentlyAtSecondPass;
    
	string			_youTubeDlPath;
	string			_pythonPathName;

	APIName			_currentApiName;

    int64_t         _currentDurationInMilliSeconds;
    string          _currentMMSSourceAssetPathName;
    string          _currentStagingEncodedAssetPathName;
    int64_t         _currentIngestionJobKey;
    int64_t         _currentEncodingJobKey;

	chrono::system_clock::time_point	_startFFMpegMethod;
	int				_startCheckingFrameInfoInMinutes;

    int				_waitingNFSSync_maxMillisecondsToWait;
    int				_waitingNFSSync_milliSecondsWaitingBetweenChecks;

	string			_incrontabConfigurationDirectory;
	string			_incrontabConfigurationFileName;
	string			_incrontabBinary;


	void setStatus(
		int64_t ingestionJobKey,
		int64_t encodingJobKey = -1,
		int64_t durationInMilliSeconds = -1,
		string mmsSourceAssetPathName = "",
		string stagingEncodedAssetPathName = ""
	);

	tuple<string, string, string> addFilters(
		Json::Value filtersRoot,
		string ffmpegVideoResolutionParameter,
		string ffmpegDrawTextFilter,
		int64_t streamingDurationInSeconds);

	string getFilter(
		Json::Value filtersRoot,
		int64_t streamingDurationInSeconds);

	int getNextLiveProxyInput(int64_t ingestionJobKey, int64_t encodingJobKey,
		Json::Value* inputsRoot, mutex* inputsRootMutex,
		int currentInputIndex, bool timedInput, Json::Value* newInputRoot);

	tuple<long, string, string, int, int64_t, Json::Value
		// vector<tuple<int, int64_t, string, string, int, int, string, long>>,
		// vector<tuple<int, int64_t, string, long, int, long, string>>
		>
		liveProxyInput(int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder,
		Json::Value inputRoot, vector<string>& ffmpegInputArgumentList);

	void outputsRootToFfmpeg(int64_t ingestionJobKey, int64_t encodingJobKey,
		bool externalEncoder,
		string otherOutputOptionsBecauseOfMaxWidth,
		Json::Value inputDrawTextDetailsRoot,
		// vector<tuple<int, int64_t, string, string, int, int, string, long>>& inputVideoTracks,
		// vector<tuple<int, int64_t, string, long, int, long, string>>& inputAudioTracks,
		long streamingDurationInSeconds,
		Json::Value outputsRoot,
		vector<string>& ffmpegOutputArgumentList);
	void outputsRootToFfmpeg_clean(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		Json::Value outputsRoot,
		bool externalEncoder);

    string getLastPartOfFile(
        string pathFileName, int lastCharsToBeRead);
    
	long getFrameByOutputLog(string ffmpegEncodingStatus);

	void addToIncrontab(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string directoryToBeMonitored);

	void removeFromIncrontab(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string directoryToBeMonitored);

	int progressDownloadCallback(
		int64_t ingestionJobKey,
		chrono::system_clock::time_point& lastTimeProgressUpdate, 
		double& lastPercentageUpdated,
		double dltotal, double dlnow,
		double ultotal, double ulnow);

	void renameOutputFfmpegPathFileName(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string outputFfmpegPathFileName);
};

#endif

