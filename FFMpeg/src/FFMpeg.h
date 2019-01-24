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
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        int64_t physicalPathKey,
        string customerDirectoryName,
        string relativePath,
        int64_t encodingJobKey,
        int64_t ingestionJobKey);
    
    void overlayImageOnVideo(
        string mmsSourceVideoAssetPathName,
        int64_t videoDurationInMilliSeconds,
        string mmsSourceImageAssetPathName,
        string imagePosition_X_InPixel,
        string imagePosition_Y_InPixel,
        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey);

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
        bool boxEnable,
        string boxColor,
        int boxPercentageOpacity,

        // string encodedFileName,
        string stagingEncodedAssetPathName,
        int64_t encodingJobKey,
        int64_t ingestionJobKey);

    int getEncodingProgress();

    tuple<int64_t,long,string,string,int,int,string,long,string,long,int,long> getMediaInfo(string mmsAssetPathName);

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
        int64_t videoDurationInMilliSeconds);

    void generateConcatMediaToIngest(
        int64_t ingestionJobKey,
        vector<string>& sourcePhysicalPaths,
        string concatenatedMediaPathName);

    void generateSlideshowMediaToIngest(
        int64_t ingestionJobKey,
        vector<string>& sourcePhysicalPaths,
        double durationOfEachSlideInSeconds, 
        int outputFrameRate,
        string slideshowMediaPathName);

    void generateCutMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        double startTimeInSeconds,
        double endTimeInSeconds,
        int framesNumber,
        string cutMediaPathName);

    void extractTrackMediaToIngest(
        int64_t ingestionJobKey,
        string sourcePhysicalPath,
        vector<pair<string,int>>& tracksToBeExtracted,
        string extractTrackMediaPathName);

	void liveRecorder(
        int64_t ingestionJobKey,
		string segmentListPathName,
        string liveURL,
        time_t utcRecordingPeriodStart, 
        time_t utcRecordingPeriodEnd, 
        int segmentDurationInSeconds,
        string outputFileFormat);

	void changeFileFormat(
		int64_t ingestionJobKey,
		int64_t sourceKey,
		string sourcePhysicalPath,
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

private:
    shared_ptr<spdlog::logger>  _logger;
    string          _ffmpegPath;
    string          _ffmpegTempDir;
    string          _ffmpegTtfFontDir;
    int             _charsToBeReadFromFfmpegErrorOutput;
    bool            _twoPasses;
    string          _outputFfmpegPathFileName;
    bool            _currentlyAtSecondPass;
    
    int64_t         _currentDurationInMilliSeconds;
    string          _currentMMSSourceAssetPathName;
    string          _currentStagingEncodedAssetPathName;
    int64_t         _currentIngestionJobKey;
    int64_t         _currentEncodingJobKey;

    void settingFfmpegParameters(
        string stagingEncodedAssetPathName,
        
        string encodingProfileDetails,
        bool isVideo,   // if false it means is audio
        
        bool& segmentFileFormat,
        string& ffmpegFileFormatParameter,

        string& ffmpegVideoCodecParameter,
        string& ffmpegVideoProfileParameter,
        string& ffmpegVideoResolutionParameter,
        string& ffmpegVideoBitRateParameter,
        string& ffmpegVideoOtherParameters,
        bool& twoPasses,
        string& ffmpegVideoMaxRateParameter,
        string& ffmpegVideoBufSizeParameter,
        string& ffmpegVideoFrameRateParameter,
        string& ffmpegVideoKeyFramesRateParameter,

        string& ffmpegAudioCodecParameter,
        string& ffmpegAudioBitRateParameter,
        string& ffmpegAudioOtherParameters,
        string& ffmpegAudioChannelsParameter,
        string& ffmpegAudioSampleRateParameter
    );
    
    string getLastPartOfFile(
        string pathFileName, int lastCharsToBeRead);
    
    bool isMetadataPresent(Json::Value root, string field);

    void removeHavingPrefixFileName(string directoryName, string prefixFileName);
};

#endif

