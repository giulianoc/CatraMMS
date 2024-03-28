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

#include "FFMPEGEncoderTask.h"
#include "FFMpeg.h"
#include "FastCGIAPI.h"
#include <deque>

// 2021-08-22: in case the VECTOR is used, we will set the size of the vector to
// a big value and use the _maxXXXXCapacity configuration variable to manage
// dinamically (without stopping the encoder) the capacity
#define VECTOR_MAX_CAPACITY 100

// see comment 2020-11-30
#define __VECTOR__NO_LOCK_FOR_ENCODINGSTATUS

class FFMPEGEncoder : public FastCGIAPI
{
  public:
	FFMPEGEncoder(
		json configurationRoot,
		// string encoderCapabilityConfigurationPathName,

		mutex *fcgiAcceptMutex,

		mutex *cpuUsageMutex, deque<int> *cpuUsage,

		// mutex* lastEncodingAcceptedTimeMutex,
		chrono::system_clock::time_point *lastEncodingAcceptedTime,

		mutex *encodingMutex,
		vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *encodingsCapability,

		mutex *liveProxyMutex,
		vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>
			*liveProxiesCapability,

		mutex *liveRecordingMutex,
		vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>
			*liveRecordingsCapability,

		mutex *encodingCompletedMutex,
		map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>
			*encodingCompletedMap,
		chrono::system_clock::time_point *lastEncodingCompletedCheck,

		mutex *tvChannelsPortsMutex, long *tvChannelPort_CurrentOffset,

		shared_ptr<spdlog::logger> logger
	);

	~FFMPEGEncoder();

	virtual void manageRequestAndResponse(
		string sThreadId, int64_t requestIdentifier,
		bool responseBodyCompressed, FCGX_Request &request, string requestURI,
		string requestMethod, unordered_map<string, string> queryParameters,
		bool authorizationPresent, string userName, string password,
		unsigned long contentLength, string requestBody,
		unordered_map<string, string> &requestDetails
	);

	virtual void
	checkAuthorization(string sThreadId, string userName, string password);

	virtual bool basicAuthenticationRequired(
		string requestURI, unordered_map<string, string> queryParameters
	);

  private:
	shared_ptr<spdlog::logger> _logger;

	mutex *_cpuUsageMutex;
	deque<int> *_cpuUsage;
	// bool						_cpuUsageThreadShutdown;

	// 2021-09-24: chrono is already thread safe.
	// mutex*						_lastEncodingAcceptedTimeMutex;
	// lastEncodingAccepted: scenario, this process receives 10 encoding
	// requests concurrently and,
	//	since the cpu usage is OK at this time, all the requestes are accepted
	// overloading the process 	To solve this issue, we will force to wait at
	// lease 5 seconds to accept a second encoding request. 	That will allow
	// the cpuUsage to be updated for the next encoding request
	int _intervalInSecondsBetweenEncodingAcceptForInternalEncoder;
	int _intervalInSecondsBetweenEncodingAcceptForExternalEncoder;
	chrono::system_clock::time_point *_lastEncodingAcceptedTime;

	string _encoderUser;
	string _encoderPassword;

	int _cpuUsageThresholdForEncoding;
	int _cpuUsageThresholdForProxy;
	int _cpuUsageThresholdForRecording;

	mutex *_encodingMutex;
	vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *_encodingsCapability;
	// commented because retrieved dinamically
	// int							_maxEncodingsCapability;
	int getMaxEncodingsCapability(void);

	mutex *_liveProxyMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>>
		*_liveProxiesCapability;
	// commented because retrieved dinamically
	// int							_maxLiveProxiesCapability;
	int getMaxLiveProxiesCapability(void);

	mutex *_liveRecordingMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>>
		*_liveRecordingsCapability;
	// commented because retrieved dinamically
	// int							_maxLiveRecordingsCapability;
	int getMaxLiveRecordingsCapability(void);

	// int calculateCapabilitiesBasedOnOtherRunningProcesses(
	// 	int configuredMaxEncodingsCapability,
	// 	int configuredMaxLiveProxiesCapability,
	// 	int configuredMaxLiveRecordingsCapability
	// );

	mutex *_encodingCompletedMutex;
	int _encodingCompletedRetentionInSeconds;
	map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>
		*_encodingCompletedMap;
	chrono::system_clock::time_point *_lastEncodingCompletedCheck;

	mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;

	int64_t _mmsAPITimeoutInSeconds;

	void encodeContentThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void overlayImageOnVideoThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void overlayTextOnVideoThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void generateFramesThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void slideShowThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void liveRecorderThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording,
		int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
	);

	void liveProxyThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy,
		int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
	);

	void liveGridThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy,
		int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
	);

	void videoSpeedThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void addSilentAudioThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void pictureInPictureThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void introOutroOverlayThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	void cutFrameAccurateThread(
		// FCGX_Request& request,
		shared_ptr<FFMPEGEncoderBase::Encoding> encoding,
		int64_t ingestionJobKey, int64_t encodingJobKey,
		json metadataRoot
	);

	string buildFilterNotificationIngestionWorkflow(
		int64_t ingestionJobKey, string filterName,
		json ingestedParametersRoot
	);

	void encodingCompletedRetention();
};

#endif
