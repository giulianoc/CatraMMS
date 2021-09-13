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
// #define __MAP__
// 2021-08-22: in case the VECTOR is used, we will set the size of the vector to a big value
// and use the _maxXXXXCapacity configuration variable to manage
// dinamically (without stopping the encoder) the capacity
#define VECTOR_MAX_CAPACITY 100

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
		string					_method;	// liveProxy, liveGrid or awaitingTheBeginning
        shared_ptr<FFMpeg>		_ffmpeg;
		pid_t					_childPid;
        bool					_killedBecauseOfNotWorking;	// by monitorThread

		string					_errorMessage;

		string					_liveGridOutputType;	// only for LiveGrid
		// Json::Value				_liveProxyOutputsRoot;	// only for LiveProxy
		vector<tuple<string, string, string, Json::Value, string, string, int, int, bool, string>> _liveProxyOutputRoots;

		int64_t					_ingestionJobKey;
		Json::Value				_ingestedParametersRoot;
        string					_channelType;
		string					_channelLabel;
		// vector<string>			_manifestFilePathNames;
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

		vector<tuple<string, string, string, Json::Value, string, string, int, int, bool, string>> _liveRecorderOutputRoots;
		// vector<string>			_manifestFilePathNames;
		int64_t					_ingestionJobKey;
		Json::Value				_encodingParametersRoot;
		Json::Value				_liveRecorderParametersRoot;
        string					_channelType;
        string					_transcoderStagingContentsPath;
        string					_stagingContentsPath;
        string					_segmentListFileName;
        string					_recordedFileNamePrefix;
		string					_lastRecordedAssetFileName;
		double					_lastRecordedAssetDurationInSeconds;
		string					_channelLabel;
		string					_segmenterType;
		chrono::system_clock::time_point	_recordingStart;

		bool					_virtualVOD;
		string					_monitorVirtualVODManifestDirectoryPath;	// used to build virtualVOD
		string					_monitorVirtualVODManifestFileName;			// used to build virtualVOD
		string					_virtualVODStagingContentsPath;
		int64_t					_liveRecorderVirtualVODImageMediaItemKey;
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
    FFMPEGEncoder(
		Json::Value configuration, 
		// string encoderCapabilityConfigurationPathName,

		mutex* fcgiAcceptMutex,

		mutex* cpuUsageMutex,
		int *cpuUsage,
		chrono::system_clock::time_point* lastEncodingAcceptedTime,

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

		mutex* satelliteChannelsPortsMutex,
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

	void liveRecorderVirtualVODIngestionThread();
	void stopLiveRecorderVirtualVODIngestionThread();

	void monitorThread();
	void stopMonitorThread();
    
	void cpuUsageThread();
	void stopCPUUsageThread();

private:
	// string						_encoderCapabilityConfigurationPathName;

	GetCpuUsage_t				_getCpuUsage;
	mutex*						_cpuUsageMutex;
	int*						_cpuUsage;
	// lastEncodingAccepted: scenario, this process receives 10 encoding requests concurrently and,
	//	since the cpu usage is OK at this time, all the requestes are accepted overloading the process 
	//	To solve this issue, we will force to wait at lease 5 seconds to accept a second encoding request.
	//	That will allow the cpuUsage to be updated for the next encoding request
	int							_intervalInSecondsBetweenEncodingAccept;
	chrono::system_clock::time_point*	_lastEncodingAcceptedTime;
	bool						_cpuUsageThreadShutdown;

	int							_cpuUsageThresholdForEncoding;
	int							_cpuUsageThresholdForProxy;
	int							_cpuUsageThresholdForRecording;

    mutex*						_encodingMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<Encoding>>* _encodingsCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<Encoding>>* _encodingsCapability;
	#endif
	// commented because retrieved dinamically
	// int							_maxEncodingsCapability;
	int getMaxEncodingsCapability(void);

    mutex*						_liveProxyMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<LiveProxyAndGrid>>* _liveProxiesCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<LiveProxyAndGrid>>* _liveProxiesCapability;
	#endif
	// commented because retrieved dinamically
	// int							_maxLiveProxiesCapability;
	int getMaxLiveProxiesCapability(void);

    mutex*						_liveRecordingMutex;
	#ifdef __VECTOR__
		vector<shared_ptr<LiveRecording>>* _liveRecordingsCapability;
	#else	// __MAP__
		map<int64_t, shared_ptr<LiveRecording>>* _liveRecordingsCapability;
	#endif
	// commented because retrieved dinamically
	// int							_maxLiveRecordingsCapability;
	int getMaxLiveRecordingsCapability(void);

	// int calculateCapabilitiesBasedOnOtherRunningProcesses(
	// 	int configuredMaxEncodingsCapability,
	// 	int configuredMaxLiveProxiesCapability,
	// 	int configuredMaxLiveRecordingsCapability
	// );

	int							_liveRecorderChunksIngestionCheckInSeconds;
	bool						_liveRecorderChunksIngestionThreadShutdown;

	int							_liveRecorderVirtualVODIngestionInSeconds;
	string						_liveRecorderVirtualVODRetention;
	bool						_liveRecorderVirtualVODIngestionThreadShutdown;
	string						_liveRecorderVirtualVODImageLabel;

	string						_satelliteChannelConfigurationDirectory;

    mutex*						_encodingCompletedMutex;
	int							_encodingCompletedRetentionInSeconds;
    map<int64_t, shared_ptr<EncodingCompleted>>*	_encodingCompletedMap;
	chrono::system_clock::time_point*				_lastEncodingCompletedCheck;

	mutex*						_satelliteChannelsPortsMutex;
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
	pair<string, double> liveRecorder_processStreamSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string channelType, 
		// bool highAvailability, bool main,
		int segmentDurationInSeconds, string outputFileFormat,
		Json::Value encodingParametersRoot,
		Json::Value liveRecorderParametersRoot,
		string transcoderStagingContentsPath,
		string stagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds);
	pair<string, double> liveRecorder_processHLSSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string channelType, 
		// bool highAvailability, bool main,
		int segmentDurationInSeconds, string outputFileFormat,
		Json::Value encodingParametersRoot,
		Json::Value liveRecorderParametersRoot,
		string transcoderStagingContentsPath,
		string stagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds);
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
		// bool highAvailability,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value liveRecorderParametersRoot,
		Json::Value encodingParametersRoot,
		bool copy);
	long liveRecorder_buildAndIngestVirtualVOD(
		int64_t liveRecorderIngestionJobKey,
		int64_t liveRecorderEncodingJobKey,

		string sourceSegmentsDirectoryPathName,
		string sourceManifestFileName,
		string stagingLiveRecorderVirtualVODPathName,

		int64_t deliveryCode,
		string liveRecorderIngestionJobLabel,
		string liveRecorderVirtualVODUniqueName,
		string liveRecorderVirtualVODRetention,
		int64_t liveRecorderVirtualVODImageMediaItemKey,
		int64_t liveRecorderUserKey,
		string liveRecorderApiKey);

	void liveProxyThread(
        // FCGX_Request& request,
        shared_ptr<LiveProxyAndGrid> liveProxy,
        int64_t encodingJobKey,
        string requestBody);

	void awaitingTheBeginningThread(
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

	void introOutroOverlayThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void cutFrameAccurateThread(
        // FCGX_Request& request,
        shared_ptr<Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError, string errorMessage,
		bool killedByUser, bool urlForbidden, bool urlNotFound);

	void removeEncodingCompletedIfPresent(int64_t encodingJobKey);

	void encodingCompletedRetention();

	void createOrUpdateSatelliteDvbLastConfigurationFile(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string multicastIP,
		string multicastPort,
		int64_t satelliteServiceId,
		int64_t satelliteFrequency,
		int64_t satelliteSymbolRate,
		string satelliteModulation,
		int satelliteVideoPid,
		int satelliteAudioItalianPid,
		bool toBeAdded
	);

	pair<string, string> getSatelliteMulticastFromDvblastConfigurationFile(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		int64_t satelliteServiceId,
		int64_t satelliteFrequency,
		int64_t satelliteSymbolRate,
		string satelliteModulation
	);
};

#endif

