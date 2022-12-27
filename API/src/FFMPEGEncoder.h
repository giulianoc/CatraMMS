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
#include <deque>
#include "FFMPEGEncoderTask.h"

// 2021-08-22: in case the VECTOR is used, we will set the size of the vector to a big value
// and use the _maxXXXXCapacity configuration variable to manage
// dinamically (without stopping the encoder) the capacity
#define VECTOR_MAX_CAPACITY 100

// see comment 2020-11-30
#define __VECTOR__NO_LOCK_FOR_ENCODINGSTATUS


class FFMPEGEncoder: public APICommon {
public:
	struct CurlUploadData {
		ifstream	mediaSourceFileStream;

		int64_t		lastByteSent;
		int64_t		fileSizeInBytes;        
	};

    FFMPEGEncoder(
		Json::Value configuration, 
		// string encoderCapabilityConfigurationPathName,

		mutex* fcgiAcceptMutex,

		mutex* cpuUsageMutex,
		deque<int> *cpuUsage,

		// mutex* lastEncodingAcceptedTimeMutex,
		chrono::system_clock::time_point* lastEncodingAcceptedTime,

		mutex* encodingMutex,
		vector<shared_ptr<FFMPEGEncoderTask::Encoding>>* encodingsCapability,

		mutex* liveProxyMutex,
		vector<shared_ptr<FFMPEGEncoderTask::LiveProxyAndGrid>>* liveProxiesCapability,

		mutex* liveRecordingMutex,
		vector<shared_ptr<FFMPEGEncoderTask::LiveRecording>>* liveRecordingsCapability, 

		mutex* encodingCompletedMutex,
		map<int64_t, shared_ptr<FFMPEGEncoderTask::EncodingCompleted>>* encodingCompletedMap,
		chrono::system_clock::time_point* lastEncodingCompletedCheck,

		mutex* tvChannelsPortsMutex,
		long* tvChannelPort_CurrentOffset,

		shared_ptr<spdlog::logger> logger);
    
    ~FFMPEGEncoder();
    
    virtual void manageRequestAndResponse(
			string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed,
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
	deque<int>*						_cpuUsage;
	bool						_cpuUsageThreadShutdown;

	// 2021-09-24: chrono is already thread safe.
	// mutex*						_lastEncodingAcceptedTimeMutex;
	// lastEncodingAccepted: scenario, this process receives 10 encoding requests concurrently and,
	//	since the cpu usage is OK at this time, all the requestes are accepted overloading the process 
	//	To solve this issue, we will force to wait at lease 5 seconds to accept a second encoding request.
	//	That will allow the cpuUsage to be updated for the next encoding request
	int							_intervalInSecondsBetweenEncodingAccept;
	chrono::system_clock::time_point*	_lastEncodingAcceptedTime;

	int							_cpuUsageThresholdForEncoding;
	int							_cpuUsageThresholdForProxy;
	int							_cpuUsageThresholdForRecording;

    mutex*						_encodingMutex;
	vector<shared_ptr<FFMPEGEncoderTask::Encoding>>* _encodingsCapability;
	// commented because retrieved dinamically
	// int							_maxEncodingsCapability;
	int getMaxEncodingsCapability(void);

    mutex*						_liveProxyMutex;
	vector<shared_ptr<FFMPEGEncoderTask::LiveProxyAndGrid>>* _liveProxiesCapability;
	// commented because retrieved dinamically
	// int							_maxLiveProxiesCapability;
	int getMaxLiveProxiesCapability(void);

    mutex*						_liveRecordingMutex;
	vector<shared_ptr<FFMPEGEncoderTask::LiveRecording>>* _liveRecordingsCapability;
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

	string						_tvChannelConfigurationDirectory;

    mutex*						_encodingCompletedMutex;
	int							_encodingCompletedRetentionInSeconds;
    map<int64_t, shared_ptr<FFMPEGEncoderTask::EncodingCompleted>>*	_encodingCompletedMap;
	chrono::system_clock::time_point*				_lastEncodingCompletedCheck;

	mutex*						_tvChannelsPortsMutex;
	long*						_tvChannelPort_CurrentOffset;
	long						_tvChannelPort_Start;
	long						_tvChannelPort_MaxNumberOfOffsets;

	int							_monitorCheckInSeconds;
	bool						_monitorThreadShutdown;

    int									_mmsAPITimeoutInSeconds;

	int							_mmsBinaryTimeoutInSeconds;

    void encodeContentThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);
    
