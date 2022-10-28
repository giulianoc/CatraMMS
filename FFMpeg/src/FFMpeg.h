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
#include <chrono>
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
        // string encodedFileName,
        string stagingEncodedAssetPathName,
        Json::Value encodingProfileDetailsRoot,
        bool isVideo,   // if false it means is audio
		Json::Value videoTracksRoot,
		Json::Value audioTracksRoot,
		int videoTrackIndexToBeUsed, int audioTrackIndexToBeUsed,
        int64_t physicalPathKey,
        string customerDirectoryName,
        string relativePath,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);
    
    void overlayImageOnVideo(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,
        string mmsSourceImageAssetPathName,
        string imagePosition_X_InPixel,
        string imagePosition_Y_InPixel,
        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

    void overlayTextOnVideo(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,

        string text,
        string textPosition_X_InPixel,
        string textPosition_Y_InPixel,
        string fontType,
        int fontSize,
        string fontColor,
        int textPercentageOpacity,
        int shadowX,
        int shadowY,
        bool boxEnable,
        string boxColor,
        int boxPercentageOpacity,

        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey,
		pid_t* pChildPid);

	void videoSpeed(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,

        string videoSpeedType,
        int videoSpeedSize,

        // string encodedFileName,
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

    int getEncodingProgress();

	bool nonMonotonousDTSInOutputLog();
	bool forbiddenErrorInOutputLog();
	bool isFrameIncreasing(int maxMilliSecondsToWait);

	pair<int64_t, long> getMediaInfo(int64_t ingestionJobKey,
		bool isMMSAssetPathName, string mediaSource,
		vector<tuple<int, int64_t, string, string, int, int, string, long>>& videoTracks,
		vector<tuple<int, int64_t, string, long, int, long, string>>& audioTracks);

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

    vector<string> generateFramesToIngest(
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

    void generateSlideshowMediaToIngest(
        int64_t ingestionJobKey,
        int64_t encodingJobKey,
        vector<string>& imagesSourcePhysicalPaths,
        double durationOfEachSlideInSeconds, 
        vector<string>& audiosSourcePhysicalPaths,
		double shortestAudioDurationInSeconds,
		string videoSyncMethod,
        int outputFrameRate,
        string slideshowMediaPathName,
		pid_t* pChildPid);

	void cutWithoutEncoding(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
		string cutType,
		bool isVideo,
        double startTimeInSeconds,
        double endTimeInSeconds,
        int framesNumber,
        string cutMediaPathName);

	void cutFrameAccurateWithEncoding(
		int64_t ingestionJobKey,
		string sourceVideoAssetPathName,
		// no keyFrameSeeking needs reencoding otherwise the key frame is always used
		// If you re-encode your video when you cut/trim, then you get a frame-accurate cut
		// because FFmpeg will re-encode the video and start with an I-frame.
		int64_t encodingJobKey,
		Json::Value encodingProfileDetailsRoot,
		double startTimeInSeconds,
		double endTimeInSeconds,
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

		pid_t* pChildPid);

/*
	void liveProxy(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		int maxWidth,

		// if actAsServer (true) means the liveURL should be like rtmp://<local IP to bind>:<port>
		//		listening for an incoming connection
		// if actAsServer (false) means the liveURL is "any thing" referring a stream
		string streamSourceType,
		string liveURL,
		// Used only in case actAsServer is true, Maximum time to wait for the incoming connection
		int listenTimeoutInSeconds,

		// parameters used only in case streamSourceType is CaptureLive
		int captureLive_videoDeviceNumber,
		string captureLive_videoInputFormat,
		int captureLive_frameRate,
		int captureLive_width,
		int captureLive_height,
		int captureLive_audioDeviceNumber,
		int captureLive_channelsNumber,

		string userAgent,
		string otherInputOptions,

		bool timePeriod,
		time_t utcProxyPeriodStart,
		time_t utcProxyPeriodEnd,

		// array, each element is an output containing the following fields
		//  string outputType (it could be: HLS, DASH, RTMP_Stream)
		//  #in case of HLS or DASH
		//      Json::Value encodingProfileDetailsRoot,
		//      int segmentDurationInSeconds,
		//      int playlistEntriesNumber,
		//      string manifestDirectoryPath,
		//      string manifestFileName,
		//  #in case of RTMP_Stream
		//      Json::Value encodingProfileDetailsRoot,
		//      string rtmpUrl,
		//
		vector<tuple<string, string, string, Json::Value, string, string, int, int, bool, string, string>>& outputRoots,

		pid_t* pChildPid);
*/

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

	/*
	void vodProxy(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,

		string vodContentType,
		string sourcePhysicalPathName,

		string otherInputOptions,

		bool timePeriod,
		time_t utcProxyPeriodStart,
		time_t utcProxyPeriodEnd,

		// array, each element is an output containing the following fields
		//  string outputType (it could be: HLS, DASH, RTMP_Stream)
		//  #in case of HLS or DASH
		//		string otherOutputOptions
		//		string audioVolumeChange
		//      Json::Value encodingProfileDetailsRoot,
		//      string encodingProfileContentType
		//      int segmentDurationInSeconds,
		//      int playlistEntriesNumber,
		//      string manifestDirectoryPath,
		//      string manifestFileName,
		//  #in case of RTMP_Stream
		//		string otherOutputOptions
		//		string audioVolumeChange
		//      Json::Value encodingProfileDetailsRoot,
		//      string encodingProfileContentType
		//      string rtmpUrl,
		//
		vector<tuple<string, string, string, Json::Value, string, string, int, int, bool, string, string>>& outputRoots,

		pid_t* pChildPid);
		*/

/*
	void awaitingTheBegining(
        int64_t encodingJobKey,
        int64_t ingestionJobKey,

        string mmsSourceVideoAssetPathName,
		int64_t videoDurationInMilliSeconds,

		time_t utcCountDownEnd,

		string text,
		string textPosition_X_InPixel,
		string textPosition_Y_InPixel,
		string fontType,
		int fontSize,
		string fontColor,
		int textPercentageOpacity,
		bool boxEnable,
		string boxColor,
		int boxPercentageOpacity,

		string outputType,
		Json::Value encodingProfileDetailsRoot,
		string manifestDirectoryPath,
		string manifestFileName,
		int segmentDurationInSeconds,
		int playlistEntriesNumber,
		bool isVideo,
		string rtmpUrl,
		string udpUrl,

		pid_t* pChildPid);
*/

	void liveGrid(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		Json::Value encodingProfileDetailsRoot,
		string userAgent,
		Json::Value inputChannelsRoot,	// name,url
		int gridColumns,
		int gridWidth,  // i.e.: 1024
		int gridHeight, // i.e.: 578

		string outputType,

		// next are parameters for the hls output
		int segmentDurationInSeconds,
		int playlistEntriesNumber,
		string manifestDirectoryPath,
		string manifestFileName,

		// next are parameters for the hls output
		string srtURL,

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

	static void encodingFileFormatValidation(string fileFormat,
		shared_ptr<spdlog::logger> logger);

    static void encodingAudioCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger);

    static void encodingVideoProfileValidation(
        string codec, string profile,
        shared_ptr<spdlog::logger> logger);

    static void encodingVideoCodecValidation(string codec,
        shared_ptr<spdlog::logger> logger);

	pair<string, string> retrieveStreamingYouTubeURL(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string youTubeURL);

private:
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

	string			_currentApiName;

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


	string getDrawTextVideoFilterDescription(
		string text,
		string textFilePathName,
		string textPosition_X_InPixel,
		string textPosition_Y_InPixel,
		string fontType,
		int fontSize,
		string fontColor,
		int textPercentageOpacity,
		int shadowX,
		int shadowY,
		bool boxEnable,
		string boxColor,
		int boxPercentageOpacity,
		int64_t streamingDurationInSeconds
	);

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

	tuple<long, string, string, int, int64_t> liveProxyInput(
		int64_t ingestionJobKey, int64_t encodingJobKey, bool externalEncoder,
		Json::Value inputRoot, vector<string>& ffmpegInputArgumentList);

	void liveProxyOutput(int64_t ingestionJobKey, int64_t encodingJobKey,
		bool externalEncoder,
		string otherOutputOptionsBecauseOfMaxWidth,
		Json::Value inputRoot,
		long streamingDurationInSeconds,
		Json::Value outputsRoot,
		vector<string>& ffmpegOutputArgumentList);

    void settingFfmpegParameters(
        Json::Value encodingProfileDetailsRoot,
        bool isVideo,   // if false it means is audio
        
		string& httpStreamingFileFormat,
		string& ffmpegHttpStreamingParameter,

        string& ffmpegFileFormatParameter,

        string& ffmpegVideoCodecParameter,
        string& ffmpegVideoProfileParameter,
        string& ffmpegVideoOtherParameters,
        bool& twoPasses,
        string& ffmpegVideoFrameRateParameter,
        string& ffmpegVideoKeyFramesRateParameter,
		vector<tuple<string, int, int, int, string, string, string>>& videoBitRatesInfo,

        string& ffmpegAudioCodecParameter,
        string& ffmpegAudioOtherParameters,
        string& ffmpegAudioChannelsParameter,
        string& ffmpegAudioSampleRateParameter,
		vector<string>& audioBitRatesInfo
    );
	void addToArguments(string parameter, vector<string>& argumentList);
    
    string getLastPartOfFile(
        string pathFileName, int lastCharsToBeRead);
    
    bool isMetadataPresent(Json::Value root, string field);

	int asInt(Json::Value root, string field = "", int defaultValue = 0);

	int64_t asInt64(Json::Value root, string field = "", int64_t defaultValue = 0);

	double asDouble(Json::Value root, string field = "", double defaultValue = 0.0);

	bool asBool(Json::Value root, string field, bool defaultValue);

    void removeHavingPrefixFileName(string directoryName, string prefixFileName);

	long getFrameByOutputLog(string ffmpegEncodingStatus);

	void addToIncrontab(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string directoryToBeMonitored);

	void removeFromIncrontab(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string directoryToBeMonitored);
};

#endif

