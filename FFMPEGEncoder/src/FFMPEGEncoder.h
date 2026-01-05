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
#include "BandwidthUsageThread.h"
#include <deque>
#include <shared_mutex>

// 2021-08-22: in case the VECTOR is used, we will set the size of the std::vector to
// a big value and use the _maxXXXXCapacity configuration variable to manage
// dinamically (without stopping the encoder) the capacity
#define VECTOR_MAX_CAPACITY 100

// see comment 2020-11-30
#define __VECTOR__NO_LOCK_FOR_ENCODINGSTATUS

class FFMPEGEncoder final : public FastCGIAPI
{
  public:
	FFMPEGEncoder(
		const nlohmann::json& configurationRoot,
		// std::string encoderCapabilityConfigurationPathName,

		std::mutex *fcgiAcceptMutex,

		std::shared_mutex *cpuUsageMutex, std::deque<int> *cpuUsage,

		// std::chrono::system_clock::time_point *lastEncodingAcceptedTime,

		std::mutex *encodingMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::Encoding>> *encodingsCapability,

		std::mutex *liveProxyMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability,

		std::mutex *liveRecordingMutex, std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,

		std::mutex *encodingCompletedMutex, std::map<int64_t, std::shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap,
		std::chrono::system_clock::time_point *lastEncodingCompletedCheck,

		std::mutex *tvChannelsPortsMutex, long *tvChannelPort_CurrentOffset,
		const std::shared_ptr<BandwidthUsageThread>& bandwidthUsageThread
	);

	~FFMPEGEncoder() override;

	void manageRequestAndResponse(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	) override;

	std::shared_ptr<FCGIRequestData::AuthorizationDetails> checkAuthorization(const std::string_view& sThreadId,
		const FCGIRequestData& requestData, const std::string_view& userName, const std::string_view& password) override;

	bool basicAuthenticationRequired(const FCGIRequestData& requestData) override;

	void sendError(FCGX_Request &request, int htmlResponseCode, const std::string_view& errorMessage) override;

  private:
	nlohmann::json _configurationRoot;

	std::shared_mutex *_cpuUsageMutex;
	std::deque<int> *_cpuUsage;
	// bool						_cpuUsageThreadShutdown;

	// 2021-09-24: std::chrono is already thread safe.
	// std::mutex*						_lastEncodingAcceptedTimeMutex;
	// lastEncodingAccepted: scenario, this process receives 10 encoding
	// requests concurrently and,
	//	since the cpu usage is OK at this time, all the requestes are accepted
	// overloading the process 	To solve this issue, we will force to wait at
	// lease 5 seconds to accept a second encoding request. 	That will allow
	// the cpuUsage to be updated for the next encoding request
	// int _intervalInSecondsBetweenEncodingAcceptForInternalEncoder{};
	// int _intervalInSecondsBetweenEncodingAcceptForExternalEncoder{};
	// std::chrono::system_clock::time_point *_lastEncodingAcceptedTime;
	int _intervalInSecondsBetweenEncodingAccept{};

	std::string _encoderUser;
	std::string _encoderPassword;

	int _cpuUsageThresholdForEncoding{};
	int _cpuUsageThresholdForProxy{};
	int _cpuUsageThresholdForRecording{};

	std::shared_ptr<BandwidthUsageThread> _bandwidthUsageThread;

	std::mutex *_encodingMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::Encoding>> *_encodingsCapability;
	// commented because retrieved dinamically
	// int							_maxEncodingsCapability;
	int getMaxEncodingsCapability() const;

	std::mutex *_liveProxyMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *_liveProxiesCapability;
	// commented because retrieved dinamically
	// int							_maxLiveProxiesCapability;
	int getMaxLiveProxiesCapability(int64_t ingestionJobKey) const;

	std::mutex *_liveRecordingMutex;
	std::vector<std::shared_ptr<FFMPEGEncoderBase::LiveRecording>> *_liveRecordingsCapability;
	// commented because retrieved dinamically
	// int							_maxLiveRecordingsCapability;
	int getMaxLiveRecordingsCapability() const;

	// int calculateCapabilitiesBasedOnOtherRunningProcesses(
	// 	int configuredMaxEncodingsCapability,
	// 	int configuredMaxLiveProxiesCapability,
	// 	int configuredMaxLiveRecordingsCapability
	// );

	std::mutex *_encodingCompletedMutex;
	int _encodingCompletedRetentionInSeconds{};
	std::map<int64_t, std::shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *_encodingCompletedMap;
	std::chrono::system_clock::time_point *_lastEncodingCompletedCheck;

	std::mutex *_tvChannelsPortsMutex;
	long *_tvChannelPort_CurrentOffset;

	int64_t _mmsAPITimeoutInSeconds{};

	void status(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void info(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void avgBandwidthUsage(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	);

	void videoSpeed(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void encodeContent(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void cutFrameAccurate(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void overlayImageOnVideo(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void overlayTextOnVideo(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void generateFrames(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void slideShow(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void addSilentAudio(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void pictureInPicture(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void introOutroOverlay(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void liveRecorder(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void liveProxy(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void liveGrid(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void encodingStatus(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void filterNotification(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void killEncodingJob(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void changeLiveProxyPlaylist(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void changeLiveProxyOverlayText(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void encodingProgress(
		const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData
	) const;

	void liveProxy_liveGrid(
		const std::string_view& method, const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void requestManagement(
		const std::string_view& method, const std::string_view& sThreadId, FCGX_Request &request,
		const FCGIRequestData& requestData);

	void encodeContentThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void overlayImageOnVideoThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void overlayTextOnVideoThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void generateFramesThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void slideShowThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void liveRecorderThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::LiveRecording> &liveRecording, int64_t ingestionJobKey, int64_t encodingJobKey,
		std::string requestBody
	) const;

	void liveProxyThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey,
		std::string requestBody
	) const;

	void liveGridThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey,
		std::string requestBody
	) const;

	void videoSpeedThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void addSilentAudioThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void pictureInPictureThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void introOutroOverlayThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	void cutFrameAccurateThread(
		// FCGX_Request& request,
		const std::shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, int64_t ingestionJobKey, int64_t encodingJobKey, const nlohmann::json &metadataRoot
	) const;

	static std::string buildFilterNotificationIngestionWorkflow(int64_t ingestionJobKey, const std::string& filterName, nlohmann::json ingestedParametersRoot);

	void encodingCompletedRetention() const;

	// void termProcess(std::shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding, int64_t encodingJobKey, std::string label, std::string message, bool kill);

	void loadConfiguration(const nlohmann::json &configurationRoot);
};