    void overlayImageOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
            int64_t encodingJobKey,
        string requestBody);

    void overlayTextOnVideoThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void generateFramesThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

    void slideShowThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void liveRecorderThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::LiveRecording> liveRecording,
        int64_t encodingJobKey,
        string requestBody);
	pair<string, double> liveRecorder_processStreamSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string streamSourceType, 
		bool externalEncoder,
		int segmentDurationInSeconds, string outputFileFormat,
		Json::Value encodingParametersRoot,
		Json::Value ingestedParametersRoot,
		string chunksTranscoderStagingContentsPath,
		string chunksNFSStagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds);
	pair<string, double> liveRecorder_processHLSSegmenterOutput(
		int64_t ingestionJobKey, int64_t encodingJobKey,
		string streamSourceType, 
		bool externalEncoder,
		int segmentDurationInSeconds, string outputFileFormat,
		Json::Value encodingParametersRoot,
		Json::Value ingestedParametersRoot,
		string chunksTranscoderStagingContentsPath,
		string sharedStagingContentsPath,
		string segmentListFileName,
		string recordedFileNamePrefix,
		string lastRecordedAssetFileName,
		double lastRecordedAssetDurationInSeconds);
	time_t liveRecorder_getMediaLiveRecorderStartTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName, int segmentDurationInSeconds, bool isFirstChunk);
	time_t liveRecorder_getMediaLiveRecorderEndTime(int64_t ingestionJobKey, int64_t encodingJobKey,
			string mediaLiveRecorderFileName);
	bool liveRecorder_isLastLiveRecorderFile(int64_t ingestionJobKey, int64_t encodingJobKey,
			time_t currentRecordedFileCreationTime, string chunksTranscoderStagingContentsPath,
			string recordedFileNamePrefix, int segmentDurationInSeconds, bool isFirstChunk);
	void liveRecorder_ingestRecordedMediaInCaseOfInternalTranscoder(
		int64_t ingestionJobKey,
		string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
		string sharedStagingContentsPath,
		string addContentTitle,
		string uniqueName,
		// bool highAvailability,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value ingestedParametersRoot,
		Json::Value encodingParametersRoot,
		bool copy);
	string liveRecorder_buildChunkIngestionWorkflow(
		int64_t ingestionJobKey,
		bool externalEncoder,
		string currentRecordedAssetFileName,
		string chunksNFSStagingContentsPath,
		string addContentTitle,
		string uniqueName,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value ingestedParametersRoot,
		Json::Value encodingParametersRoot
	);

	void liveRecorder_ingestRecordedMediaInCaseOfExternalTranscoder(
		int64_t ingestionJobKey,
		string chunksTranscoderStagingContentsPath, string currentRecordedAssetFileName,
		string addContentTitle,
		string uniqueName,
		Json::Value userDataRoot,
		string fileFormat,
		Json::Value ingestedParametersRoot,
		Json::Value encodingParametersRoot);
	long liveRecorder_buildAndIngestVirtualVOD(
		int64_t liveRecorderIngestionJobKey,
		int64_t liveRecorderEncodingJobKey,
		bool externalEncoder,

		string sourceSegmentsDirectoryPathName,
		string sourceManifestFileName,
		string stagingLiveRecorderVirtualVODPathName,

		int64_t deliveryCode,
		string liveRecorderIngestionJobLabel,
		string liveRecorderVirtualVODUniqueName,
		string liveRecorderVirtualVODRetention,
		int64_t liveRecorderVirtualVODImageMediaItemKey,
		int64_t liveRecorderUserKey,
		string liveRecorderApiKey,
		string mmsWorkflowIngestionURL,
		string mmsBinaryIngestionURL);
	string liveRecorder_buildVirtualVODIngestionWorkflow(
		int64_t liveRecorderIngestionJobKey,
		int64_t liveRecorderEncodingJobKey,
		bool externalEncoder,

		int64_t utcStartTimeInMilliSecs,
		int64_t utcEndTimeInMilliSecs,
		int64_t deliveryCode,
		string liveRecorderIngestionJobLabel,
		string tarGzStagingLiveRecorderVirtualVODPathName,
		string liveRecorderVirtualVODUniqueName,
		string liveRecorderVirtualVODRetention,
		int64_t liveRecorderVirtualVODImageMediaItemKey);
	long getAddContentIngestionJobKey(
		int64_t ingestionJobKey,
		string ingestionResponse);

	void liveProxyThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::LiveProxyAndGrid> liveProxy,
        int64_t encodingJobKey,
        string requestBody);

	void liveGridThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::LiveProxyAndGrid> liveProxy,
        int64_t encodingJobKey,
        string requestBody);

	void videoSpeedThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void pictureInPictureThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void introOutroOverlayThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void cutFrameAccurateThread(
        // FCGX_Request& request,
        shared_ptr<FFMPEGEncoderTask::Encoding> encoding,
        int64_t encodingJobKey,
        string requestBody);

	void addEncodingCompleted(
        int64_t encodingJobKey, bool completedWithError, string errorMessage,
		bool killedByUser, bool urlForbidden, bool urlNotFound);

	void removeEncodingCompletedIfPresent(int64_t encodingJobKey);

	void encodingCompletedRetention();

	void createOrUpdateTVDvbLastConfigurationFile(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string multicastIP,
		string multicastPort,
		string tvType,
		int64_t tvServiceId,
		int64_t tvFrequency,
		int64_t tvSymbolRate,
		int64_t tvBandwidthInMhz,
		string tvModulation,
		int tvVideoPid,
		int tvAudioItalianPid,
		bool toBeAdded
	);

	pair<string, string> getTVMulticastFromDvblastConfigurationFile(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		string tvType,
		int64_t tvServiceId,
		int64_t tvFrequency,
		int64_t tvSymbolRate,
		int64_t tvBandwidthInMhz,
		string tvModulation
	);

	string buildAddContentIngestionWorkflow(
		int64_t ingestionJobKey,
		string label,
		string fileFormat,
		string ingester,

		// in case of a new content
		string sourceURL,	// if empty it means binary is ingested later (PUSH)
		string title,
		Json::Value userDataRoot,
		Json::Value ingestedParametersRoot,	// it could be also nullValue

		// in case of a Variant
		int64_t variantOfMediaItemKey = -1,
		int64_t variantEncodingProfileKey = -1);

	int64_t ingestContentByPushingBinary(
		int64_t ingestionJobKey,
		string workflowMetadata,
		string fileFormat,
		string binaryPathFileName,
		int64_t binaryFileSizeInBytes,
		int64_t userKey,
		string apiKey,
		string mmsWorkflowIngestionURL,
		string mmsBinaryIngestionURL);

	string downloadMediaFromMMS(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		shared_ptr<FFMpeg> ffmpeg,
		string sourceFileExtension,
		string sourcePhysicalDeliveryURL,
		string destAssetPathName);

	void uploadLocalMediaToMMS(
		int64_t ingestionJobKey,
		int64_t encodingJobKey,
		Json::Value ingestedParametersRoot,
		Json::Value encodingProfileDetailsRoot,
		Json::Value encodingParametersRoot,
		string sourceFileExtension,
		string encodedStagingAssetPathName,
		string workflowLabel,
		string ingester,
		// in case of a Variant
		int64_t variantOfMediaItemKey = -1,
		int64_t variantEncodingProfileKey = -1);
};

#endif

