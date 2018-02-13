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

#ifndef EncoderVideoAudioProxy_h
#define EncoderVideoAudioProxy_h

#include "CMSEngineDBFacade.h"
#include "CMSStorage.h"
#include "spdlog/spdlog.h"

struct MaxConcurrentJobsReached: public exception {
    char const* what() const throw() 
    {
        return "Encoder reached the max number of concurrent jobs";
    }; 
};

struct EncoderError: public exception {
    char const* what() const throw() 
    {
        return "Encoder error";
    }; 
};

struct EncodingStatusNotAvailable: public exception {
    char const* what() const throw() 
    {
        return "Encoding status not available";
    }; 
};

enum class EncodingJobStatus
{
    Free,
    ToBeRun,
    Running
};

class EncoderVideoAudioProxy {
public:
    EncoderVideoAudioProxy();

    virtual ~EncoderVideoAudioProxy();
    
    void setData(
        mutex* mtEncodingJobs,
        EncodingJobStatus* status,
        shared_ptr<CMSEngineDBFacade> cmsEngineDBFacade,
        shared_ptr<CMSStorage> cmsStorage,
        shared_ptr<CMSEngineDBFacade::EncodingItem> encodingItem,
        #ifdef __FFMPEGLOCALENCODER__
            int* pffmpegEncoderRunning,
        #else
        #endif
        shared_ptr<spdlog::logger> logger);

    void operator ()();

    int getEncodingProgress();

    static void encodingFileFormatValidation(string fileFormat);
    static void ffmpeg_encodingVideoCodecValidation(string codec);
    static void ffmpeg_encodingVideoProfileValidation(string codec, string profile);
    static void ffmpeg_encodingAudioCodecValidation(string codec);

    static int64_t getVideoOrAudioDurationInMilliSeconds(string cmsAssetPathName);
    static void generateScreenshotToIngest(
        string imagePathName,
        double timePositionInSeconds,
        int sourceImageWidth,
        int sourceImageHeight,
        string cmsAssetPathName);

    string                              _outputFfmpegPathFileName;

private:
    shared_ptr<spdlog::logger>          _logger;
    mutex*                              _mtEncodingJobs;
    EncodingJobStatus*                  _status;
    shared_ptr<CMSEngineDBFacade>       _cmsEngineDBFacade;
    shared_ptr<CMSStorage>              _cmsStorage;
    shared_ptr<CMSEngineDBFacade::EncodingItem> _encodingItem;
    
    bool                                _twoPasses;
    bool                                _currentlyAtSecondPass;
    
    // static string                              _ffmpegPathName;
    string                              _MP4Encoder;
    string                              _mpeg2TSEncoder;
    
    // string                              _outputFfmpegPathFileName;

    #ifdef __FFMPEGLOCALENCODER__
        int*                            _pffmpegEncoderRunning;
        int                             _ffmpegMaxCapacity;
    #endif

    string encodeContentVideoAudio();
    string encodeContent_VideoAudio_through_ffmpeg();

    void processEncodedContentVideoAudio(string stagingEncodedAssetPathName);
    
    void settingFfmpegPatameters(
        string stagingEncodedAssetPathName,
    
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

    static string getLastPartOfFile(string pathFileName, int lastCharsToBeRead);
};

#endif

