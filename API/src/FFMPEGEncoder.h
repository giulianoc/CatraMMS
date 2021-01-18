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

#ifndef FFMPEGEncoder_h
#define FFMPEGEncoder_h

#include "APICommon.h"
#include "FFMpeg.h"

#define __VECTOR__

// see comment 2020-11-30
#define __VECTOR__NO_LOCK_FOR_ENCODINGSTATUS

struct Encoding
{
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>		_ffmpeg;
		pid_t					_childPid;

		string					_errorMessage;
};

struct LiveProxyAndGrid
{
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>		_ffmpeg;
		pid_t					_childPid;
        bool					_killedBecauseOfNotWorking;	// by monitorThread

		string					_errorMessage;

		string					_liveGridOutputType;	// only for LiveGrid
		// Json::Value				_liveProxyOutputsRoot;	// only for LiveProxy
		vector<tuple<string, string, Json::Value, string, string, int, int, bool, string>> _liveProxyOutputRoots;

		int64_t					_ingestionJobKey;
		Json::Value				_ingestedParametersRoot;
        string					_channelType;
		string					_channelLabel;
		vector<string>			_manifestFilePathNames;
		chrono::system_clock::time_point	_proxyStart;
};

// no encoding, just copying the video/audio tracks
struct LiveRecording
{
        bool                    _running;
        int64_t                 _encodingJobKey;
        shared_ptr<FFMpeg>      _ffmpeg;
		pid_t					_childPid;
        bool					_killedBecauseOfNotWorking;	// by monitorThread

		string					_errorMessage;

		int64_t					_ingestionJobKey;
		Json::Value				_encodingParametersRoot;
		Json::Value				_liveRecorderParametersRoot;
        string					_channelType;
        string					_transcoderStagingContentsPath;
        string					_stagingContentsPath;
        string					_segmentListFileName;
        string					_recordedFileNamePrefix;
		string					_lastRecordedAssetFileName;
		int						_lastRecordedAssetDurationInSeconds;
		string					_channelLabel;
		chrono::system_clock::time_point	_recordingStart;
};

struct EncodingCompleted
{
		int64_t					_encodingJobKey;
		bool					_completedWithError;
		string					_errorMessage;
		bool					_killedByUser;
		bool					_urlForbidden;
		bool					_urlNotFound;
		chrono::system_clock::time_point	_timestamp;
};

class FFMPEGEncoder: public APICommon {
public:
    FFMPEGEncoder(Json::Value configuration, 
		mutex* fcgiAcceptMutex,

		mutex* encodingMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<Encoding>>* encodingsCapability,
		#else	// __MAP__
			map<int64_t, shared_ptr<Encoding>>* encodingsCapability,
		#endif

		mutex* liveProxyMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<LiveProxyAndGrid>>* liveProxiesCapability,
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveProxyAndGrid>>* liveProxiesCapability,
		#endif

		mutex* liveRecordingMutex,
		#ifdef __VECTOR__
			vector<shared_ptr<LiveRecording>>* liveRecordingsCapability, 
		#else	// __MAP__
			map<int64_t, shared_ptr<LiveRecording>>* liveRecordingsCapability,
		#endif

		mutex* encodingCompletedMutex,
		map<int64_t, shared_ptr<EncodingCompleted>>* encodingCompletedMap,
		chrono::system_clock::time_point* lastEncodingCompletedCheck,

		mutex* satelliteChannelsUdpPortsMutex,
		long* satelliteChannelPort_CurrentOffset,

		shared_ptr<spdlog::logger> logger);
    
    ~FFMPEGEncoder();
    
    /*
    virtual void getBinaryAndResponse(
        string requestURI,
        string requestMethod,
        string xCatraMMSResumeHeader,
        unordered_map<string, string> queryParameters,
        tuple<int64_t,shared_ptr<Workspace>,bool,bool,bool>& userKeyWorkspaceAndFlags,
        unsigned long contentLength);
    */

