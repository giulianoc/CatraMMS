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
#include "Convert.h"
#include "CurlWrapper.h"
#include "CutFrameAccurate.h"
#include "Datetime.h"
#include "EncodeContent.h"
#include "Encrypt.h"
#include "FFMPEGEncoderDaemons.h"
#include "FFMpegFilters.h"
#include "GenerateFrames.h"
#include "IntroOutroOverlay.h"
#include "JSONUtils.h"
#include "LiveGrid.h"
#include "LiveProxy.h"
#include "LiveRecorder.h"
#include "MMSStorage.h"
#include "OverlayImageOnVideo.h"
#include "OverlayTextOnVideo.h"
#include "PictureInPicture.h"
#include "ProcessUtility.h"
#include "SlideShow.h"
#include "StringUtils.h"
#include "System.h"
#include "VideoSpeed.h"
#include "spdlog/fmt/bundled/format.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"
#include <fstream>

// extern char** environ;

FFMPEGEncoder::FFMPEGEncoder(

	const json& configurationRoot,
	// string encoderCapabilityConfigurationPathName,

	mutex *fcgiAcceptMutex,

	shared_mutex *cpuUsageMutex, deque<int> *cpuUsage,

	// chrono::system_clock::time_point *lastEncodingAcceptedTime,

	mutex *encodingMutex, vector<shared_ptr<FFMPEGEncoderBase::Encoding>> *encodingsCapability,

	mutex *liveProxyMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>> *liveProxiesCapability,

	mutex *liveRecordingMutex, vector<shared_ptr<FFMPEGEncoderBase::LiveRecording>> *liveRecordingsCapability,

	mutex *encodingCompletedMutex, map<int64_t, shared_ptr<FFMPEGEncoderBase::EncodingCompleted>> *encodingCompletedMap,
	chrono::system_clock::time_point *lastEncodingCompletedCheck,

	mutex *tvChannelsPortsMutex, long *tvChannelPort_CurrentOffset
)
	: FastCGIAPI(configurationRoot, fcgiAcceptMutex)
{

	loadConfiguration(configurationRoot);

	_cpuUsageMutex = cpuUsageMutex;
	_cpuUsage = cpuUsage;

	// _lastEncodingAcceptedTime = lastEncodingAcceptedTime;

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

	registerHandler<FFMPEGEncoder>("status", &FFMPEGEncoder::status);
	registerHandler<FFMPEGEncoder>("info", &FFMPEGEncoder::info);
	registerHandler<FFMPEGEncoder>("videoSpeed", &FFMPEGEncoder::videoSpeed);
	registerHandler<FFMPEGEncoder>("encodeContent", &FFMPEGEncoder::encodeContent);
	registerHandler<FFMPEGEncoder>("cutFrameAccurate", &FFMPEGEncoder::cutFrameAccurate);
	registerHandler<FFMPEGEncoder>("overlayImageOnVideo", &FFMPEGEncoder::overlayImageOnVideo);
	registerHandler<FFMPEGEncoder>("overlayTextOnVideo", &FFMPEGEncoder::overlayTextOnVideo);
	registerHandler<FFMPEGEncoder>("generateFrames", &FFMPEGEncoder::generateFrames);
	registerHandler<FFMPEGEncoder>("slideShow", &FFMPEGEncoder::slideShow);
	registerHandler<FFMPEGEncoder>("addSilentAudio", &FFMPEGEncoder::addSilentAudio);
	registerHandler<FFMPEGEncoder>("pictureInPicture", &FFMPEGEncoder::pictureInPicture);
	registerHandler<FFMPEGEncoder>("introOutroOverlay", &FFMPEGEncoder::introOutroOverlay);
	registerHandler<FFMPEGEncoder>("liveRecorder", &FFMPEGEncoder::liveRecorder);
	registerHandler<FFMPEGEncoder>("liveProxy", &FFMPEGEncoder::liveProxy);
	registerHandler<FFMPEGEncoder>("liveGrid", &FFMPEGEncoder::liveGrid);
	registerHandler<FFMPEGEncoder>("encodingStatus", &FFMPEGEncoder::encodingStatus);
	registerHandler<FFMPEGEncoder>("filterNotification", &FFMPEGEncoder::filterNotification);
	registerHandler<FFMPEGEncoder>("killEncodingJob", &FFMPEGEncoder::killEncodingJob);
	registerHandler<FFMPEGEncoder>("changeLiveProxyPlaylist", &FFMPEGEncoder::changeLiveProxyPlaylist);
	registerHandler<FFMPEGEncoder>("changeLiveProxyOverlayText", &FFMPEGEncoder::changeLiveProxyOverlayText);
	registerHandler<FFMPEGEncoder>("encodingProgress", &FFMPEGEncoder::encodingProgress);
}

FFMPEGEncoder::~FFMPEGEncoder() = default;

// 2020-06-11: FFMPEGEncoder is just one thread, so make sure
// manageRequestAndResponse is very fast because
//	the time used by manageRequestAndResponse is time FFMPEGEncoder is not
// listening 	for new connections (encodingStatus, ...)
// 2023-10-17: non penso che il commento sopra sia vero, FFMPEGEncoder non è un
// solo thread!!!

void FFMPEGEncoder::manageRequestAndResponse(
	const string_view& sThreadId, int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI, const string_view& requestMethod,
	const string_view& requestBody, bool responseBodyCompressed, unsigned long contentLength,
	const unordered_map<string, string> &requestDetails, const unordered_map<string, string>& queryParameters
)
{
	if (chrono::system_clock::now() - *_lastEncodingCompletedCheck >= chrono::seconds(_encodingCompletedRetentionInSeconds))
	{
		*_lastEncodingCompletedCheck = chrono::system_clock::now();
		// gli encoding completati vengono eliminati dalla mappa _encodingCompletedMap dopo _encodingCompletedRetentionInSeconds
		encodingCompletedRetention();
	}

	try
	{
		handleRequest(sThreadId, requestIdentifier, request, authorizationDetails, requestURI, requestMethod, requestBody,
			responseBodyCompressed, requestDetails, queryParameters, true);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"manage request failed"
			", requestBody: {}"
			", e.what(): {}",
			requestBody, e.what()
		);

		int htmlResponseCode = 500;
		string errorMessage;
		if (dynamic_cast<HTTPError*>(&e))
		{
			htmlResponseCode = dynamic_cast<HTTPError*>(&e)->httpErrorCode;
			errorMessage = e.what();
		}
		else
			errorMessage = getHtmlStandardMessage(htmlResponseCode);
		SPDLOG_ERROR(errorMessage);

		sendError(request, htmlResponseCode, errorMessage);

		throw;
	}
}

