/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   FFMPEGEncoder.cpp
 * Author: giuliano
 *
 * Created on February 18, 2018, 1:27 AM
 */

#include "FFMPEGEncoder.h"
#include "AddSilentAudio.h"
#include "CutFrameAccurate.h"
#include "EncodeContent.h"
#include "FFMPEGEncoderDaemons.h"
#include "GenerateFrames.h"
#include "IntroOutroOverlay.h"
#include "JSONUtils.h"
#include "LiveGrid.h"
#include "LiveProxy.h"
#include "LiveRecorder.h"
#include "LiveRecorderDaemons.h"
#include "MMSCURL.h"
#include "MMSStorage.h"
#include "OverlayImageOnVideo.h"
#include "OverlayTextOnVideo.h"
#include "PictureInPicture.h"
#include "SlideShow.h"
#include "VideoSpeed.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/GetCpuUsage.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include "catralibraries/System.h"
#include "spdlog/fmt/bundled/core.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <fstream>
#include <sstream>

// extern char** environ;

FFMPEGEncoder::FFMPEGEncoder(

	json configurationRoot,
	// string encoderCapabilityConfigurationPathName,

	mutex *fcgiAcceptMutex,

	mutex *cpuUsageMutex, deque<int> *cpuUsage,

	// mutex* lastEncodingAcceptedTimeMutex,
	chrono::system_clock::time_point *lastEncodingAcceptedTime,

	mutex *encodingMutex, vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *encodingsCapability,

	mutex *liveProxyMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability,

	mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,

	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap,
	chrono::system_clock::time_point *lastEncodingCompletedCheck,

	mutex *tvChannelsPortsMutex, long *tvChannelPort_CurrentOffset,

	shared_ptr<spdlog::logger> logger
)
	: FastCGIAPI(configurationRoot, fcgiAcceptMutex)
{

	_logger = spdlog::default_logger();
	_encodingCompletedRetentionInSeconds = JSONUtils::asInt(_configurationRoot["ffmpeg"], "encodingCompletedRetentionInSeconds", 0);
	_logger->info(
		__FILEREF__ + "Configuration item" + ", ffmpeg->encodingCompletedRetentionInSeconds: " + to_string(_encodingCompletedRetentionInSeconds)
	);

	_mmsAPITimeoutInSeconds = JSONUtils::asInt(_configurationRoot["api"], "timeoutInSeconds", 120);
	_logger->info(__FILEREF__ + "Configuration item" + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds));

	_cpuUsageThresholdForEncoding = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForEncoding", 50);
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->cpuUsageThresholdForEncoding: " + to_string(_cpuUsageThresholdForEncoding));
	_cpuUsageThresholdForRecording = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForRecording", 60);
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->cpuUsageThresholdForRecording: " + to_string(_cpuUsageThresholdForRecording));
	_cpuUsageThresholdForProxy = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForProxy", 70);
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->cpuUsageThresholdForProxy: " + to_string(_cpuUsageThresholdForProxy));
	_intervalInSecondsBetweenEncodingAcceptForInternalEncoder =
		JSONUtils::asInt(_configurationRoot["ffmpeg"], "intervalInSecondsBetweenEncodingAcceptForInternalEncoder", 5);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", "
		"ffmpeg->intervalInSecondsBetweenEncodingAcceptForInternalEncoder:"
		" " +
		to_string(_intervalInSecondsBetweenEncodingAcceptForInternalEncoder)
	);
	_intervalInSecondsBetweenEncodingAcceptForExternalEncoder =
		JSONUtils::asInt(_configurationRoot["ffmpeg"], "intervalInSecondsBetweenEncodingAcceptForExternalEncoder", 120);
	_logger->info(
		__FILEREF__ + "Configuration item" +
		", "
		"ffmpeg->intervalInSecondsBetweenEncodingAcceptForExternalEncoder:"
		" " +
		to_string(_intervalInSecondsBetweenEncodingAcceptForExternalEncoder)
	);

	_encoderUser = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderUser", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderUser: " + _encoderUser);
	_encoderPassword = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderPassword", "");
	_logger->info(__FILEREF__ + "Configuration item" + ", ffmpeg->encoderPassword: " + _encoderPassword);

	_cpuUsageMutex = cpuUsageMutex;
	_cpuUsage = cpuUsage;

	// _lastEncodingAcceptedTimeMutex = lastEncodingAcceptedTimeMutex;
	_lastEncodingAcceptedTime = lastEncodingAcceptedTime;

	_encodingMutex = encodingMutex;
	_encodingsCapability = encodingsCapability;

	_liveProxyMutex = liveProxyMutex;
	_liveProxiesCapability = liveProxiesCapability;

	_liveRecordingMutex = liveRecordingMutex;
	_liveRecordingsCapability = liveRecordingsCapability;

	_encodingCompletedMutex = encodingCompletedMutex;
	_encodingCompletedMap = encodingCompletedMap;
	_lastEncodingCompletedCheck = lastEncodingCompletedCheck;

	_tvChannelsPortsMutex = tvChannelsPortsMutex;
	_tvChannelPort_CurrentOffset = tvChannelPort_CurrentOffset;

	*_lastEncodingCompletedCheck = chrono::system_clock::now();
}

FFMPEGEncoder::~FFMPEGEncoder() {}

// 2020-06-11: FFMPEGEncoder is just one thread, so make sure
// manageRequestAndResponse is very fast because
//	the time used by manageRequestAndResponse is time FFMPEGEncoder is not
// listening 	for new connections (encodingStatus, ...)
// 2023-10-17: non penso che il commento sopra sia vero, FFMPEGEncoder non è un
// solo thread!!!

