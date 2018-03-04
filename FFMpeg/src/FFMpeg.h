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
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
#include "json/json.h"

struct FFMpegEncodingStatusNotAvailable: public exception {
    char const* what() const throw() 
    {
        return "Encoding status not available";
    }; 
};

class FFMpeg {
public:
    FFMpeg(Json::Value configuration,
            shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
            shared_ptr<MMSStorage> mmsStorage,
            shared_ptr<spdlog::logger> logger);
    
    ~FFMpeg();

    void encodeContent(
        string mmsSourceAssetPathName,
        int64_t durationInMilliSeconds,
        string encodedFileName,
        string stagingEncodedAssetPathName,
        string encodingProfileDetails,
        MMSEngineDBFacade::ContentType contentType,
        int64_t physicalPathKey,
        string customerDirectoryName,
        string relativePath,
        int64_t encodingJobKey,
        int64_t ingestionJobKey);

    int getEncodingProgress();

    int64_t getVideoOrAudioDurationInMilliSeconds(
        string mmsAssetPathName);

    void generateScreenshotToIngest(
        string imagePathName,
        double timePositionInSeconds,
        int sourceImageWidth,
        int sourceImageHeight,
        string mmsAssetPathName);

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
    shared_ptr<MMSEngineDBFacade>   _mmsEngineDBFacade;
    shared_ptr<MMSStorage>      _mmsStorage;
    string          _ffmpegPath;
    int             _charsToBeReadFromFfmpegErrorOutput;
    bool            _twoPasses;
    string          _outputFfmpegPathFileName;
    bool            _currentlyAtSecondPass;
    
    int64_t         _currentDurationInMilliSeconds;
    string          _currentMMSSourceAssetPathName;
    string          _currentStagingEncodedAssetPathName;
    int64_t         _currentIngestionJobKey;
    int64_t         _currentEncodingJobKey;

    void settingFfmpegPatameters(
        string stagingEncodedAssetPathName,
        
        string encodingProfileDetails,
        MMSEngineDBFacade::ContentType contentType,
        
        bool& segmentFileFormat,
        string& ffmpegFileFormatParameter,

        string& ffmpegVideoCodecParameter,
        string& ffmpegVideoProfileParameter,
        string& ffmpegVideoResolutionParameter,
        string& ffmpegVideoBitRateParameter,
        bool& twoPasses,
        string& ffmpegVideoMaxRateParameter,
        string& ffmpegVideoBufSizeParameter,
        string& ffmpegVideoFrameRateParameter,
        string& ffmpegVideoKeyFramesRateParameter,

        string& ffmpegAudioCodecParameter,
        string& ffmpegAudioBitRateParameter
    );
    
    string getLastPartOfFile(
        string pathFileName, int lastCharsToBeRead);
    
};

#endif

