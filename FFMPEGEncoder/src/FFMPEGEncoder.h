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

#pragma once

#include "FFMPEGEncoderTask.h"
#include "FFMpegWrapper.h"
#include "FastCGIAPI.h"
#include <deque>
#include <shared_mutex>

// 2021-08-22: in case the VECTOR is used, we will set the size of the vector to
// a big value and use the _maxXXXXCapacity configuration variable to manage
// dinamically (without stopping the encoder) the capacity
#define VECTOR_MAX_CAPACITY 100

// see comment 2020-11-30
#define __VECTOR__NO_LOCK_FOR_ENCODINGSTATUS

class FFMPEGEncoder final : public FastCGIAPI
{
  public:
	FFMPEGEncoder(
		const json& configurationRoot,
		// string encoderCapabilityConfigurationPathName,

		mutex *fcgiAcceptMutex,

		shared_mutex *cpuUsageMutex, deque<int> *cpuUsage,

		// mutex* lastEncodingAcceptedTimeMutex,
		chrono::system_clock::time_point *lastEncodingAcceptedTime,

		mutex *encodingMutex, vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *encodingsCapability,

		mutex *liveProxyMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability,

		mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,

		mutex *encodingCompletedMutex, map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap,
		chrono::system_clock::time_point *lastEncodingCompletedCheck,

		mutex *tvChannelsPortsMutex, long *tvChannelPort_CurrentOffset
	);

	~FFMPEGEncoder() override;

	void manageRequestAndResponse(
		const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI, const string_view& requestMethod,
		const string_view& requestBody, bool responseBodyCompressed, unsigned long contentLength,
		const unordered_map<string, string> &requestDetails, const unordered_map<string, string>& queryParameters
	) override;

	shared_ptr<AuthorizationDetails> checkAuthorization(const string_view& sThreadId, const string_view& userName, const string_view& password) override;

	bool basicAuthenticationRequired(const string &requestURI, const unordered_map<string, string> &queryParameters) override;

	void sendError(FCGX_Request &request, int htmlResponseCode, const string_view& errorMessage) override;

  private:
	json _configurationRoot;

	shared_mutex *_cpuUsageMutex;
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
	int _intervalInSecondsBetweenEncodingAcceptForInternalEncoder{};
	int _intervalInSecondsBetweenEncodingAcceptForExternalEncoder{};
	chrono::system_clock::time_point *_lastEncodingAcceptedTime;

	string _encoderUser;
	string _encoderPassword;

	int _cpuUsageThresholdForEncoding{};
	int _cpuUsageThresholdForProxy{};
	int _cpuUsageThresholdForRecording{};

	mutex *_encodingMutex;
	vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *_encodingsCapability;
	// commented because retrieved dinamically
	// int							_maxEncodingsCapability;
	int getMaxEncodingsCapability() const;

	mutex *_liveProxyMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *_liveProxiesCapability;
	// commented because retrieved dinamically
	// int							_maxLiveProxiesCapability;
	int getMaxLiveProxiesCapability(int64_t ingestionJobKey) const;

	mutex *_liveRecordingMutex;
	vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *_liveRecordingsCapability;
	// commented because retrieved dinamically
	// int							_maxLiveRecordingsCapability;
	int getMaxLiveRecordingsCapability() const;

	// int calculateCapabilitiesBasedOnOtherRunningProcesses(
	// 	int configuredMaxEncodingsCapability,
	// 	int configuredMaxLiveProxiesCapability,
	// 	int configuredMaxLiveRecordingsCapability
	// );

	mutex *_encodingCompletedMutex;
	int _encodingCompletedRetentionInSeconds{};
	map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *_encodingCompletedMap;
	chrono::system_clock::time_point *_lastEncodingCompletedCheck;

	mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;

	int64_t _mmsAPITimeoutInSeconds{};

	void status(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void info(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void videoSpeed(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void encodeContent(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void cutFrameAccurate(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void overlayImageOnVideo(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void overlayTextOnVideo(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void generateFrames(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void slideShow(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void addSilentAudio(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void pictureInPicture(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void introOutroOverlay(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void liveRecorder(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void liveProxy(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void liveGrid(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void encodingStatus(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void filterNotification(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void killEncodingJob(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void changeLiveProxyPlaylist(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void changeLiveProxyOverlayText(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void encodingProgress(
		const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void liveProxy_liveGrid(
		const string_view& method, const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void requestManagement(
		const string_view& method, const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
		const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
		const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
		const unordered_map<string, string>& requestDetails,
		const unordered_map<string, string>& queryParameters);

	void encodeContentThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void overlayImageOnVideoThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void overlayTextOnVideoThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void generateFramesThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void slideShowThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	) const;

	void liveRecorderThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::LiveRecording> &liveRecording, int64_t ingestionJobKey, int64_t encodingJobKey,
		string requestBody
	) const;

	void liveProxyThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey,
		string requestBody
	) const;

	void liveGridThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey,
		string requestBody
	) const;

	void videoSpeedThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	) const;

	void addSilentAudioThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	) const;

	void pictureInPictureThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void introOutroOverlayThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	);

	void cutFrameAccurateThread(
		// FCGX_Request& request,
		const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const json &metadataRoot
	) const;

	static string buildFilterNotificationIngestionWorkflow(int64_t ingestionJobKey, const string& filterName, json ingestedParametersRoot);

	void encodingCompletedRetention() const;

	// void termProcess(shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding, int64_t encodingJobKey, string label, string message, bool kill);

	void loadConfiguration(const json &configurationRoot);
};
