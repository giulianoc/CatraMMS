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
#include "EncodersLoadBalancer.h"
#include "spdlog/spdlog.h"

struct MaxConcurrentJobsReached: public exception {
    char const* what() const throw() 
    {
        return "Encoder reached the max number of concurrent jobs";
    }; 
};

struct NoEncodingJobKeyFound: public exception {
    char const* what() const throw() 
    {
        return "No Encoding Job Key Found";
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
    
    void init(
        int proxyIdentifier, mutex* mtEncodingJobs,
        Json::Value configuration,
        shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade,
        shared_ptr<MMSStorage> mmsStorage,
        shared_ptr<EncodersLoadBalancer> encodersLoadBalancer,
        #ifdef __LOCALENCODER__
            int* pRunningEncodingsNumber,
        #else
        #endif
        shared_ptr<spdlog::logger> logger);
    
    void setEncodingData(
        EncodingJobStatus* status,
        shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem
    );

    void operator ()();

    int getEncodingProgress(int64_t encodingJobKey);

private:
    shared_ptr<spdlog::logger>          _logger;
    int                                 _proxyIdentifier;
    Json::Value                         _configuration;
    mutex*                              _mtEncodingJobs;
    EncodingJobStatus*                  _status;
    shared_ptr<MMSEngineDBFacade>       _mmsEngineDBFacade;
    shared_ptr<MMSStorage>              _mmsStorage;
    shared_ptr<EncodersLoadBalancer>    _encodersLoadBalancer;
    shared_ptr<MMSEngineDBFacade::EncodingItem> _encodingItem;
    
    string                              _mp4Encoder;
    string                              _mpeg2TSEncoder;
    int                                 _intervalInSecondsToCheckEncodingFinished;
    
    string                              _ffmpegEncoderProtocol;
    int                                 _ffmpegEncoderPort;
    string                              _ffmpegEncoderUser;
    string                              _ffmpegEncoderPassword;
    string                              _ffmpegEncoderProgressURI;
    string                              _ffmpegEncoderStatusURI;
    string                              _ffmpegEncodeURI;
    string                              _ffmpegOverlayImageOnVideoURI;
    
    #ifdef __LOCALENCODER__
        shared_ptr<FFMpeg>              _ffmpeg;
        int*                            _pRunningEncodingsNumber;
        int                             _ffmpegMaxCapacity;
    #else
        string                          _currentUsedFFMpegEncoderHost;
    #endif

    string encodeContentVideoAudio();
    string encodeContent_VideoAudio_through_ffmpeg();

    int64_t processEncodedContentVideoAudio(string stagingEncodedAssetPathName);    

    string overlayImageOnVideo();
    string overlayImageOnVideo_through_ffmpeg();

    pair<int64_t,int64_t> processOverlayedImageOnVideo(string stagingEncodedAssetPathName);    

    bool getEncodingStatus(int64_t encodingJobKey);
};

#endif