void FFMPEGEncoder::status(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "status";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		json responseBodyRoot;
		responseBodyRoot["status"] = "Encoder up and running";

		string responseBody = JSONUtils::toString(responseBodyRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(500);
	}
}

void FFMPEGEncoder::info(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "info";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int lastBiggerCpuUsage = -1;
		{
			shared_lock locker(*_cpuUsageMutex);

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
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(500);
	}
}

void FFMPEGEncoder::videoSpeed(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "videoSpeed";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::encodeContent(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "encodeContent";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::cutFrameAccurate(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "cutFrameAccurate";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::overlayImageOnVideo(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "overlayImageOnVideo";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::overlayTextOnVideo(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "overlayTextOnVideo";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::generateFrames(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "generateFrames";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::slideShow(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "slideShow";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::addSilentAudio(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "addSilentAudio";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::pictureInPicture(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "pictureInPicture";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::introOutroOverlay(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "introOutroOverlay";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	requestManagement(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::requestManagement(
	const string_view& method, const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	int64_t ingestionJobKey = getMapParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
	int64_t encodingJobKey = getMapParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

	{
		lock_guard<mutex> locker(*_encodingMutex);

		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;
		bool freeEncodingFound = false;
		bool encodingAlreadyRunning = false;
		int maxEncodingsCapability = getMaxEncodingsCapability();
		// Ci sono dei casi in cui non bisogna accettare ulteriori encoding anche se la CPU è bassa:
		//	- è arrivato un encoding su un externalEncoding che impiega 1h per scaricare i contenuti da encodare.
		//		Se accettassimo ulteriori encoding, tutti i download rimarrebbero bloccati.
		//	- arriva un encoding su un encoder interno, bisogna aspettare che la variabile
		//		cpuUsage si aggiorni prima di accettare un nuovo encoding
		// Per questo motivo sono stati introdotti atLeastThisEncodingJobKeyNotStartedYet (cioè ancora in downloading) e lastEncodingStart
		// Se inizializziamo atLeastThisEncodingJobKeyNotStartedYet (cioè ancora in downloading), lastEncodingStart è inutile perchè comunque
		// non possiamo accettare altri encoding
		optional<int64_t> atLeastThisEncodingJobKeyNotStartedYet;
		optional<chrono::system_clock::time_point> lastEncodingStart;
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

				if (!atLeastThisEncodingJobKeyNotStartedYet)
				{
					// fino ad ora gli encoding sono tutti partiti (cioè non ancora in downloading)
					if (encoding->_encodingStart)
					{
						if (!lastEncodingStart)
							lastEncodingStart = encoding->_encodingStart;
						else if (lastEncodingStart && *encoding->_encodingStart > *lastEncodingStart)
							lastEncodingStart = encoding->_encodingStart;
					}
					else
						atLeastThisEncodingJobKeyNotStartedYet = encoding->_encodingJobKey;
				}
			}
		}
		if (encodingAlreadyRunning || !freeEncodingFound)
		{
			string errorMessage;
			if (encodingAlreadyRunning)
				errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, EncodingIsAlreadyRunning().what());
			else if (maxEncodingsCapability == 0)
				errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, MaxConcurrentJobsReached().what());
			else
				errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingAvailable().what());

			SPDLOG_ERROR(
				"{}"
				", encodingAlreadyRunning: {}"
				", freeEncodingFound: {}",
				errorMessage, encodingAlreadyRunning, freeEncodingFound
			);

			throw HTTPError(400, errorMessage);
		}

		try
		{
			if (atLeastThisEncodingJobKeyNotStartedYet)
			{
				string errorMessage = std::format("Too early to accept a new encoding request, one is still in downloading"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", atLeastThisEncodingJobKeyNotStartedYet: {}"
					", {}", ingestionJobKey, encodingJobKey, *atLeastThisEncodingJobKeyNotStartedYet, NoEncodingAvailable().what());
				SPDLOG_WARN(errorMessage);

				throw HTTPError(400, errorMessage);
			}
			if (lastEncodingStart)
			{
				int elapsedSecondsSinceLastEncodingAccepted = chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - *lastEncodingStart).count();
				if (elapsedSecondsSinceLastEncodingAccepted < _intervalInSecondsBetweenEncodingAccept)
				{
					int secondsToWait = _intervalInSecondsBetweenEncodingAccept - elapsedSecondsSinceLastEncodingAccepted;
					string errorMessage = std::format("Too early to accept a new encoding request, waiting cpuUsage to be updated"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", elapsedSecondsSinceLastEncodingAccepted: {}"
						", intervalInSecondsBetweenEncodingAccept: {}"
						", secondsToWait: {}"
						", {}", ingestionJobKey, encodingJobKey, elapsedSecondsSinceLastEncodingAccepted,
							_intervalInSecondsBetweenEncodingAccept, secondsToWait, NoEncodingAvailable().what());
					SPDLOG_WARN(errorMessage);

					throw HTTPError(400, errorMessage);
				}
			}

			json metadataRoot = JSONUtils::toJson(requestBody);
			/*
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
				string errorMessage = std::format("Too early to accept a new encoding request"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", elapsedSecondsSinceLastEncodingAccepted: {}"
					", intervalInSecondsBetweenEncodingAccept: {}"
					", secondsToWait: {}"
					", {}", ingestionJobKey, encodingJobKey, elapsedSecondsSinceLastEncodingAccepted,
						intervalInSecondsBetweenEncodingAccept, secondsToWait, NoEncodingAvailable().what());

				SPDLOG_WARN(errorMessage);

				throw HTTPError(400, errorMessage);
			}
			*/

			SPDLOG_INFO(
				"Accept a new encoding request"
				", ingestionJobKey: {}"
				", encodingJobKey: {}",
				ingestionJobKey, encodingJobKey
			);

			// 2023-06-15: scenario: tanti encoding stanno aspettando di essere gestiti.
			//	L'encoder finisce il task in corso, tutti gli encoding in attesa verificano che la CPU è bassa e,
			// tutti entrano per essere gestiti. Per risolvere questo problema, è necessario aggiornare _lastEncodingAcceptedTime
			// as soon as possible, altrimenti tutti quelli in coda entrano per essere gestiti
			// *_lastEncodingAcceptedTime = chrono::system_clock::now();

			selectedEncoding->initEncoding(encodingJobKey, method);

			SPDLOG_INFO(
				"Creating {} thread"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", requestBody: {}",
				method, ingestionJobKey, encodingJobKey, requestBody
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
				selectedEncoding->reset();

				string errorMessage = std::format(
					"wrong method"
					// ", ingestionJobKey: {}" aggiunto nel catch
					// ", encodingJobKey: {}"
					", method: {}",
					method
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}
		catch (exception &e)
		{
			selectedEncoding->reset();

			string errorMessage = std::format(
				"{} failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				// ", requestBody: {}"	aggiunto nel catch del chiamante
				", e.what(): {}",
				method, ingestionJobKey, requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw;
		}
	}

	{
		json responseBodyRoot;
		responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
		responseBodyRoot["encodingJobKey"] = encodingJobKey;
		responseBodyRoot["ffmpegEncoderHost"] = System::hostName();

		string responseBody = JSONUtils::toString(responseBodyRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
}

void FFMPEGEncoder::liveRecorder(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "liveRecorder";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t ingestionJobKey = getMapParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		int64_t encodingJobKey = getMapParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

		{
			lock_guard<mutex> locker(*_liveRecordingMutex);

			shared_ptr<FFMPEGEncoderBase::LiveRecording> selectedLiveRecording;
			bool freeEncodingFound = false;
			bool encodingAlreadyRunning = false;
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
					errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, EncodingIsAlreadyRunning().what());
				else
					errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingAvailable().what());
				SPDLOG_ERROR(
					"{}"
					", encodingAlreadyRunning: {}"
					", freeEncodingFound: {}",
					errorMessage, encodingAlreadyRunning, freeEncodingFound
				);

				throw HTTPError(400, errorMessage);
			}

			try
			{
				/*
				 * 2021-09-15: live-recorder cannot wait. Scenario: received a lot of requests that fail
				 * Those requests set _lastEncodingAcceptedTime and delay a lot the requests that would work fine
				 * Consider that Live-Recorder is a Task where FFMPEGEncoder could receive a lot of close requests
				lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
				// Make some time after the acception of the previous encoding request in order to give time to the cpuUsage
				// variable to be correctly updated
				chrono::system_clock::time_point now = chrono::system_clock::now();
				if (now - *_lastEncodingAcceptedTime < chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
				{
					string errorMessage = string("Too early to accept a new encoding request")
						+ ", seconds since the last request: " + to_string(chrono::duration_cast<chrono::seconds>(
							now - *_lastEncodingAcceptedTime).count())
						+ ", " + NoEncodingAvailable().what();
					warn(__FILEREF__ + errorMessage);

					sendError(request, 400, errorMessage);

					// throw runtime_error(noEncodingAvailableMessage);
					return;
				}
				*/

				// 2022-11-23: ho visto che, in caso di autoRenew, monitoring generates errors and trys to kill the process.
				// Moreover the selectedLiveRecording->_errorMessage remain initialized with the error (like killed
				// because segment file is not present).
				// For this reason, _recordingStart is initialized to make sure monitoring does not perform his checks before recorder is not
				// really started. _recordingStart will be initialized correctly into the liveRecorderThread method
				selectedLiveRecording->initEncoding(encodingJobKey, api);
				selectedLiveRecording->_encodingStart = chrono::system_clock::now() + chrono::seconds(60);

				SPDLOG_INFO(
					"Creating liveRecorder thread"
					", ingestionJobKey: {}"
					", selectedLiveRecording->_encodingJobKey: {}"
					", requestBody: {}",
					ingestionJobKey, encodingJobKey, requestBody
				);
				thread liveRecorderThread(
					&FFMPEGEncoder::liveRecorderThread, this, selectedLiveRecording, ingestionJobKey, encodingJobKey,
					string(requestBody)
				);
				liveRecorderThread.detach();

				// *_lastEncodingAcceptedTime =
				// chrono::system_clock::now();
			}
			catch (exception &e)
			{
				selectedLiveRecording->reset();

				string errorMessage = std::format(
					"liveRecorder failed"
					", ingestionJobKey: {}"
					", selectedLiveRecording->_encodingJobKey: {}"
					", requestBody: {}"
					", e.what(): {}",
					ingestionJobKey, encodingJobKey, requestBody, e.what()
				);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		{
			json responseBodyRoot;
			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["ffmpegEncoderHost"] = System::hostName();

			string responseBody = JSONUtils::toString(responseBodyRoot);

			sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
		}
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void FFMPEGEncoder::liveProxy(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "liveProxy";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	liveProxy_liveGrid(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::liveGrid(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "liveGrid";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	liveProxy_liveGrid(api,
		sThreadId, requestIdentifier, request, authorizationDetails, requestURI,
		requestMethod, requestBody, responseBodyCompressed,
		requestDetails, queryParameters
	);
}

void FFMPEGEncoder::liveProxy_liveGrid(
	const string_view& method, const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	int64_t ingestionJobKey = getMapParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
	int64_t encodingJobKey = getMapParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

	{
		lock_guard<mutex> locker(*_liveProxyMutex);

		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;
		bool freeEncodingFound = false;
		bool encodingAlreadyRunning = false;
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
				errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, EncodingIsAlreadyRunning().what());
			else
				errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingAvailable().what());
			SPDLOG_ERROR(
				"{}"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodingAlreadyRunning: {}"
				", freeEncodingFound: {}",
				errorMessage, ingestionJobKey, encodingJobKey, encodingAlreadyRunning, freeEncodingFound
			);

			throw HTTPError(400, errorMessage);
		}

		try
		{
			/*
			 * 2021-09-15: liveProxy cannot wait. Scenario: received a lot of requests that fail Those requests set _lastEncodingAcceptedTime
			and delay a lot the requests that would work fine Consider that Live-Proxy is a Task where FFMPEGEncoder
			 * could receive a lot of close requests
			lock_guard<mutex> locker(*_lastEncodingAcceptedTimeMutex);
			// Make some time after the acception of the previous encoding request
			// in order to give time to the cpuUsage variable to be correctly updated
			chrono::system_clock::time_point now = chrono::system_clock::now();
			if (now - *_lastEncodingAcceptedTime < chrono::seconds(_intervalInSecondsBetweenEncodingAccept))
			{
				int secondsToWait = chrono::seconds(_intervalInSecondsBetweenEncodingAccept).count() -
					chrono::duration_cast<chrono::seconds>(now - *_lastEncodingAcceptedTime).count();
				string errorMessage = string("Too early to accept a new encoding request")
						+ ", encodingJobKey: " + to_string(encodingJobKey)
						+ ", seconds since the last request: " + to_string(chrono::duration_cast<chrono::seconds>(now -
							*_lastEncodingAcceptedTime).count())
						+ ", secondsToWait: " + to_string(secondsToWait)
						+ ", " + NoEncodingAvailable().what();
				warn(__FILEREF__ + errorMessage);

				sendError(request, 400, errorMessage);

				// throw runtime_error(noEncodingAvailableMessage);
				return;
			}
			*/

			selectedLiveProxy->initEncoding(encodingJobKey, method);

			if (method == "liveProxy")
			{
				SPDLOG_INFO(
					"Creating liveProxy thread"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", requestBody: {}",
					ingestionJobKey, encodingJobKey, requestBody
				);
				thread liveProxyThread(&FFMPEGEncoder::liveProxyThread, this, selectedLiveProxy, ingestionJobKey, encodingJobKey,
					string(requestBody));
				liveProxyThread.detach();
			}
			else if (method == "liveGrid")
			{
				SPDLOG_INFO(
					"Creating liveGrid thread"
					", ingestionJobKey: {}"
					", encodingJobKey: {}"
					", requestBody: {}",
					ingestionJobKey, encodingJobKey, requestBody
				);
				thread liveGridThread(&FFMPEGEncoder::liveGridThread, this, selectedLiveProxy, ingestionJobKey, encodingJobKey,
					string(requestBody));
				liveGridThread.detach();
			}

			// *_lastEncodingAcceptedTime =
			// chrono::system_clock::now();
		}
		catch (exception &e)
		{
			selectedLiveProxy->reset();

			string errorMessage = std::format(
				"liveProxyThread failed"
				", method: {}"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", requestBody: {}"
				", e.what(): {}",
				method, ingestionJobKey, encodingJobKey, requestBody, e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
	}

	{
		json responseBodyRoot;
		responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
		responseBodyRoot["encodingJobKey"] = encodingJobKey;
		responseBodyRoot["ffmpegEncoderHost"] = System::hostName();

		string responseBody = JSONUtils::toString(responseBodyRoot);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
}

void FFMPEGEncoder::encodingStatus(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "encodingStatus";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t ingestionJobKey = getQueryParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

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

			auto it = _encodingCompletedMap->find(encodingJobKey);
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
				for (const shared_ptr<FFMPEGEncoderBase::Encoding>& encoding : *_encodingsCapability)
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
					 * 2020-11-30 CIBORTV PROJECT. SCENARIO: - The encodingStatus is called
					 *by the mmsEngine periodically for each running transcoding. Often this method
					 *takes a lot of times to answer, depend on the period encodingStatus is
					 *called, 50 secs in case it is called every 5 seconds, 35 secs in case it is
					 *called every 30 secs. This because the Lock (lock_guard) does not provide any
					 *guarantee, in case there are a lot of threads, as it is our case, may be a
					 *thread takes the lock and the OS switches to another thread. It could
					 *take time the OS re-switch on the previous thread in order to release
					 *the lock.
					 *
					 *	To solve this issue we should found an algorithm that guarantees the
					 *Lock is managed in a fast way also in case of a lot of threads. I do not
					 *have now a solution for this. For this since I thought:
					 *	- in case of __VECTOR__ all the structure is "fixes", every thing is
					 *allocated at the beggining and do not change
					 *	- so for this method, since it checks some attribute in a "static"
					 *structure, WE MAY AVOID THE USING OF THE LOCK
					 *
					 */
					for (const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
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
					for (const shared_ptr<FFMPEGEncoderBase::LiveRecording>& liveRecording : *_liveRecordingsCapability)
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

		string sChildProcessId = "not available";
		if (encodingFound)
			sChildProcessId = selectedEncoding->_childProcessId.toString();
		else if (liveProxyFound)
			sChildProcessId = selectedLiveProxy->_childProcessId.toString();
		else if (liveRecordingFound)
			sChildProcessId = selectedLiveRecording->_childProcessId.toString();
		SPDLOG_INFO(
			"encodingStatus"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", encodingFound: {}"
			", encodingAvailable: {}"
			", liveProxyFound: {}"
			", liveRecordingFound: {}"
			", childPid: {}"
			", encodingCompleted: {}"
			", encodingCompletedMutexDuration: {}"
			", encodingMutexDuration: {}"
			", liveProxyMutexDuration: {}"
			", liveRecordingMutexDuration: {}"
			", @MMS statistics@ - duration looking for encodingStatus (secs): @{}@",
			ingestionJobKey, encodingJobKey, encodingFound, encodingFound ? to_string(selectedEncoding->_available) : "not available", liveProxyFound,
			liveRecordingFound, sChildProcessId, encodingCompleted, encodingCompletedMutexDuration, encodingMutexDuration, liveProxyMutexDuration,
			liveRecordingMutexDuration, chrono::duration_cast<chrono::seconds>(endLookingForEncodingStatus - startEncodingStatus).count()
		);

		string responseBody;
		if (!encodingFound && !liveProxyFound && !liveRecordingFound && !encodingCompleted)
		{
			// it should never happen
			json responseBodyRoot;

			responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
			responseBodyRoot["encodingJobKey"] = encodingJobKey;
			responseBodyRoot["pid"] = 0;
			responseBodyRoot["killedByUser"] = false;
			responseBodyRoot["encodingFinished"] = true;
			responseBodyRoot["encodingProgress"] = 100.0;
			responseBodyRoot["progress"] = nullptr;

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
				responseBodyRoot["killedByUser"] =
					selectedEncodingCompleted->_killedByUser && selectedEncodingCompleted->_killToRestartByEngine
					? false
					: selectedEncodingCompleted->_killedByUser;
				responseBodyRoot["completedWithError"] = selectedEncodingCompleted->_completedWithError;
				// responseBodyRoot["errorMessage"] = selectedEncodingCompleted->_errorMessage;
				responseBodyRoot["encodingFinished"] = true;
				responseBodyRoot["encodingProgress"] = 100.0;
				responseBodyRoot["data"] = selectedEncodingCompleted->_callbackData->toJson();

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

						optional<double> localEncodingProgress = selectedEncoding->_callbackData->getProgressPercent();
						if (localEncodingProgress)
							encodingProgress = *localEncodingProgress;

						chrono::system_clock::time_point endEncodingProgress = chrono::system_clock::now();
						SPDLOG_INFO(
							"getEncodingProgress statistics"
							", @MMS statistics@ - encodingProgress (secs): @{}@",
							chrono::duration_cast<chrono::seconds>(endEncodingProgress - startEncodingProgress).count()
						);
					}
					catch (exception &e)
					{
						SPDLOG_ERROR(
							"_ffmpeg->getEncodingProgress failed"
							", ingestionJobKey: {}"
							", encodingJobKey: {}"
							", e.what(): {}",
							ingestionJobKey, encodingJobKey, e.what()
						);
					}
				}

				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedEncoding->_encodingJobKey;
				responseBodyRoot["pid"] = selectedEncoding->_childProcessId.pid;
				responseBodyRoot["killedByUser"] = false;
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
				responseBodyRoot["data"] = selectedEncoding->_callbackData->toJson();

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else if (liveProxyFound)
			{
				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = selectedLiveProxy->_ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedLiveProxy->_encodingJobKey;
				responseBodyRoot["pid"] = selectedLiveProxy->_childProcessId.pid;
				responseBodyRoot["numberOfRestartBecauseOfFailure"] = selectedLiveProxy->_numberOfRestartBecauseOfFailure;
				responseBodyRoot["killedByUser"] = false;
				responseBodyRoot["encodingFinished"] = encodingCompleted;

				// 2020-06-11: it's a live, it does not have
				// sense the encoding progress
				responseBodyRoot["encodingProgress"] = nullptr;
				responseBodyRoot["data"] = selectedLiveProxy->_callbackData->toJson();

				responseBody = JSONUtils::toString(responseBodyRoot);
			}
			else // if (liveRecording)
			{
				json responseBodyRoot;

				responseBodyRoot["ingestionJobKey"] = selectedLiveRecording->_ingestionJobKey;
				responseBodyRoot["encodingJobKey"] = selectedLiveRecording->_encodingJobKey;
				responseBodyRoot["pid"] = selectedLiveRecording->_childProcessId.pid;
				responseBodyRoot["numberOfRestartBecauseOfFailure"] = selectedLiveRecording->_numberOfRestartBecauseOfFailure;
				responseBodyRoot["killedByUser"] = false;
				responseBodyRoot["encodingFinished"] = encodingCompleted;

				// 2020-10-13: we do not have here the
				// information to calculate the encoding
				// progress,
				//	it is calculated in
				// EncoderVideoAudioProxy.cpp
				responseBodyRoot["encodingProgress"] = nullptr;
				responseBodyRoot["data"] = selectedLiveRecording->_callbackData->toJson();

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
			ingestionJobKey, encodingJobKey, encodingFound, liveProxyFound, liveRecordingFound, encodingCompleted,
			responseBody, chrono::duration_cast<chrono::seconds>(endEncodingStatus - startEncodingStatus).count()
		);

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(500);
	}
}

void FFMPEGEncoder::filterNotification(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "filterNotification";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t ingestionJobKey = getQueryParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);
		string filterName = getQueryParameter(queryParameters, "filterName", string(), true);

		bool encodingCompleted = false;
		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			auto it = _encodingCompletedMap->find(encodingJobKey);
			if (it != _encodingCompletedMap->end())
				encodingCompleted = true;
		}

		if (encodingCompleted)
		{
			string errorMessage = std::format(
				"filterNotification, encoding is already finished"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", encodingCompleted: {}",
				ingestionJobKey, ingestionJobKey, encodingCompleted
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		bool liveProxyFound = false;
		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

		for (const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
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
			string errorMessage = std::format(
				"filterNotification, liveProxy not found"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", liveProxyFound: {}",
				ingestionJobKey, ingestionJobKey, liveProxyFound
			);
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}

		string mmsWorkflowIngestionURL;
		string workflowMetadata;
		try
		{
			json ingestedParametersRoot = selectedLiveProxy->_ingestedParametersRoot;
			json encodingParametersRoot = selectedLiveProxy->_encodingParametersRoot;

			workflowMetadata = buildFilterNotificationIngestionWorkflow(ingestionJobKey, filterName, ingestedParametersRoot);
			if (!workflowMetadata.empty())
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
						string errorMessage = std::format(
							"Field is not present or it is null"
							", _ingestionJobKey: {}"
							", Field: {}",
							ingestionJobKey, field
						);
						SPDLOG_ERROR(errorMessage);

						throw runtime_error(errorMessage);
					}
					mmsWorkflowIngestionURL = JSONUtils::asString(encodingParametersRoot, field, "");
				}

				vector<string> otherHeaders;
				string sResponse = CurlWrapper::httpPostString(
				   mmsWorkflowIngestionURL, _mmsAPITimeoutInSeconds, CurlWrapper::basicAuthorization(to_string(userKey), apiKey),
				   workflowMetadata,
				   "application/json", // contentType
				   otherHeaders, std::format(", ingestionJobKey: {}", ingestionJobKey),
				   3 // maxRetryNumber
				).second;
			}
		}
		catch (exception& e)
		{
			string errorMessage = std::format(
				"Ingested URL failed (runtime_error)"
				", ingestionJobKey: {}"
				", mmsWorkflowIngestionURL: {}"
				", workflowMetadata: {}"
				", exception: {}",
				ingestionJobKey, mmsWorkflowIngestionURL, workflowMetadata, e.what()
			);
			SPDLOG_ERROR(errorMessage);

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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(500);
	}
}

void FFMPEGEncoder::killEncodingJob(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "killEncodingJob";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t ingestionJobKey = getQueryParameter(queryParameters, "ingestionJobKey", static_cast<int64_t>(-1), true);
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

		// killType: "kill", "restartWithinEncoder", "killToRestartByEngine"
		string killType = getQueryParameter(queryParameters, "killType", string("kill"), false);

		SPDLOG_INFO(
			"Received killEncodingJob"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", killType: {}",
			ingestionJobKey, encodingJobKey, killType
		);

		// pid_t pidToBeKilled;
		bool encodingFound = false;
		bool liveProxyFound = false;
		bool liveRecorderFound = false;
		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;

		{
			for (const shared_ptr<FFMPEGEncoderBase::Encoding>& encoding : *_encodingsCapability)
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
			for (const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
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
			for (const shared_ptr<FFMPEGEncoderBase::LiveRecording>& liveRecording : *_liveRecordingsCapability)
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
			string errorMessage = std::format(
				"ingestionJobKey: {}"
				", encodingJobKey: {}"
				", {}",
				ingestionJobKey, encodingJobKey, NoEncodingJobKeyFound().what()
			);
			SPDLOG_ERROR(errorMessage);
			throw HTTPError(400, errorMessage);
		}

		SPDLOG_INFO(
			"ProcessUtility::killProcess. Found Encoding to kill"
			", ingestionJobKey: {}"
			", encodingJobKey: {}"
			", _childProcessId: {}"
			", killType: {}",
			ingestionJobKey, encodingJobKey, selectedEncoding->_childProcessId.toString(), killType
		);

		// if (pidToBeKilled == 0)
		if (!selectedEncoding->_childProcessId.isInitialized())
		{
			string errorMessage = std::format(
				"The EncodingJob seems not running (see _childProcessId)"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _childProcessId: {}",
				ingestionJobKey, encodingJobKey, selectedEncoding->_childProcessId.toString()
			);
			SPDLOG_ERROR(errorMessage);

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
				FFMPEGEncoderDaemons::termProcess(selectedEncoding, encodingJobKey, "unknown", "received killEncodingJob", false);
			}
			else
			{
				if (killType == "killToRestartByEngine")
					selectedEncoding->_killToRestartByEngine = true;
				FFMPEGEncoderDaemons::termProcess(selectedEncoding, encodingJobKey, "unknown", "received killEncodingJob", true);
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
			string errorMessage = std::format(
				"ProcessUtility::killProcess failed"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", _childProcessId: {}"
				", e.what(): {}",
				ingestionJobKey, encodingJobKey, selectedEncoding->_childProcessId.toString(), e.what()
			);
			SPDLOG_ERROR(errorMessage);

			throw;
		}

		string responseBody;
		{
			json responseBodyRoot;

			string field = "ingestionJobKey";
			responseBodyRoot[field] = ingestionJobKey;

			field = "encodingJobKey";
			responseBodyRoot[field] = encodingJobKey;

			field = "pid";
			responseBodyRoot[field] = selectedEncoding->_childProcessId.pid;

			responseBody = JSONUtils::toString(responseBodyRoot);
		}

		sendSuccess(sThreadId, requestIdentifier, responseBodyCompressed, request, requestURI, requestMethod, 200, responseBody);
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void FFMPEGEncoder::changeLiveProxyPlaylist(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "changeLiveProxyPlaylist";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);
		bool interruptPlaylist = getQueryParameter(queryParameters, "interruptPlaylist", false, false);

		SPDLOG_INFO(
			"Received changeLiveProxyPlaylist"
			", encodingJobKey: {}",
			encodingJobKey
		);

		json newInputsRoot = JSONUtils::toJson(requestBody);

		{
			bool encodingFound = false;
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

			for (const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
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
				string errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingJobKeyFound().what());
				SPDLOG_ERROR(errorMessage);

				throw HTTPError(500, errorMessage);
			}

			{
				SPDLOG_INFO(
					"Replacing the LiveProxy playlist"
					", ingestionJobKey: {}"
					", encodingJobKey: {}",
					selectedLiveProxy->_ingestionJobKey, encodingJobKey
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
					FFMPEGEncoderDaemons::termProcess(selectedLiveProxy, selectedLiveProxy->_ingestionJobKey, "unknown", "received changeLiveProxyPlaylist", false);
				}
				catch (runtime_error &e)
				{
					string errorMessage = std::format(
						"ProcessUtility::termProcess failed"
						", ingestionJobKey: {}"
						", encodingJobKey: {}"
						", _childProcessId: {}"
						", e.what(): {}",
						selectedLiveProxy->_ingestionJobKey, selectedLiveProxy->_encodingJobKey, selectedLiveProxy->_childProcessId.toString(),
						e.what()
					);
					SPDLOG_ERROR(errorMessage);
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void FFMPEGEncoder::changeLiveProxyOverlayText(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
)
{
	string api = "changeLiveProxyOverlayText";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
	{
		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

		string newOverlayText(requestBody);

		{
			bool encodingFound = false;
			lock_guard<mutex> locker(*_liveProxyMutex);

			shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;

			for (const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid>& liveProxy : *_liveProxiesCapability)
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
				string errorMessage = std::format("EncodingJobKey: {}, {}", encodingJobKey, NoEncodingJobKeyFound().what());
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}

			SPDLOG_INFO(
				"Received changeLiveProxyOverlayText"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", requestBody: {}",
				selectedLiveProxy->_ingestionJobKey, encodingJobKey, requestBody
			);

			string textTemporaryFileName = FFMpegFilters::getDrawTextTemporaryPathName(
				selectedLiveProxy->_ffmpeg->_ffmpegTempDir, selectedLiveProxy->_ingestionJobKey, encodingJobKey, 0
			);
			// selectedLiveProxy->_ffmpeg->getDrawTextTemporaryPathName(selectedLiveProxy->_ingestionJobKey, encodingJobKey, 0);
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
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw;
	}
}

void FFMPEGEncoder::encodingProgress(
	const string_view& sThreadId, const int64_t requestIdentifier, FCGX_Request &request,
	const shared_ptr<AuthorizationDetails>& authorizationDetails, const string_view& requestURI,
	const string_view& requestMethod, const string_view& requestBody, const bool responseBodyCompressed,
	const unordered_map<string, string>& requestDetails,
	const unordered_map<string, string>& queryParameters
) const
{
	string api = "encodingProgress";

	SPDLOG_INFO(
		"Received {}"
		", requestBody: {}",
		api, requestBody
	);

	try
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
			SPDLOG_ERROR(errorMessage);

			sendError(request, 403, errorMessage);

			throw runtime_error(errorMessage);
		}
		*/

		int64_t encodingJobKey = getQueryParameter(queryParameters, "encodingJobKey", static_cast<int64_t>(-1), true);

		bool encodingCompleted = false;
		shared_ptr<FFMPEGEncoderBase::EncodingCompleted> selectedEncodingCompleted;

		shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding;
		bool encodingFound = false;

		shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> selectedLiveProxy;
		bool liveProxyFound = false;

		{
			lock_guard<mutex> locker(*_encodingCompletedMutex);

			auto it = _encodingCompletedMap->find(encodingJobKey);
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

	SPDLOG_ERROR(errorMessage);

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
						info(__FILEREF__ +
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
						error(__FILEREF__ +
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
						info(__FILEREF__ +
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
						error(__FILEREF__ +
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
				info(__FILEREF__ + errorMessage);

				sendError(request, 500, errorMessage);

				// throw e;
				return;
		}
		*/
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(
			"API failed"
			", API: {}"
			", requestBody: {}"
			", e.what(): {}",
			api, requestBody, e.what()
		);

		throw HTTPError(500);
	}
}

shared_ptr<FastCGIAPI::AuthorizationDetails> FFMPEGEncoder::checkAuthorization(const string_view& sThreadId, const string_view& userName, const string_view& password)
{
	if (userName != _encoderUser || password != _encoderPassword)
	{
		SPDLOG_ERROR(
			"Username/password of the basic authorization are wrong"
			", _requestIdentifier: {}"
			", threadId: {}"
			", userName: {}"
			", _encoderUser: {}"
			", password: {}"
			", _encoderPassword: {}",
			_requestIdentifier, sThreadId, userName, _encoderUser, password, _encoderPassword
		);

		throw HTTPError(401);
	}

	auto authorizationDetails = make_shared<AuthorizationDetails>();
	authorizationDetails->userName = userName;
	authorizationDetails->password = password;

	return authorizationDetails;
}

bool FFMPEGEncoder::basicAuthenticationRequired(const string &requestURI, const unordered_map<string, string> &queryParameters)
{
	bool basicAuthenticationRequired = true;

	const string method = getQueryParameter(queryParameters, "x-api-method", string(), false);
	if (method.empty())
	{
		SPDLOG_ERROR("The 'x-api-method' parameter is not found");

		return basicAuthenticationRequired;
	}

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
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		EncodeContent encodeContent(encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		encodeContent.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::overlayImageOnVideoThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		OverlayImageOnVideo overlayImageOnVideo(
			encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap
		);
		overlayImageOnVideo.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::overlayTextOnVideoThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		OverlayTextOnVideo overlayTextOnVideo(
			encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap
		);
		overlayTextOnVideo.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::generateFramesThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		GenerateFrames generateFrames(encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		generateFrames.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();
	}
}

void FFMPEGEncoder::slideShowThread(
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		SlideShow slideShow(encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		slideShow.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();
	}
}

void FFMPEGEncoder::videoSpeedThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		VideoSpeed videoSpeed(encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		videoSpeed.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::addSilentAudioThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		AddSilentAudio addSilentAudio(encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		addSilentAudio.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::pictureInPictureThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		PictureInPicture pictureInPicture(
			encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap
		);
		pictureInPicture.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::introOutroOverlayThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		IntroOutroOverlay introOutroOverlay(
			encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap
		);
		introOutroOverlay.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();
		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::cutFrameAccurateThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::Encoding> &encoding, const int64_t ingestionJobKey, const int64_t encodingJobKey, const json &metadataRoot
) const
{
	try
	{
		encoding->_ingestionJobKey = ingestionJobKey;
		encoding->_encodingJobKey = encodingJobKey;
		CutFrameAccurate cutFrameAccurate(
			encoding, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap
		);
		cutFrameAccurate.encodeContent(metadataRoot);
		encoding->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		encoding->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveRecorderThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::LiveRecording> &liveRecording, const int64_t ingestionJobKey, const int64_t encodingJobKey,
	const string requestBody
) const
{
	try
	{
		liveRecording->_ingestionJobKey = ingestionJobKey;
		liveRecording->_encodingJobKey = encodingJobKey;
		LiveRecorder liveRecorder(
			liveRecording, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _tvChannelsPortsMutex,
			_tvChannelPort_CurrentOffset
		);
		liveRecorder.encodeContent(requestBody);
		liveRecording->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		liveRecording->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveProxyThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, const int64_t ingestionJobKey, const int64_t encodingJobKey,
	const string requestBody
) const
{
	try
	{
		liveProxyData->_ingestionJobKey = ingestionJobKey;
		liveProxyData->_encodingJobKey = encodingJobKey;
		LiveProxy liveProxy(
			liveProxyData, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap, _tvChannelsPortsMutex,
			_tvChannelPort_CurrentOffset
		);
		liveProxy.encodeContent(requestBody);
		liveProxyData->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		liveProxyData->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::liveGridThread(
	// FCGX_Request& request,
	const shared_ptr<FFMPEGEncoderBase::LiveProxyAndGrid> &liveProxyData, const int64_t ingestionJobKey, const int64_t encodingJobKey,
	const string requestBody
) const
{
	try
	{
		liveProxyData->_ingestionJobKey = ingestionJobKey;
		liveProxyData->_encodingJobKey = encodingJobKey;
		LiveGrid liveGrid(liveProxyData, _configurationRoot, _encodingCompletedMutex, _encodingCompletedMap);
		liveGrid.encodeContent(requestBody);
		liveProxyData->reset();
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(e.what());

		liveProxyData->reset();

		// this method run on a detached thread, we will not generate
		// exception The ffmpeg method will make sure the encoded file
		// is removed (this is checked in EncoderVideoAudioProxy) throw
		// runtime_error(errorMessage);
	}
}

void FFMPEGEncoder::encodingCompletedRetention() const
{

	lock_guard<mutex> locker(*_encodingCompletedMutex);

	const chrono::system_clock::time_point start = chrono::system_clock::now();

	for (auto it = _encodingCompletedMap->begin(); it != _encodingCompletedMap->end();)
	{
		if (start - (it->second->_timestamp) >= chrono::seconds(_encodingCompletedRetentionInSeconds))
			it = _encodingCompletedMap->erase(it);
		else
			++it;
	}

	const chrono::system_clock::time_point end = chrono::system_clock::now();

	SPDLOG_INFO(
		"encodingCompletedRetention"
		", encodingCompletedMap size: {}"
		", @MMS statistics@ - duration encodingCompleted retention processing (secs): @{}@",
		_encodingCompletedMap->size(), chrono::duration_cast<chrono::seconds>(end - start).count()
	);
}

int FFMPEGEncoder::getMaxEncodingsCapability() const
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		shared_lock locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (const int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForEncoding)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		SPDLOG_INFO(
			"getMaxXXXXCapability"
			", lastCPUUsage: {}"
			", maxCapability: {}",
			accumulate(
				begin(*_cpuUsage), end(*_cpuUsage), string(),
				[](const string &s, int cpuUsage)
				{ return (s.empty() ? std::format("{}", cpuUsage) : std::format("{}, {}", s, cpuUsage)); }
			), maxCapability
		);

		return maxCapability;
	}
}

int FFMPEGEncoder::getMaxLiveProxiesCapability(int64_t ingestionJobKey) const
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		shared_lock locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (const int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForProxy)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		SPDLOG_INFO(
			"getMaxXXXXCapability"
			", ingestionJobKey: {}"
			", lastCPUUsage: {}"
			", maxCapability: {}",
			ingestionJobKey, accumulate(
				begin(*_cpuUsage), end(*_cpuUsage), string(),
				[](const string &s, int cpuUsage)
				{ return (s.empty() ? std::format("{}", cpuUsage) : std::format("{}, {}", s, cpuUsage)); }
			), maxCapability
		);

		return maxCapability;
	}
}

int FFMPEGEncoder::getMaxLiveRecordingsCapability() const
{
	// 2021-08-23: Use of the cpu usage to determine if an activity has to
	// be done
	{
		shared_lock locker(*_cpuUsageMutex);

		int maxCapability = VECTOR_MAX_CAPACITY; // it could be done

		for (const int cpuUsage : *_cpuUsage)
		{
			if (cpuUsage > _cpuUsageThresholdForRecording)
			{
				maxCapability = 0; // no to be done

				break;
			}
		}

		SPDLOG_INFO(
			"getMaxXXXXCapability"
			", lastCPUUsage: {}"
			", maxCapability: {}",
			accumulate(
				begin(*_cpuUsage), end(*_cpuUsage), string(),
				[](const string &s, int cpuUsage)
				{ return (s.empty() ? std::format("{}", cpuUsage) : std::format("{}, {}", s, cpuUsage)); }
			), maxCapability
		);

		return maxCapability;
	}
}

string FFMPEGEncoder::buildFilterNotificationIngestionWorkflow(int64_t ingestionJobKey, const string& filterName, json ingestedParametersRoot)
{
	try
	{
		string workflowMetadata;
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

					if (filterName == "blackdetect" || filterName == "blackframe" || filterName == "freezedetect"
						|| filterName == "silentdetect")
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
	catch (exception& e)
	{
		SPDLOG_ERROR(
			"buildFilterNotificationIngestionWorkflow failed"
			", ingestionJobKey: {}"
			", exception: {}",
			ingestionJobKey, e.what()
		);

		throw;
	}
}

// questo metodo è duplicato anche in FFMPEGEncoderDaemons
/*
void FFMPEGEncoder::termProcess(
	shared_ptr<FFMPEGEncoderBase::Encoding> selectedEncoding, int64_t ingestionJobKey, string label, string message, bool kill
)
{
	try
	{
		// 2022-11-02: SIGQUIT is managed inside FFMpeg.cpp by liveProxy
		// 2023-02-18: using SIGQUIT, the process was not stopped, it worked with SIGTERM SIGTERM now is managed by FFMpeg.cpp too
		chrono::system_clock::time_point start = chrono::system_clock::now();
		ProcessUtility::ProcessId previousChildProcessId;
		previousChildProcessId = selectedEncoding->_childProcessId;
		if (!previousChildProcessId.isInitialized())
			return;
		long secondsToWait = 10;
		int counter = 0;
		do
		{
			if (!(selectedEncoding->_childProcessId).isInitialized() || selectedEncoding->_childProcessId != previousChildProcessId)
				break;

			if (kill)
				ProcessUtility::killProcess(previousChildProcessId);
			else
				ProcessUtility::termProcess(previousChildProcessId);
			SPDLOG_INFO(
				"ProcessUtility::termProcess"
				", ingestionJobKey: {}"
				", encodingJobKey: {}"
				", label: {}"
				", message: {}"
				", previousChildPid: {}"
				", selectedEncoding->_childProcessId: {}"
				", kill: {}"
				", counter: {}",
				ingestionJobKey, selectedEncoding->_encodingJobKey, label, message, previousChildProcessId.toString(),
				selectedEncoding->_childProcessId.toString(), kill, counter++
			);
			this_thread::sleep_for(chrono::seconds(1));
			// ripete il loop se la condizione è true
		} while (selectedEncoding->_childProcessId == previousChildProcessId && chrono::system_clock::now() - start <= chrono::seconds(secondsToWait)
		);
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
*/

void FFMPEGEncoder::sendError(FCGX_Request &request, int htmlResponseCode, const string_view& errorMessage)
{
	json responseBodyRoot;
	responseBodyRoot["status"] = to_string(htmlResponseCode);
	responseBodyRoot["error"] = errorMessage;

	FastCGIAPI::sendError(request, htmlResponseCode, JSONUtils::toString(responseBodyRoot));
}

void FFMPEGEncoder::loadConfiguration(const json& configurationRoot)
{
	_configurationRoot = configurationRoot;
	_encodingCompletedRetentionInSeconds = JSONUtils::asInt(_configurationRoot["ffmpeg"], "encodingCompletedRetentionInSeconds", 0);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encodingCompletedRetentionInSeconds: {}",
		_encodingCompletedRetentionInSeconds
	);

	_mmsAPITimeoutInSeconds = JSONUtils::asInt(_configurationRoot["api"], "timeoutInSeconds", 120);
	SPDLOG_INFO(
		"Configuration item"
		", api->timeoutInSeconds: {}",
		_mmsAPITimeoutInSeconds
	);

	_cpuUsageThresholdForEncoding = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForEncoding", 50);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->cpuUsageThresholdForEncoding: {}",
		_cpuUsageThresholdForEncoding
	);
	_cpuUsageThresholdForRecording = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForRecording", 60);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->cpuUsageThresholdForRecording: {}",
		_cpuUsageThresholdForRecording
	);
	_cpuUsageThresholdForProxy = JSONUtils::asInt(_configurationRoot["ffmpeg"], "cpuUsageThresholdForProxy", 70);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->cpuUsageThresholdForProxy: {}",
		_cpuUsageThresholdForProxy
	);
	_intervalInSecondsBetweenEncodingAccept =
		JSONUtils::asInt(_configurationRoot["ffmpeg"], "intervalInSecondsBetweenEncodingAccept", 30);
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->intervalInSecondsBetweenEncodingAccept: {}",
		_intervalInSecondsBetweenEncodingAccept
	);

	_encoderUser = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderUser", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderUser: {}",
		_encoderUser
	);
	_encoderPassword = JSONUtils::asString(_configurationRoot["ffmpeg"], "encoderPassword", "");
	SPDLOG_INFO(
		"Configuration item"
		", ffmpeg->encoderPassword: {}",
		_encoderPassword
	);
}