void FFMPEGEncoder::manageRequestAndResponse(
	string sThreadId, int64_t requestIdentifier, bool responseBodyCompressed, FCGX_Request &request, string requestURI, string requestMethod,
	unordered_map<string, string> queryParameters, bool authorizationPresent, string userName, string password, unsigned long contentLength,
	string requestBody, unordered_map<string, string> &requestDetails
)
{
	// chrono::system_clock::time_point startManageRequestAndResponse =
	// chrono::system_clock::now();

	auto methodIt = queryParameters.find("method");
	if (methodIt == queryParameters.end())
	{
		_logger->error(__FILEREF__ + "The 'method' parameter is not found");

		string errorMessage = string("Internal server error");

		sendError(request, 500, errorMessage);

		throw runtime_error(errorMessage);
	}
	string method = methodIt->second;

	if (method == "status")
	{
		try
		{
			json responseBodyRoot;
			responseBodyRoot["status"] = "Encoder up and running";

			string responseBody = JSONUtils::toString(responseBodyRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "status failed" + ", requestBody: " + requestBody + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else if (method == "info")
	{
		try
		{
			int lastBiggerCpuUsage = -1;
			{
				lock_guard<mutex> locker(*_cpuUsageMutex);

				for (int cpuUsage : *_cpuUsage)
				{
					if (cpuUsage > lastBiggerCpuUsage)
						lastBiggerCpuUsage = cpuUsage;
				}
			}

			json infoRoot;
			infoRoot["status"] = "Encoder up and running";
			infoRoot["cpuUsage"] = lastBiggerCpuUsage;

			string responseBody = JSONUtils::toString(infoRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (exception &e)
		{
			_logger->error(__FILEREF__ + "status failed" + ", requestBody: " + requestBody + ", e.what(): " + e.what());

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else if (method == "videoSpeed" || method == "encodeContent" || method == "cutFrameAccurate" || method == "overlayImageOnVideo" ||
			 method == "overlayTextOnVideo" || method == "generateFrames" || method == "slideShow" || method == "addSilentAudio" ||
			 method == "pictureInPicture" || method == "introOutroOverlay")
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		{
			lock_guard<mutex> locker(*_encodingMutex);

			shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;
			bool freeEncodingFound = false;
			bool encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::Encoding>
			// encoding:
			// *_encodingsCapability)
			int maxEncodingsCapability = getMaxEncodingsCapability();
			for (int encodingIndex = 0; encodingIndex < maxEncodingsCapability; encodingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::Encoding> encoding = (*_encodingsCapability)[encodingIndex];

				if (encoding->_available)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedEncoding = encoding;
					}
				}
				else
				{
					if (encoding->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + EncodingIsAlreadyRunning().what();
				else if (maxEncodingsCapability == 0)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + MaxConcurrentJobsReached().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + NoEncodingAvailable().what();

				_logger->error(
					__FILEREF__ + errorMessage + ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning) +
					", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw
				// runtime_error(noEncodingAvailableMessage);
				return;
			}

			try
			{
				json metadataRoot = JSONUtils::toJson(requestBody);

				bool externalEncoder = JSONUtils::asBool(metadataRoot, "externalEncoder", false);

				// se si tratta di un encoder esterno,
				// aspettiamo piu tempo in modo che la variabile
				// cpuUsage si aggiorni prima di accettare una
				// nuova richiesta
				int intervalInSecondsBetweenEncodingAccept = externalEncoder ? _intervalInSecondsBetweenEncodingAcceptForExternalEncoder
																			 : _intervalInSecondsBetweenEncodingAcceptForInternalEncoder;

				chrono::system_clock::time_point now = chrono::system_clock::now();
				int elapsedSecondsSinceLastEncodingAccepted = chrono::duration_cast<chrono::seconds>(now - *_lastEncodingAcceptedTime).count();
				if (elapsedSecondsSinceLastEncodingAccepted < intervalInSecondsBetweenEncodingAccept)
				{
					int secondsToWait = intervalInSecondsBetweenEncodingAccept - elapsedSecondsSinceLastEncodingAccepted;
					string errorMessage = string("Too early to accept a new "
												 "encoding request") +
										  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) +
										  ", "
										  "elapsedSecondsSinceLastEncodingAcc"
										  "epted: " +
										  to_string(elapsedSecondsSinceLastEncodingAccepted) +
										  ", "
										  "intervalInSecondsBetweenEncodingAc"
										  "cept: " +
										  to_string(intervalInSecondsBetweenEncodingAccept) + ", secondsToWait: " + to_string(secondsToWait) + ", " +
										  NoEncodingAvailable().what();

					_logger->warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw
					// runtime_error(noEncodingAvailableMessage);
					return;
				}
				else
				{
					_logger->info(
						__FILEREF__ + "Accept a new encoding request" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey) +
						", "
						"elapsedSecondsSinceLastEncodingAcc"
						"epted: " +
						to_string(elapsedSecondsSinceLastEncodingAccepted) +
						", "
						"intervalInSecondsBetweenEncodingAc"
						"cept: " +
						to_string(intervalInSecondsBetweenEncodingAccept)
					);
				}

				// 2023-06-15: scenario: tanti encoding stanno
				// aspettando di essere gestiti.
				//	L'encoder finisce il task in corso,
				// tutti gli encoding
				// in attesa 	verificano che la CPU è bassa e,
				// tutti entrano per essere gestiti. Per
				// risolvere questo problema, è necessario
				// aggiornare _lastEncodingAcceptedTime as soon
				// as possible, altrimenti tutti quelli in coda
				// entrano per essere gestiti
				*_lastEncodingAcceptedTime = chrono::system_clock::now();

				selectedEncoding->_available = false;
				selectedEncoding->_childPid = 0; // not running
				selectedEncoding->_killToRestartByEngine = false;
				selectedEncoding->_encodingJobKey = encodingJobKey;
				selectedEncoding->_ffmpegTerminatedSuccessful = false;

				_logger->info(
					__FILEREF__ + "Creating " + method + " thread" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody
				);
				if (method == "videoSpeed")
				{
					thread videoSpeedThread(&FFMPEGEncoder::videoSpeedThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot);
					videoSpeedThread.detach();
				}
				else if (method == "encodeContent")
				{
					thread encodeContentThread(
						&FFMPEGEncoder::encodeContentThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					encodeContentThread.detach();
				}
				else if (method == "cutFrameAccurate")
				{
					thread cutFrameAccurateThread(
						&FFMPEGEncoder::cutFrameAccurateThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					cutFrameAccurateThread.detach();
				}
				else if (method == "overlayImageOnVideo")
				{
					thread overlayImageOnVideoThread(
						&FFMPEGEncoder::overlayImageOnVideoThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					overlayImageOnVideoThread.detach();
				}
				else if (method == "overlayTextOnVideo")
				{
					thread overlayTextOnVideoThread(
						&FFMPEGEncoder::overlayTextOnVideoThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					overlayTextOnVideoThread.detach();
				}
				else if (method == "addSilentAudio")
				{
					thread addSilentAudioThread(
						&FFMPEGEncoder::addSilentAudioThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					addSilentAudioThread.detach();
				}
				else if (method == "slideShow")
				{
					thread slideShowThread(&FFMPEGEncoder::slideShowThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot);
					slideShowThread.detach();
				}
				else if (method == "generateFrames")
				{
					thread generateFramesThread(
						&FFMPEGEncoder::generateFramesThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					generateFramesThread.detach();
				}
				else if (method == "pictureInPicture")
				{
					thread pictureInPictureThread(
						&FFMPEGEncoder::pictureInPictureThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					pictureInPictureThread.detach();
				}
				else if (method == "introOutroOverlay")
				{
					thread introOutroOverlayThread(
						&FFMPEGEncoder::introOutroOverlayThread, this, selectedEncoding, ingestionJobKey, encodingJobKey, metadataRoot
					);
					introOutroOverlayThread.detach();
				}
				else
				{
					selectedEncoding->_available = true;
					selectedEncoding->_childPid = 0; // not running

					string errorMessage = string("wrong method") + ", method: " + method;

					_logger->error(
						__FILEREF__ + errorMessage + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", encodingJobKey: " + to_string(encodingJobKey)
					);

					sendError(request, 500, errorMessage);

					// throw
					// runtime_error(noEncodingAvailableMessage);
					return;
				}
			}
			catch (exception &e)
			{
				selectedEncoding->_available = true;
				selectedEncoding->_childPid = 0; // not running

				_logger->error(
					__FILEREF__ + method + " failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		try
		{
			json responseBodyRoot;
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "sendSuccess failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
			);

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else if (method == "liveRecorder")
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			shared_ptr<FFMPEGEncoderBase::LiveRecording> selectedLiveRecording;
			bool freeEncodingFound = false;
			bool encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::LiveRecording>
			// liveRecording:
			// *_liveRecordingsCapability)
			int maxLiveRecordingsCapability = getMaxLiveRecordingsCapability();
			for (int liveRecordingIndex = 0; liveRecordingIndex < maxLiveRecordingsCapability; liveRecordingIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording = (*_liveRecordingsCapability)[liveRecordingIndex];

				if (liveRecording->_available)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedLiveRecording = liveRecording;
					}
				}
				else
				{
					if (liveRecording->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + NoEncodingAvailable().what();

				_logger->error(
					__FILEREF__ + errorMessage + ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning) +
					", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw
				// runtime_error(noEncodingAvailableMessage);
				return;
			}

			try
			{
				/*
				 * 2021-09-15: live-recorder cannot wait.
				Scenario: received a lot of requests that fail
				 * Those requests set _lastEncodingAcceptedTime
				and delay a lot
				 * the requests that would work fine
				 * Consider that Live-Recorder is a Task where
				FFMPEGEncoder
				 * could receive a lot of close requests
				lock_guard<mutex>
				locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the
				previous encoding request
				// in order to give time to the cpuUsage
				variable to be correctly updated
				chrono::system_clock::time_point now =
				chrono::system_clock::now(); if (now -
				*_lastEncodingAcceptedTime <
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
						string errorMessage =
				string("Too early to accept a new encoding
				request")
								+ ", seconds
				since the last request: "
										+
				to_string(chrono::duration_cast<chrono::seconds>(
				now -
				*_lastEncodingAcceptedTime).count())
								+ ", " +
				NoEncodingAvailable().what();

						_logger->warn(__FILEREF__ +
				errorMessage);

						sendError(request, 400,
				errorMessage);

						// throw
				runtime_error(noEncodingAvailableMessage);
						return;
				}
				*/

				// 2022-11-23: ho visto che, in caso di
				// autoRenew, monitoring generates errors and
				// trys to kill
				//		the process. Moreover the
				// selectedLiveRecording->_errorMessage remain
				// initialized with the error (like killed
				// because segment file is not present).
				// For this reason, _recordingStart is
				// initialized to make sure monitoring does not
				// perform his checks before recorder is not
				// really started. _recordingStart will be
				// initialized correctly into the
				// liveRecorderThread method
				selectedLiveRecording->_recordingStart = chrono::system_clock::now() + chrono::seconds(60);
				selectedLiveRecording->_available = false;
				selectedLiveRecording->_childPid = 0; // not running
				selectedLiveRecording->_killToRestartByEngine = false;
				selectedLiveRecording->_encodingJobKey = encodingJobKey;

				_logger->info(
					__FILEREF__ + "Creating liveRecorder thread" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", "
					"selectedLiveRecording->_encodingJobKey: " +
					to_string(encodingJobKey) + ", requestBody: " + requestBody
				);
				thread liveRecorderThread(
					&FFMPEGEncoder::liveRecorderThread, this, selectedLiveRecording, ingestionJobKey, encodingJobKey, requestBody
				);
				liveRecorderThread.detach();

				// *_lastEncodingAcceptedTime =
				// chrono::system_clock::now();
			}
			catch (exception &e)
			{
				selectedLiveRecording->_available = true;
				selectedLiveRecording->_childPid = 0; // not running

				_logger->error(
					__FILEREF__ + "liveRecorder failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", "
					"selectedLiveRecording->_encodingJobKey: " +
					to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		try
		{
			json responseBodyRoot;
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ + "liveRecorderThread failed" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", selectedEncoding->_encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
			);

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else if (method == "liveProxy"
			 // || method == "vodProxy"
			 || method == "liveGrid"
			 // || method == "countdown"
	)
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;
			bool freeEncodingFound = false;
			bool encodingAlreadyRunning = false;
			// for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>
			// liveProxy:
			// *_liveProxiesCapability)
			int maxLiveProxiesCapability = getMaxLiveProxiesCapability(ingestionJobKey);
			for (int liveProxyIndex = 0; liveProxyIndex < maxLiveProxiesCapability; liveProxyIndex++)
			{
				shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy = (*_liveProxiesCapability)[liveProxyIndex];

				if (liveProxy->_available)
				{
					if (!freeEncodingFound)
					{
						freeEncodingFound = true;
						selectedLiveProxy = liveProxy;
					}
				}
				else
				{
					if (liveProxy->_encodingJobKey == encodingJobKey)
						encodingAlreadyRunning = true;
				}
			}
			if (encodingAlreadyRunning || !freeEncodingFound)
			{
				string errorMessage;
				if (encodingAlreadyRunning)
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + EncodingIsAlreadyRunning().what();
				else
					errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + NoEncodingAvailable().what();

				_logger->error(
					__FILEREF__ + errorMessage + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey) + ", encodingAlreadyRunning: " + to_string(encodingAlreadyRunning) +
					", freeEncodingFound: " + to_string(freeEncodingFound)
				);

				sendError(request, 400, errorMessage);

				// throw
				// runtime_error(noEncodingAvailableMessage);
				return;
			}

			try
			{
				/*
				 * 2021-09-15: liveProxy cannot wait. Scenario:
				received a lot of requests that fail
				 * Those requests set _lastEncodingAcceptedTime
				and delay a lot
				 * the requests that would work fine
				 * Consider that Live-Proxy is a Task where
				FFMPEGEncoder
				 * could receive a lot of close requests
				lock_guard<mutex>
				locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the
				previous encoding request
				// in order to give time to the cpuUsage
				variable to be correctly updated
				chrono::system_clock::time_point now =
				chrono::system_clock::now(); if (now -
				*_lastEncodingAcceptedTime <
						chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
						int secondsToWait =
								chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count()
				- chrono::duration_cast<chrono::seconds>( now -
				*_lastEncodingAcceptedTime).count(); string
				errorMessage = string("Too early to accept a new
				encoding request")
								+ ",
				encodingJobKey: " + to_string(encodingJobKey)
								+ ", seconds
				since the last request: "
										+
				to_string(chrono::duration_cast<chrono::seconds>(
				now -
				*_lastEncodingAcceptedTime).count())
								+ ",
				secondsToWait: " + to_string(secondsToWait)
								+ ", " +
				NoEncodingAvailable().what();

						_logger->warn(__FILEREF__ +
				errorMessage);

						sendError(request, 400,
				errorMessage);

						// throw
				runtime_error(noEncodingAvailableMessage);
						return;
				}
				*/

				selectedLiveProxy->_available = false;
				selectedLiveProxy->_childPid = 0; // not running
				selectedLiveProxy->_killToRestartByEngine = false;
				selectedLiveProxy->_encodingJobKey = encodingJobKey;
				selectedLiveProxy->_method = method;

				if (method == "liveProxy")
				{
					_logger->info(
						__FILEREF__ + "Creating liveProxy thread" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", "
						"selectedLiveProxy->_"
						"encodingJobKey: " +
						to_string(encodingJobKey) + ", requestBody: " + requestBody
					);
					thread liveProxyThread(&FFMPEGEncoder::liveProxyThread, this, selectedLiveProxy, ingestionJobKey, encodingJobKey, requestBody);
					liveProxyThread.detach();
				}
				/*
				else if (method == "vodProxy")
				{
						_logger->info(__FILEREF__ +
				"Creating vodProxy thread"
								+ ",
				selectedLiveProxy->_encodingJobKey: " +
				to_string(encodingJobKey)
								+ ",
				requestBody: " + requestBody
						);
						thread
				vodProxyThread(&FFMPEGEncoder::vodProxyThread,
								this,
				selectedLiveProxy, encodingJobKey, requestBody);
				vodProxyThread.detach();
				}
				*/
				else if (method == "liveGrid")
				{
					_logger->info(
						__FILEREF__ + "Creating liveGrid thread" + ", ingestionJobKey: " + to_string(ingestionJobKey) +
						", "
						"selectedLiveProxy->_"
						"encodingJobKey: " +
						to_string(encodingJobKey) + ", requestBody: " + requestBody
					);
					thread liveGridThread(&FFMPEGEncoder::liveGridThread, this, selectedLiveProxy, ingestionJobKey, encodingJobKey, requestBody);
					liveGridThread.detach();
				}
				/*
				else // if (method == "countdown")
				{
						_logger->info(__FILEREF__ +
				"Creating countdown thread"
								+ ",
				selectedLiveProxy->_encodingJobKey: " +
				to_string(encodingJobKey)
								+ ",
				requestBody: " + requestBody
						);
						thread
				awaitingTheBeginningThread(&FFMPEGEncoder::awaitingTheBeginningThread,
								this,
				selectedLiveProxy, encodingJobKey, requestBody);
				awaitingTheBeginningThread.detach();
				}
				*/

				// *_lastEncodingAcceptedTime =
				// chrono::system_clock::now();
			}
			catch (exception &e)
			{
				selectedLiveProxy->_available = true;
				selectedLiveProxy->_childPid = 0; // not running

				_logger->error(
					__FILEREF__ + "liveProxyThread failed" + ", method: " + method + ", ingestionJobKey: " + to_string(ingestionJobKey) +
					", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
				);

				string errorMessage = string("Internal server error");
				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		try
		{
			json responseBodyRoot;
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::getHostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
		catch (exception &e)
		{
			_logger->error(
				__FILEREF__ +
				"liveProxy/vodProxy/liveGrid/awaitingTheBeginning "
				"Thread "
				"failed" +
				", method: " + method + ", ingestionJobKey: " + to_string(ingestionJobKey) +
				", selectedLiveProxy->_encodingJobKey: " + to_string(encodingJobKey) + ", requestBody: " + requestBody + ", e.what(): " + e.what()
			);

			string errorMessage = string("Internal server error");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else if (method == "encodingStatus")
	{
		int64_t ingestionJobKey = getQueryParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		/*
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);
		*/

		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);
		/*
		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
		*/

		chrono::system_clock::time_point startEncodingStatus = chrono::system_clock::now();

		bool encodingFound = false;
		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;

		bool liveProxyFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

		bool liveRecordingFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveRecording> selectedLiveRecording;

		bool encodingCompleted = false;
		shared_ptr<FFMPEGEncoderBase::EncodingCompleted> selectedEncodingCompleted;

		int encodingCompletedMutexDuration = -1;
		int encodingMutexDuration = -1;
		int liveProxyMutexDuration = -1;
		int liveRecordingMutexDuration = -1;
		{
			chrono::system_clock::time_point startLockTime = chrono::system_clock::now();
			lock_guard<mutex> locker(*_encodingCompletedMutex);
			chrono::system_clock::time_point endLockTime = chrono::system_clock::now();
			encodingCompletedMutexDuration = chrono::duration_cast<chrono::seconds>(endLockTime - startLockTime).count();

			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it = _encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		if (!encodingCompleted)
		{
			// next block is to make the lock free as soon as the check is done
			{
				for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding : *_encodingsCapability)
				{
					if (encoding->_encodingJobKey == encodingJobKey)
					{
						encodingFound = true;
						selectedEncoding = encoding;

						break;
					}
				}
			}

			if (!encodingFound)
			{
				// next block is to make the lock free as soon as
				// the check is done
				{
					/*
					 * 2020-11-30
					 * CIBORTV PROJECT. SCENARIO:
					 *	- The encodingStatus is called
					 *by the mmsEngine periodically for each
					 *running transcoding. Often this method
					 *takes a lot of times to answer, depend
					 *on the period encodingStatus is
					 *called, 50 secs in case it is called
					 *every 5 seconds, 35 secs in case it is
					 *called every 30 secs. This because the
					 *Lock (lock_guard) does not provide any
					 *guarantee, in case there are a lot of
					 *threads, as it is our case, may be a
					 *thread takes the lock and the OS
					 *switches to another thread. It could
					 *take time the OS re-switch on the
					 *previous thread in order to release
					 *the lock.
					 *
					 *	To solve this issue we should
					 *found an algorithm that guarantees the
					 *Lock is managed in a fast way also in
					 *case of a lot of threads. I do not
					 *have now a solution for this. For this
					 *since I thought:
					 *	- in case of __VECTOR__ all the
					 *structure is "fixes", every thing is
					 *allocated at the beggining and do not
					 *change
					 *	- so for this method, since it
					 *checks some attribute in a "static"
					 *structure, WE MAY AVOID THE USING OF
					 *THE LOCK
					 *
					 */
					for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy : *_liveProxiesCapability)
					{
						if (liveProxy->_encodingJobKey == encodingJobKey)
						{
							liveProxyFound = true;
							selectedLiveProxy = liveProxy;

							break;
						}
					}
				}

				if (!liveProxyFound)
				{
					for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording : *_liveRecordingsCapability)
					{
						if (liveRecording->_encodingJobKey == encodingJobKey)
						{
							liveRecordingFound = true;
							selectedLiveRecording = liveRecording;

							break;
						}
					}
				}
			}
		}

		chrono::system_clock::time_point endLookingForEncodingStatus = chrono::system_clock::now();

		SPDLOG_INFO(
			"encodingStatus"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", encodingFound: {}"
			", available: {}"
			", childPid: {}"
			", liveProxyFound: {}"
			", liveRecordingFound: {}"
			", encodingCompleted: {}"
			", encodingCompletedMutexDuration: {}"
			", encodingMutexDuration: {}"
			", liveProxyMutexDuration: {}"
			", liveRecordingMutexDuration: {}"
			", @MMS statistics@ - duration looking for encodingStatus (secs): @{}@",
			ingestionJobKey, encodingJobKey, encodingFound, encodingFound ? to_string(selectedEncoding->_available) : "not available",
			encodingFound ? to_string(selectedEncoding->_childPid) : "not available", liveProxyFound, liveRecordingFound, encodingCompleted,
			encodingCompletedMutexDuration, encodingMutexDuration, liveProxyMutexDuration, liveRecordingMutexDuration,
			chrono::duration_cast<chrono::seconds>(endLookingForEncodingStatus - startEncodingStatus).count()
		);

		string responseBody;
		if (!encodingFound && !liveProxyFound && !liveRecordingFound && !encodingCompleted)
		{
			// it should never happen
			json responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			field = "pid";
			responseBodyRoot[field] = 0;

			field = "killedByUser";
			responseBodyRoot[field] = false;

			field = "encodingFinished";
			responseBodyRoot[field] = true;

			field = "encodingProgress";
			responseBodyRoot[field] = 100.0;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}
		else
		{
			if (encodingCompleted)
			{
				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedEncodingCompleted->_encodingJobKey;
				responseBodyRoot["pid"] = 0;
				// killedByUser true implica una uscita dal loop in EncoderProxy (nell'engine). Nel caso in cui pero' il kill era stato fatto
				// solo per eseguire un restart dell'EncoderProxy verso un nuovo encoder, mettiamo killedByUser a false
				responseBodyRoot["killedByUser"] = selectedEncodingCompleted->_killedByUser && selectedEncodingCompleted->_killToRestartByEngine
													   ? false
													   : selectedEncodingCompleted->_killedByUser;
				responseBodyRoot["urlForbidden"] = selectedEncodingCompleted->_urlForbidden;
				responseBodyRoot["urlNotFound"] = selectedEncodingCompleted->_urlNotFound;
				responseBodyRoot["completedWithError"] = selectedEncodingCompleted->_completedWithError;
				responseBodyRoot["errorMessage"] = selectedEncodingCompleted->_errorMessage;
				responseBodyRoot["encodingFinished"] = true;
				responseBodyRoot["encodingProgress"] = 100.0;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else if (encodingFound)
			{
				double encodingProgress = -2.0;
				if (selectedEncoding->_ffmpegTerminatedSuccessful)
				{
					// _ffmpegTerminatedSuccessful è stata introdotta perchè, soprattutto in caso di transcoder esterno, una volta
					// che l'encoding è terminato, è necessario eseguire l'ingestion in MMS (PUSH). Questo spesso richiede
					// parecchio tempo e la GUI mostra 0.0 come percentuale di encoding perchè ovviamente non abbiamo piu il file di
					// log dell'encoding. Questo flag fa si che, in questo periodo di upload del contenuto in MMS (PUSH), la
					// percentuale mostrata sia 100%

					// Il problema esiste anche per i transcoder interni che eseguono invece una move. Nel caso di file
					// grandi, anche la move potrebbe richiedere parecchio tempo e, grazie a questa variabile, la GUI mostrera
					// anche in questo caso 100%

					encodingProgress = 100.0;
				}
				else
				{
					try
					{
						chrono::system_clock::time_point startEncodingProgress = chrono::system_clock::now();

						encodingProgress = selectedEncoding->_ffmpeg->getEncodingProgress();

						chrono::system_clock::time_point endEncodingProgress = chrono::system_clock::now();
						_logger->info(
							__FILEREF__ +
							"getEncodingProgress "
							"statistics" +
							", @MMS statistics@ - "
							"encodingProgress (secs): "
							"@" +
							to_string(chrono::duration_cast<chrono::seconds>(endEncodingProgress - startEncodingProgress).count()) + "@"
						);
					}
					catch (FFMpegEncodingStatusNotAvailable &e)
					{
						SPDLOG_INFO(
							"_ffmpeg->getEncodingProgress failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);

						// sendError(request, 500,
						// errorMessage);

						// throw e;
						// return;
					}
					catch (exception &e)
					{
						string errorMessage = fmt::format(
							"_ffmpeg->getEncodingProgress failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
						_logger->error(__FILEREF__ + errorMessage);

						// sendError(request, 500,
						// errorMessage);

						// throw e;
						// return;
					}
				}

				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedEncoding->_encodingJobKey;
				responseBodyRoot["pid"] = selectedEncoding->_childPid;
				responseBodyRoot["killedByUser"] = false;
				responseBodyRoot["urlForbidden"] = false;
				responseBodyRoot["urlNotFound"] = false;
				responseBodyRoot["errorMessage"] = selectedEncoding->_errorMessage;
				responseBodyRoot["encodingFinished"] = encodingCompleted;
				if (encodingProgress == -2.0)
				{
					if (selectedEncoding->_available && !encodingCompleted) // non dovrebbe
																			// accadere mai
						responseBodyRoot["encodingProgress"] = 100.0;
					else
						responseBodyRoot["encodingProgress"] = nullptr;
				}
				else
					responseBodyRoot["encodingProgress"] = encodingProgress;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else if (liveProxyFound)
			{
				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = selectedLiveProxy->_ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedLiveProxy->_encodingJobKey;
				responseBodyRoot["pid"] = selectedLiveProxy->_childPid;
				responseBodyRoot["realTimeFrameRate"] = selectedLiveProxy->_realTimeFrameRate;
				responseBodyRoot["realTimeBitRate"] = selectedLiveProxy->_realTimeBitRate;
				responseBodyRoot["numberOfRestartBecauseOfFailure"] = selectedLiveProxy->_numberOfRestartBecauseOfFailure;
				responseBodyRoot["killedByUser"] = false;
				responseBodyRoot["urlForbidden"] = false;
				responseBodyRoot["urlNotFound"] = false;
				responseBodyRoot["errorMessage"] = selectedLiveProxy->_errorMessage;
				responseBodyRoot["encodingFinished"] = encodingCompleted;

				// 2020-06-11: it's a live, it does not have
				// sense the encoding progress
				responseBodyRoot["encodingProgress"] = nullptr;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else // if (liveRecording)
			{
				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = selectedLiveRecording->_ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedLiveRecording->_encodingJobKey;
				responseBodyRoot["pid"] = selectedLiveRecording->_childPid;
				responseBodyRoot["realTimeFrameRate"] = selectedLiveRecording->_realTimeFrameRate;
				responseBodyRoot["realTimeBitRate"] = selectedLiveRecording->_realTimeBitRate;
				responseBodyRoot["numberOfRestartBecauseOfFailure"] = selectedLiveRecording->_numberOfRestartBecauseOfFailure;
				responseBodyRoot["killedByUser"] = false;
				responseBodyRoot["urlForbidden"] = false;
				responseBodyRoot["urlNotFound"] = false;
				responseBodyRoot["errorMessage"] = selectedLiveRecording->_errorMessage;
				responseBodyRoot["encodingFinished"] = encodingCompleted;

				// 2020-10-13: we do not have here the
				// information to calculate the encoding
				// progress,
				//	it is calculated in
				// EncoderVideoAudioProxy.cpp
				responseBodyRoot["encodingProgress"] = nullptr;

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
		}

		chrono::system_clock::time_point endEncodingStatus = chrono::system_clock::now();

		SPDLOG_INFO(
			"encodingStatus"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", encodingFound: {}"
			", liveProxyFound: {}"
			", liveRecordingFound: {}"
			", encodingCompleted: {}"
			", responseBody: {}"
			", @MMS statistics@ - duration encodingStatus (secs): @{}@",
			ingestionJobKey, encodingJobKey, encodingFound, liveProxyFound, liveRecordingFound, encodingCompleted, JSONUtils::toString(responseBody),
			to_string(chrono::duration_cast<chrono::seconds>(endEncodingStatus - startEncodingStatus).count())
		);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "filterNotification")
	{
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		bool encodingCompleted = false;
		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it = _encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
				encodingCompleted = true;
		}

		if (encodingCompleted)
		{
			string errorMessage = fmt::format(
				"filterNotification, encoding is already finished"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodingCompleted: {}",
				ingestionJobKey, ingestionJobKey, encodingCompleted
			);

			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		auto filterNameIt = queryParameters.find("filterName");
		if (filterNameIt == queryParameters.end())
		{
			string errorMessage = string("The 'filterName' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		string filterName = filterNameIt->second;

		bool liveProxyFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

		for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy : *_liveProxiesCapability)
		{
			if (liveProxy->_encodingJobKey == encodingJobKey)
			{
				liveProxyFound = true;
				selectedLiveProxy = liveProxy;

				break;
			}
		}

		if (!liveProxyFound)
		{
			string errorMessage = fmt::format(
				"filterNotification, liveProxy not found"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", liveProxyFound: {}",
				ingestionJobKey, ingestionJobKey, liveProxyFound
			);

			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		string mmsWorkflowIngestionURL;
		string workflowMetadata;
		try
		{
			json ingestedParametersRoot = selectedLiveProxy->_ingestedParametersRoot;
			json encodingParametersRoot = selectedLiveProxy->_encodingParametersRoot;

			workflowMetadata = buildFilterNotificationIngestionWorkflow(ingestionJobKey, filterName, ingestedParametersRoot);
			if (workflowMetadata != "")
			{
				int64_t userKey;
				string apiKey;
				{
					string field = "internalMMS";
					if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
					{
						json internalMMSRoot = ingestedParametersRoot[field];

						field = "credentials";
						if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
						{
							json credentialsRoot = internalMMSRoot[field];

							field = "userKey";
							userKey = JSONUtils::asInt64(credentialsRoot, field, -1);

							field = "apiKey";
							string apiKeyEncrypted = JSONUtils::asString(credentialsRoot, field, "");
							apiKey = Encrypt::opensslDecrypt(apiKeyEncrypted);
						}
					}
				}

				{
					string field = "mmsWorkflowIngestionURL";
					if (!JSONUtils::isMetadataPresent(encodingParametersRoot, field))
					{
						string errorMessage = __FILEREF__ + "Field is not present or it is null" + ", ingestionJobKey: " +
											  to_string(ingestionJobKey)
											  // + ", encodingJobKey: " +
											  // to_string(encodingJobKey)
											  + ", Field: " + field;
						_logger->error(errorMessage);

						throw runtime_error(errorMessage);
					}
					mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
				}

				vector<string> otherHeaders;
				string sResponse =
					MMSCURL::httpPostString(
						_logger, ingestionJobKey, mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, to_string(userKey), apiKey, workflowMetadata,
						"application/json", // contentType
						otherHeaders,
						3 // maxRetryNumber
					)
						.second;
			}
		}
		catch (runtime_error e)
		{
			string errorMessage = fmt::format(
				"Ingested URL failed (runtime_error)"
				", ingestionJobKey: {}"
				", mmsWorkflowIngestionURL: {}"
				", workflowMetadata: {}"
				", exception: {}",
				ingestionJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		catch (exception e)
		{
			string errorMessage = fmt::format(
				"Ingested URL failed (exception)"
				", ingestionJobKey: {}"
				", mmsWorkflowIngestionURL: {}"
				", workflowMetadata: {}"
				", exception: {}",
				ingestionJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		string responseBody;
		{
			// it should never happen
			json responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "killEncodingJob")
	{
		int64_t ingestionJobKey = getQueryParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		/*
		auto ingestionJobKeyIt = queryParameters.find("ingestionJobKey");
		if (ingestionJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'ingestionJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t ingestionJobKey = stoll(ingestionJobKeyIt->second);
		*/

		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);
		/*
		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);
		*/

		// killType: "kill", "restartWithinEncoder", "killToRestartByEngine"
		string killType = getQueryParameter(queryParameters, "killType", string("kill"), false);
		/*
		bool lightKill = false;
		auto lightKillIt = queryParameters.find("lightKill");
		if (lightKillIt != queryParameters.end())
			lightKill = lightKillIt->second == "true" ? true : false;
		*/

		SPDLOG_INFO(
			"Received killEncodingJob"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", lightKill: {}",
			ingestionJobKey, encodingJobKey, killType
		);

		// pid_t pidToBeKilled;
		bool encodingFound = false;
		bool liveProxyFound = false;
		bool liveRecorderFound = false;
		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;

		{
			for (shared_ptr<FFMPEGEncoderBase::Encoding> encoding : *_encodingsCapability)
			{
				if (encoding->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					selectedEncoding = encoding;
					// pidToBeKilled = encoding->_childPid;

					break;
				}
			}
		}

		if (!encodingFound)
		{
			for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy : *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					liveProxyFound = true;
					selectedEncoding = liveProxy;
					// pidToBeKilled = liveProxy->_childPid;

					break;
				}
			}
		}

		if (!encodingFound && !liveProxyFound)
		{
			for (shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording : *_liveRecordingsCapability)
			{
				if (liveRecording->_encodingJobKey == encodingJobKey)
				{
					liveRecorderFound = true;
					// pidToBeKilled = liveRecording->_childPid;
					selectedEncoding = liveRecording;

					break;
				}
			}
		}

		if (!encodingFound && !liveProxyFound && !liveRecorderFound)
		{
			string errorMessage = "ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingJobKey: " + to_string(encodingJobKey) + ", " +
								  NoEncodingJobKeyFound().what();
			SPDLOG_ERROR(errorMessage);

			sendError(request, 400, errorMessage);

			// throw runtime_error(errorMessage);
			return;
		}

		SPDLOG_INFO(
			"ProcessUtility::killProcess. Found Encoding to kill"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", _childPid: {}"
			", killType: {}",
			ingestionJobKey, encodingJobKey, selectedEncoding->_childPid, killType
		);

		// if (pidToBeKilled == 0)
		if (selectedEncoding->_childPid == 0)
		{
			SPDLOG_ERROR(
				"The EncodingJob seems not running (see _childPid)"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _childPid: {}",
				ingestionJobKey, encodingJobKey, selectedEncoding->_childPid
			);

			string errorMessage = string("Internal server error");
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw runtime_error(errorMessage);
		}

		try
		{
			chrono::system_clock::time_point startKillProcess = chrono::system_clock::now();

			// killType: "kill", "restartWithinEncoder", "killToRestartByEngine"
			if (killType == "restartWithinEncoder")
			{
				// 2022-11-02: SIGQUIT is managed inside
				// FFMpeg.cpp by liverecording e liveProxy
				// 2023-02-18: using SIGQUIT, the process was
				// not stopped, it worked with SIGTERM
				//	SIGTERM now is managed by FFMpeg.cpp too
				termProcess(selectedEncoding, encodingJobKey, "unknown", "received killEncodingJob", false);
			}
			else
			{
				if (killType == "killToRestartByEngine")
					selectedEncoding->_killToRestartByEngine = true;
				termProcess(selectedEncoding, encodingJobKey, "unknown", "received killEncodingJob", true);
			}

			chrono::system_clock::time_point endKillProcess = chrono::system_clock::now();
			SPDLOG_INFO(
				"killProcess statistics"
				", @MMS statistics@ - killProcess (secs): @{}@",
				chrono::duration_cast<chrono::seconds>(endKillProcess - startKillProcess).count()
			);
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("ProcessUtility::killProcess failed") + ", ingestionJobKey: " + to_string(ingestionJobKey) +
								  ", encodingJobKey: " + to_string(encodingJobKey) + ", _childPid: " + to_string(selectedEncoding->_childPid) +
								  ", e.what(): " + e.what();
			SPDLOG_ERROR(errorMessage);

			sendError(request, 500, errorMessage);

			throw e;
		}

		string responseBody;
		{
			json responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			field = "pid";
			responseBodyRoot[field] = selectedEncoding->_childPid;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "changeLiveProxyPlaylist")
	{
		/*
		bool isAdminAPI = get<1>(workspaceAndFlags);
		if (!isAdminAPI)
		{
			string errorMessage = string("APIKey flags does not have
		the ADMIN permission"
					", isAdminAPI: " + to_string(isAdminAPI)
					);
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		bool interruptPlaylist = false;
		auto interruptPlaylistIt = queryParameters.find("interruptPlaylist");
		if (interruptPlaylistIt != queryParameters.end())
			interruptPlaylist = interruptPlaylistIt->second == "true";

		_logger->info(__FILEREF__ + "Received changeLiveProxyPlaylist" + ", encodingJobKey: " + to_string(encodingJobKey));

		bool encodingFound = false;

		json newInputsRoot;
		try
		{
			newInputsRoot = JSONUtils::toJson(requestBody);
		}
		catch (runtime_error &e)
		{
			_logger->error(__FILEREF__ + e.what());

			sendError(request, 500, e.what());

			return;
		}

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

			for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy : *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					selectedLiveProxy = liveProxy;

					break;
				}
			}

			if (!encodingFound)
			{
				string errorMessage = string("EncodingJobKey: ") + to_string(encodingJobKey) + ", " + NoEncodingJobKeyFound().what();

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
				return;
			}

			{
				_logger->info(
					__FILEREF__ + "Replacing the LiveProxy playlist" + ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey) +
					", encodingJobKey: " + to_string(encodingJobKey)
				);

				lock_guard<mutex> locker(selectedLiveProxy->_inputsRootMutex);

				selectedLiveProxy->_inputsRoot = newInputsRoot;
			}

			// 2022-10-21: abbiamo due opzioni:
			//	- apply the new playlist now
			//	- apply the new playlist at the end of current
			// media
			if (interruptPlaylist)
			{
				try
				{
					termProcess(selectedLiveProxy, selectedLiveProxy->_ingestionJobKey, "unknown", "received changeLiveProxyPlaylist", false);
				}
				catch (runtime_error &e)
				{
					string errorMessage = string("ProcessUtility::"
												 "termProcess failed") +
										  ", ingestionJobKey: " + to_string(selectedLiveProxy->_ingestionJobKey) +
										  ", encodingJobKey: " + to_string(selectedLiveProxy->_encodingJobKey) +
										  ", _childPid: " + to_string(selectedLiveProxy->_childPid) + ", e.what(): " + e.what();
					_logger->error(__FILEREF__ + errorMessage);
				}
			}
		}

		string responseBody;
		{
			json responseBodyRoot;

			string field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "changeLiveProxyOverlayText")
	{
		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}
		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		string newOverlayText = requestBody;

		bool encodingFound = false;

		{
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

			for (shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy : *_liveProxiesCapability)
			{
				if (liveProxy->_encodingJobKey == encodingJobKey)
				{
					encodingFound = true;
					selectedLiveProxy = liveProxy;

					break;
				}
			}

			if (!encodingFound)
			{
				string errorMessage = fmt::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingJobKeyFound().what());

				_logger->error(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw runtime_error(errorMessage);
				return;
			}

			SPDLOG_INFO(
				"Received changeLiveProxyOverlayText"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", requestBody: {}",
				selectedLiveProxy->_ingestionJobKey, encodingJobKey, requestBody
			);

			string textTemporaryFileName =
				selectedLiveProxy->_ffmpeg->getDrawTextTemporaryPathName(selectedLiveProxy->_ingestionJobKey, encodingJobKey, 0);
			if (fs::exists(textTemporaryFileName))
			{
				ofstream of(textTemporaryFileName, ofstream::trunc);
				of << newOverlayText;
				of.flush();
			}
		}

		string responseBody;
		{
			json responseBodyRoot;

			string field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	else if (method == "encodingProgress")
	{
		// 2020-10-13: The encodingProgress API is not called anymore
		// because it is the encodingStatus API returning the
		// encodingProgress

		/*
		bool isAdminAPI = get<1>(workspaceAndFlags);
		if (!isAdminAPI)
		{
			string errorMessage = string("APIKey flags does not have
		the ADMIN permission"
					", isAdminAPI: " + to_string(isAdminAPI)
					);
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		auto encodingJobKeyIt = queryParameters.find("encodingJobKey");
		if (encodingJobKeyIt == queryParameters.end())
		{
			string errorMessage = string("The 'encodingJobKey' parameter is not found");
			_logger->error(__FILEREF__ + errorMessage);

			sendError(request, 400, errorMessage);

			throw runtime_error(errorMessage);
		}

		int64_t encodingJobKey = stoll(encodingJobKeyIt->second);

		bool encodingCompleted = false;
		shared_ptr<FFMPEGEncoderBase::EncodingCompleted> selectedEncodingCompleted;

		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;
		bool encodingFound = false;

		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;
		bool liveProxyFound = false;

		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it = _encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
			{
				encodingCompleted = true;
				selectedEncodingCompleted = it->second;
			}
		}

		/*
		if (!encodingCompleted)
		{
				// next block is to make the lock free as soon as the check is done
				{
						lock_guard<mutex>
	locker(*_encodingMutex);

						for
	(shared_ptr<FFMPEGEncoderBase::Encoding> encoding:
	*_encodingsCapability)
						{
								if
	(encoding->_encodingJobKey == encodingJobKey)
								{
										encodingFound = true;
										selectedEncoding = encoding;

										break;
								}
						}
				}

				if (!encodingFound)
				{
						lock_guard<mutex>
	locker(*_liveProxyMutex);

						for
	(shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxy:
	*_liveProxiesCapability)
						{
								if
	(liveProxy->_encodingJobKey == encodingJobKey)
								{
										liveProxyFound = true;
										selectedLiveProxy = liveProxy;

										break;
								}
						}
				}
		}

	if (!encodingCompleted && !encodingFound && !liveProxyFound)
	{
	string errorMessage = string("EncodingJobKey: ") +
	to_string(encodingJobKey)
						+ ", " +
	NoEncodingJobKeyFound().what();

	_logger->error(__FILEREF__ + errorMessage);

	sendError(request, 400, errorMessage);

	// throw runtime_error(errorMessage);
				return;
	}

	if (encodingFound)
		{
				int encodingProgress;
				try
				{
						encodingProgress =
	selectedEncoding->_ffmpeg->getEncodingProgress();
				}
				catch(FFMpegEncodingStatusNotAvailable& e)
				{
						string errorMessage =
	string("_ffmpeg->getEncodingProgress failed")
										+ ", encodingJobKey: " +
	to_string(encodingJobKey)
								+ ", e.what(): "
	+ e.what()
										;
						_logger->info(__FILEREF__ +
	errorMessage);

						sendError(request, 500,
	errorMessage);

						// throw e;
						return;
				}
				catch(exception& e)
				{
						string errorMessage =
	string("_ffmpeg->getEncodingProgress failed")
			+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", e.what(): "
	+ e.what()
			;
						_logger->error(__FILEREF__ +
	errorMessage);

						sendError(request, 500,
	errorMessage);

						// throw e;
						return;
				}

				string responseBody = string("{ ")
						+ "\"encodingJobKey\": " +
	to_string(encodingJobKey)
						+ ", \"pid\": " +
	to_string(selectedEncoding->_childPid)
						+ ", \"encodingProgress\": " +
	to_string(encodingProgress) + " "
						+ "}";

				sendSuccess(request, 200, responseBody);
		}
		else if (liveProxyFound)
		{
				int encodingProgress;
				try
				{
						// 2020-06-11: it's a live, it
	does not have sense the encoding progress
						// encodingProgress =
	selectedLiveProxy->_ffmpeg->getEncodingProgress(); encodingProgress = -1;
				}
				catch(FFMpegEncodingStatusNotAvailable& e)
				{
						string errorMessage =
	string("_ffmpeg->getEncodingProgress failed")
										+ ", encodingJobKey: " +
	to_string(encodingJobKey)
								+ ", e.what(): "
	+ e.what()
										;
						_logger->info(__FILEREF__ +
	errorMessage);

						sendError(request, 500,
	errorMessage);

						// throw e;
						return;
				}
				catch(exception& e)
				{
						string errorMessage =
	string("_ffmpeg->getEncodingProgress failed")
			+ ", encodingJobKey: " + to_string(encodingJobKey)
								+ ", e.what(): "
	+ e.what()
			;
						_logger->error(__FILEREF__ +
	errorMessage);

						sendError(request, 500,
	errorMessage);

						// throw e;
						return;
				}

				string responseBody = string("{ ")
						+ "\"encodingJobKey\": " +
	to_string(encodingJobKey)
						+ ", \"pid\": " +
	to_string(selectedLiveProxy->_childPid)
						+ ", \"encodingProgress\": " +
	to_string(encodingProgress) + " "
						+ "}";

				sendSuccess(request, 200, responseBody);
		}
		else if (encodingCompleted)
		{
				int encodingProgress = 100;

				string responseBody = string("{ ")
						+ "\"encodingJobKey\": " +
	to_string(encodingJobKey)
						+ ", \"encodingProgress\": " +
	to_string(encodingProgress) + " "
						+ "}";

				sendSuccess(request, 200, responseBody);
		}
		else // if (!encodingCompleted)
		{
				string errorMessage = method + ": " +
	FFMpegEncodingStatusNotAvailable().what()
								+ ",
	encodingJobKey: " + to_string(encodingJobKey)
								+ ",
	encodingCompleted: " + to_string(encodingCompleted)
								;
				_logger->info(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
		}
		*/
	}
	else
	{
		string errorMessage =
			string("No API is matched") + ", requestURI: " + requestURI + ", method: " + method + ", requestMethod: " + requestMethod;
		_logger->error(__FILEREF__ + errorMessage);

		sendError(request, 400, errorMessage);

		throw runtime_error(errorMessage);
	}

	if (chrono::system_clock::now() - *_lastEncodingCompletedCheck >= chrono::seconds(_encodingCompletedRetentionInSeconds))
	{
		*_lastEncodingCompletedCheck = chrono::system_clock::now();
		encodingCompletedRetention();
	}

	/* this statistics information is already present in APICommon.cpp
	chrono::system_clock::time_point endManageRequestAndResponse =
	chrono::system_clock::now(); _logger->info(__FILEREF__ +
	"manageRequestAndResponse"
			+ ", method: " + method
			+ ", @MMS statistics@ - duration
	manageRequestAndResponse (secs): @"
					+
	to_string(chrono::duration_cast<chrono::seconds>(endManageRequestAndResponse
	- startManageRequestAndResponse).count()) + "@"
	);
	*/
}

void FFMPEGEncoder::checkAuthorization(string sThreadId, string userName, string password)
{
	string userKey = userName;
	string apiKey = password;

	if (userKey != _encoderUser || apiKey != _encoderPassword)
	{
		SPDLOG_ERROR(
			"Username/password of the basic authorization are wrong"
			", _requestIdentifier: {}"
			", threadId: {}"
			", userKey: {}"
			", apiKey: {}",
			_requestIdentifier, sThreadId, userKey, apiKey
		);

		throw CheckAuthorizationFailed();
	}
}

bool FFMPEGEncoder::basicAuthenticationRequired(string requestURI, unordered_map<string, string> queryParameters)
{
	bool basicAuthenticationRequired = true;

	auto methodIt = queryParameters.find("method");
	if (methodIt == queryParameters.end())
	{
		SPDLOG_ERROR("The 'method' parameter is not found");

		return basicAuthenticationRequired;
	}
	string method = methodIt->second;

	if (method == "registerUser" || method == "confirmRegistration" || method == "createTokenToResetPassword" || method == "resetPassword" ||
		method == "login" || method == "manageHTTPStreamingManifest_authorizationThroughParameter" ||
		method == "deliveryAuthorizationThroughParameter" || method == "deliveryAuthorizationThroughPath" ||
		method == "status" // often used as healthy check
	)
		basicAuthenticationRequired = false;

	// This is the authorization asked when the deliveryURL is received by
	// nginx Here the token is checked and it is not needed any basic
	// authorization
	if (requestURI == "/catramms/delivery/authorization")
		basicAuthenticationRequired = false;

	return basicAuthenticationRequired;
}

void FFMPEGEncoder::encodeContentThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		EncodeContent encodeContent(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		encodeContent.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::overlayImageOnVideoThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		OverlayImageOnVideo overlayImageOnVideo(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		overlayImageOnVideo.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::overlayTextOnVideoThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		OverlayTextOnVideo overlayTextOnVideo(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		overlayTextOnVideo.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::generateFramesThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		GenerateFrames generateFrames(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		generateFrames.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
}

void FFMPEGEncoder::slideShowThread(
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		SlideShow slideShow(encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		slideShow.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
}

void FFMPEGEncoder::videoSpeedThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		VideoSpeed videoSpeed(encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger);
		videoSpeed.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::addSilentAudioThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		AddSilentAudio addSilentAudio(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		addSilentAudio.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::pictureInPictureThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		PictureInPicture pictureInPicture(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		pictureInPicture.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::introOutroOverlayThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		IntroOutroOverlay introOutroOverlay(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		introOutroOverlay.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::cutFrameAccurateThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::Encoding> encoding, int64_t ingestionJobKey, int64_t encodingJobKey, json metadataRoot
)
{
	try
	{
		CutFrameAccurate cutFrameAccurate(
			encoding, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		cutFrameAccurate.encodeContent(metadataRoot);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveRecorderThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveRecording> liveRecording, int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
)
{
	try
	{
		LiveRecorder liveRecorder(
			liveRecording, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger,
			_tvChannelsPortsMutex, _tvChannelPort_CurrentOffset
		);
		liveRecorder.encodeContent(requestBody);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (FFMpegURLForbidden &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (FFMpegURLNotFound &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveProxyThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
)
{
	try
	{
		LiveProxy liveProxy(
			liveProxyData, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger,
			_tvChannelsPortsMutex, _tvChannelPort_CurrentOffset
		);
		liveProxy.encodeContent(requestBody);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (FFMpegURLForbidden &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (FFMpegURLNotFound &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveGridThread(
	// FCGX_Request& request,
	shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> liveProxyData, int64_t ingestionJobKey, int64_t encodingJobKey, string requestBody
)
{
	try
	{
		LiveGrid liveGrid(
			liveProxyData, ingestionJobKey, encodingJobKey, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _logger
		);
		liveGrid.encodeContent(requestBody);
	}
	catch (FFMpegEncodingKilledByUser &e)
	{
		_logger->error(__FILEREF__ + e.what());
	}
	catch (FFMpegURLForbidden &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (FFMpegURLNotFound &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (runtime_error &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
	catch (exception &e)
	{
		_logger->error(__FILEREF__ + e.what());

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::encodingCompletedRetention()
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	for (map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>>::iterator it = _encodingCompletedMap->begin();
		 it != _encodingCompletedMap->end();)
	{
		if (start - (it->second->_timestamp) >= chrono::seconds(_encodingCompletedRetentionInSeconds))
			it = _encodingCompletedMap->erase(it);
		else
			it++;
	}

	chrono::system_clock::time_point end = chrono::system_clock::now();

	_logger->info(
		__FILEREF__ + "encodingCompletedRetention" + ", encodingCompletedMap size: " + to_string(_encodingCompletedMap->size()) +
		", @MMS statistics@ - duration encodingCompleted retention "
		"processing "
		"(secs): @" +
		to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
	);
}

int FFMPEGEncoder::getMaxEncodingsCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForEncoding)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		string lastCPUUsage;
		for (int cpuUsage : *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		_logger->info(__FILEREF__ + "getMaxXXXXCapability" + ", lastCPUUsage: " + lastCPUUsage + ", maxCapability: " + to_string(maxCapability));

		return maxCapability;
	}
}

int FFMPEGEncoder::getMaxLiveProxiesCapability(int64_t ingestionJobKey)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForProxy)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		string lastCPUUsage;
		for (int cpuUsage : *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		SPDLOG_INFO(
			"getMaxXXXXCapability"
			", ingestionJobKey: {}"
			", lastCPUUsage: {}"
			", maxCapability: {}",
			ingestionJobKey, lastCPUUsage, maxCapability
		);

		return maxCapability;
	}

	/*
	int maxLiveProxiesCapability = 1;

	try
	{
			if
	(FileIO::fileExisting(_encoderCapabilityConfigurationPathName))
			{
					json
	encoderCapabilityConfiguration = APICommon::loadConfigurationFile(
							_encoderCapabilityConfigurationPathName.c_str());

					maxLiveProxiesCapability =
	JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
							"maxLiveProxiesCapability",
	1); _logger->info(__FILEREF__ + "Configuration item"
							+ ",
	ffmpeg->maxLiveProxiesCapability: " +
	to_string(maxLiveProxiesCapability)
					);

					if (maxLiveProxiesCapability >
	VECTOR_MAX_CAPACITY)
					{
							_logger->error(__FILEREF__
	+ "getMaxXXXXCapability. maxLiveProxiesCapability cannot be bigger than
	VECTOR_MAX_CAPACITY"
									+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
									+ ",
	maxLiveProxiesCapability: " + to_string(maxLiveProxiesCapability)
									+ ",
	VECTOR_MAX_CAPACITY: " + to_string(VECTOR_MAX_CAPACITY)
							);

							maxLiveProxiesCapability
	= VECTOR_MAX_CAPACITY;
					}

					maxLiveProxiesCapability =
	calculateCapabilitiesBasedOnOtherRunningProcesses( -1,
							maxLiveProxiesCapability,
							-1
					);
			}
			else
			{
					_logger->error(__FILEREF__ +
	"getMaxXXXXCapability. Encoder Capability Configuration Path Name is not
	present"
							+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
					);
			}
	}
	catch (exception e)
	{
			_logger->error(__FILEREF__ + "getMaxXXXXCapability
	failed"
					+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
			);
	}

	_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", maxLiveProxiesCapability: " +
	to_string(maxLiveProxiesCapability)
	);

	return maxLiveProxiesCapability;
	*/
}

int FFMPEGEncoder::getMaxLiveRecordingsCapability(void)
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		lock_guard<mutex> locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForRecording)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		string lastCPUUsage;
		for (int cpuUsage : *_cpuUsage)
			lastCPUUsage += (to_string(cpuUsage) + " ");
		_logger->info(__FILEREF__ + "getMaxXXXXCapability" + ", lastCPUUsage: " + lastCPUUsage + ", maxCapability: " + to_string(maxCapability));

		return maxCapability;
	}

	/*

	try
	{
			if
	(FileIO::fileExisting(_encoderCapabilityConfigurationPathName))
			{
					json
	encoderCapabilityConfiguration = APICommon::loadConfigurationFile(
							_encoderCapabilityConfigurationPathName.c_str());

					maxLiveRecordingsCapability =
	JSONUtils::asInt(encoderCapabilityConfiguration["ffmpeg"],
							"maxLiveRecordingsCapability",
	1); _logger->info(__FILEREF__ + "Configuration item"
							+ ",
	ffmpeg->maxLiveRecordingsCapability: " +
	to_string(maxLiveRecordingsCapability)
					);

					if (maxLiveRecordingsCapability >
	VECTOR_MAX_CAPACITY)
					{
							_logger->error(__FILEREF__
	+ "getMaxXXXXCapability. maxLiveRecordingsCapability cannot be bigger
	than VECTOR_MAX_CAPACITY"
									+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
									+ ",
	maxLiveRecordingsCapability: " + to_string(maxLiveRecordingsCapability)
									+ ",
	VECTOR_MAX_CAPACITY: " + to_string(VECTOR_MAX_CAPACITY)
							);

							maxLiveRecordingsCapability
	= VECTOR_MAX_CAPACITY;
					}

					maxLiveRecordingsCapability =
	calculateCapabilitiesBasedOnOtherRunningProcesses( -1, -1,
							maxLiveRecordingsCapability
					);
			}
			else
			{
					_logger->error(__FILEREF__ +
	"getMaxXXXXCapability. Encoder Capability Configuration Path Name is not
	present"
							+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
					);
			}
	}
	catch (exception e)
	{
			_logger->error(__FILEREF__ +
	"getMaxLiveRecordingsCapability failed"
					+ ",
	_encoderCapabilityConfigurationPathName: " +
	_encoderCapabilityConfigurationPathName
			);
	}

	_logger->info(__FILEREF__ + "getMaxXXXXCapability"
			+ ", maxLiveRecordingsCapability: " +
	to_string(maxLiveRecordingsCapability)
	);

	return maxLiveRecordingsCapability;
	*/
}

string FFMPEGEncoder::buildFilterNotificationIngestionWorkflow(int64_t ingestionJobKey, string filterName, json ingestedParametersRoot)
{
	string workflowMetadata;
	try
	{
		/*
		{
			"label": "<workflow label>",
			"type": "Workflow",
			"task": <task of the event>
		}
		*/

		json eventTaskRoot = nullptr;
		{
			string field = "internalMMS";
			if (JSONUtils::isMetadataPresent(ingestedParametersRoot, field))
			{
				json internalMMSRoot = ingestedParametersRoot[field];

				field = "events";
				if (JSONUtils::isMetadataPresent(internalMMSRoot, field))
				{
					json eventsRoot = internalMMSRoot[field];

					if (filterName == "blackdetect" || filterName == "blackframe" || filterName == "freezedetect" || filterName == "silentdetect")
					{
						field = "onError";
						if (JSONUtils::isMetadataPresent(eventsRoot, field))
							eventTaskRoot = eventsRoot[field];
					}
					/*
							  field = "onSuccess";
							  if (JSONUtils::isMetadataPresent(eventsRoot,
					   field)) addContentRoot[field] = eventsRoot[field];


							  field = "onComplete";
							  if (JSONUtils::isMetadataPresent(eventsRoot,
					   field)) addContentRoot[field] = eventsRoot[field];
					*/
				}
			}
		}

		if (eventTaskRoot == nullptr)
		{
			SPDLOG_ERROR(
				"buildFilterNotificationIngestionWorkflow, no events found in "
				"Workflow"
				", ingestionJobKey: {}",
				ingestionJobKey
			);

			return "";
		}

		json workflowRoot;

		string field = "label";
		workflowRoot[field] = filterName;

		field = "type";
		workflowRoot[field] = "Workflow";

		field = "task";
		workflowRoot[field] = eventTaskRoot[field];

		workflowMetadata = JSONUtils::toString(workflowRoot);

		SPDLOG_INFO(
			"buildFilterNotificationIngestionWorkflow, Workflow generated"
			", ingestionJobKey: {}"
			", workflowMetadata: {}",
			ingestionJobKey, workflowMetadata
		);

		return workflowMetadata;
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"buildFilterNotificationIngestionWorkflow failed"
			", ingestionJobKey: {}"
			", exception: {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"buildFilterNotificationIngestionWorkflow failed"
			", ingestionJobKey: {}"
			", exception: {}",
			ingestionJobKey, e.what()
		);

		throw e;
	}
}

// questo metodo è duplicato anche in FFMPEGEncoderDaemons
void FFMPEGEncoder::termProcess(
	shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding, int64_t ingestionJobKey, string label, string message, bool kill
)
{
	try
	{
		// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
		// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM SIGTERM now is managed by FFMpeg.cpp too
		chrono::system_clock::time_point start = chrono::system_clock::now();
		pid_t previousChildPid = selectedEncoding->_childPid;
		if (previousChildPid == 0)
			return;
		long secondsToWait = 10;
		int counter = 0;
		do
		{
			if (selectedEncoding->_childPid == 0 || selectedEncoding->_childPid != previousChildPid)
				break;

			if (kill)
				ProcessUtility::killProcess(previousChildPid);
			else
				ProcessUtility::termProcess(previousChildPid);
			SPDLOG_INFO(
				"ProcessUtility::termProcess"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", label: {}"
				", message: {}"
				", previousChildPid: {}"
				", selectedEncoding->_childPid: {}"
				", kill: {}"
				", counter: {}",
				ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, previousChildPid, selectedEncoding->_childPid, kill, counter++
			);
			this_thread::sleep_for(chrono::seconds(1));
			// ripete il loop se la condizione è true
		} while (selectedEncoding->_childPid == previousChildPid && chrono::system_clock::now() - start <= chrono::seconds(secondsToWait));
	}
	catch (runtime_error e)
	{
		SPDLOG_ERROR(
			"termProcess failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", label: {}"
			", message: {}"
			", kill: {}"
			", exception: {}",
			ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, kill, e.what()
		);

		throw e;
	}
	catch (exception e)
	{
		SPDLOG_ERROR(
			"termProcess failed"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", label: {}"
			", message: {}"
			", kill: {}"
			", exception: {}",
			ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, kill, e.what()
		);

		throw e;
	}
}

void FFMPEGEncoder::sendError(FCGX_Request &request, int htmlResponseCode, string errorMessage)
{
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	FastCGIAPI::sendError(request, htmlResponseCode, JSONUtils::toString(responseBodyRoot));
}