    virtual void manageRequestAndResponse(
            FCGX_Request& request,
            string requestURI,
            string requestMethod,
            unordered_map<string, string> queryParameters,
            bool basicAuthenticationPresent,
            tuple<int64_t,shared_ptr<Workspace>, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool, bool>& userKeyWorkspaceAndFlags,
			string apiKey,
            unsigned long contentLength,
            string requestBody,
            unordered_map<string, string>& requestDetails
    );

	void liveRecorderChunksIngestionThread();
	void stopLiveRecorderChunksIngestionThread();

	void monitorThread();
	void stopMonitorThread();
    
private:
    mutex*						_encodingMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<Encoding>>* _encodingsCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<Encoding>>* _encodingsCapability;
	#endif
	int							_maxEncodingsCapability;

    mutex*						_liveProxyMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<LiveProxyAndGrid>>* _liveProxiesCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<LiveProxyAndGrid>>* _liveProxiesCapability;
	#endif
	int							_maxLiveProxiesCapability;

    mutex*						_liveRecordingMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<LiveRecording>>* _liveRecordingsCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<LiveRecording>>* _liveRecordingsCapability;
	#endif
	int							_maxLiveRecordingsCapability;
	int							_liveRecorderChunksIngestionCheckInSeconds;
	bool						_liveRecorderChunksIngestionThreadShutdown;

    mutex*						_encodingCompletedMutex;
	int							_encodingCompletedRetentionInSeconds;
    map<int64_t, shared_ptr<EncodingCompleted>>*	_encodingCompletedMap;
	chrono::system_clock::time_point*				_lastEncodingCompletedCheck;

	mutex*						_satelliteChannelsUdpPortsMutex;
	long*						_satelliteChannelPort_CurrentOffset;
	long						_satelliteChannelPort_Start;
	long						_satelliteChannelPort_MaxNumberOfOffsets;

	int							_monitorCheckInSeconds;
	bool						_monitorThreadShutdown;

    string								_mmsAPIProtocol;
    string								_mmsAPIHostname;
    int									_mmsAPIPort;
    // string								_mmsAPIUser;
    // string								_mmsAPIPassword;
    string								_mmsAPIIngestionURI;
    int									_mmsAPITimeoutInSeconds;

    void encodeContentThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);
    
    void overlayImageOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);

    void overlayTextOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void generateFramesThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void slideShowThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void liveRecorderThread(
        // FCGX_Request& request,
        shared_ptr<LiveRecording> liveRecording,
        int64_t encodingJobKey,
        string requestBody);
	pair<string, int> liveRecorder_processLastGeneratedLiveRecorderFiles(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string channelType, 
		bool highAvailability, bool main, int segmentDurationInSeconds, string outputFileFormat,
		Json::Value encodingParametersRoot,
		Json::Value liveRecorderParametersRoot,
		string transcoderStagingContentsPath,
		string stagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName,
		int lastRecordedAssetDurationInSeconds);
	time_t liveRecorder_getMediaLiveRecorderStartTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName, int segmentDurationInSeconds, bool isFirstChunk);
	time_t liveRecorder_getMediaLiveRecorderEndTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName);
	bool liveRecorder_isLastLiveRecorderFile(int64_t ingestionJobKey, int64_t encodingJobKey,
			time_t currentRecordedFileCreationTime, string transcoderStagingContentsPath,
			string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk);
	void liveRecorder_ingestRecordedMedia(
		int64_t ingestionJobKey,
		string transcoderStagingContentsPath, string currentRecordedAssetFileName,
		string stagingContentsPath,
		string addContentTitle,
		string uniqueName,
		bool highAvailability,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value liveRecorderParametersRoot);

	void liveProxyThread(
        // FCGX_Request& request,
        shared_ptr<LiveProxyAndGrid> liveProxy,
        int64_t encodingJobKey,
        string requestBody);

	void liveGridThread(
        // FCGX_Request& request,
        shared_ptr<LiveProxyAndGrid> liveProxy,
        int64_t encodingJobKey,
        string requestBody);

	void videoSpeedThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void pictureInPictureThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError, string errorMessage,
		bool killedByUser, bool urlForbidden, bool urlNotFound);

	void removeEncodingCompletedIfPresent(int64_t encodingJobKey);

	void encodingCompletedRetention();
};

#endif

