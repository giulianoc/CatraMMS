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

#ifdef __LOCALENCODER__
#include "FFMpeg.h"
#endif
#include "MMSEngineDBFacade.h"
#include "MMSStorage.h"
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

enum class EncodingJobStatus
{
    Free,
    ToBeRun,
    Running
};

struct EncodingStatusNotAvailable: public exception {
    char const* what() const throw() 
    {
        return "Encoding status not available";
    }; 
};

class EncoderVideoAudioProxy {
public:
    EncoderVideoAudioProxy();

    virtual ~EncoderVideoAudioProxy();
    
    void setData(
        Json::Value configuration,
        mutex* mtEncodingJobs,
        EncodingJobStatus* status,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem,
        #ifdef __LOCALENCODER__
            int* pRunningEncodingsNumber,
        #else
        #endif
        shared_ptr<spdlog::logger> logger);

    void operator ()();

    int getEncodingProgress(int64_t encodingJobKey);

private:
    shared_ptr<spdlog::logger>          _logger;
    Json::Value                         _configuration;
    mutex*                              _mtEncodingJobs;
    EncodingJobStatus*                  _status;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
    
    string                              _mp4Encoder;
    string                              _mpeg2TSEncoder;
    
    #ifdef __LOCALENCODER__
        shared_ptr<FFMpeg>              _ffmpeg;
        int*                            _pRunningEncodingsNumber;
        int                             _ffmpegMaxCapacity;
    #else
        string                          _currentUsedFFMpegEncoderHost;
    #endif

    string encodeContentVideoAudio();
    string encodeContent_VideoAudio_through_ffmpeg();

    void processEncodedContentVideoAudio(string stagingEncodedAssetPathName);    
};

#endif

