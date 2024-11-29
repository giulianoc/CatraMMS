
#include "MMSEngineProcessor.h"
#include "CheckEncodingTimes.h"
#include "CheckIngestionTimes.h"
#include "CheckRefreshPartitionFreeSizeTimes.h"
#include "ContentRetentionTimes.h"
#include "DBDataRetentionTimes.h"
#include "GEOInfoTimes.h"
#include "JSONUtils.h"
#include "MMSCURL.h"
#include "ThreadsStatisticTimes.h"
#include "catralibraries/Encrypt.h"
#include "catralibraries/System.h"
#include <curlpp/Easy.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/cURLpp.hpp>
/*
#include <stdio.h>

#include "FFMpeg.h"
#include "PersistenceLock.h"
#include "catralibraries/Convert.h"
#include "catralibraries/DateTime.h"
#include "catralibraries/ProcessUtility.h"
#include "catralibraries/StringUtils.h"
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
// #include "EMailSender.h"
#include "Magick++.h"
// #include <openssl/md5.h>
#include "spdlog/spdlog.h"
#include <openssl/evp.h>

#define MD5BUFFERSIZE 16384
*/

MMSEngineProcessor::MMSEngineProcessor(
	int processorIdentifier, shared_ptr<spdlog::logger> logger, shared_ptr<MultiEventsSet> multiEventsSet,
	shared_ptr<MMSEngineDBFacade> mmsEngineDBFacade, shared_ptr<MMSStorage> mmsStorage, shared_ptr<long> processorsThreadsNumber,
	shared_ptr<ThreadsStatistic> mmsThreadsStatistic, shared_ptr<MMSDeliveryAuthorization> mmsDeliveryAuthorization,
	ActiveEncodingsManager *pActiveEncodingsManager, mutex *cpuUsageMutex, deque<int> *cpuUsage, json configurationRoot
)
{
	_processorIdentifier = processorIdentifier;
	_logger = logger;
	_configurationRoot = configurationRoot;
	_multiEventsSet = multiEventsSet;
	_mmsEngineDBFacade = mmsEngineDBFacade;
	_mmsStorage = mmsStorage;
	_processorsThreadsNumber = processorsThreadsNumber;
	_mmsThreadsStatistic = mmsThreadsStatistic;
	_mmsDeliveryAuthorization = mmsDeliveryAuthorization;
	_pActiveEncodingsManager = pActiveEncodingsManager;

	_processorMMS = System::getHostName();

	_cpuUsageMutex = cpuUsageMutex;
	_cpuUsage = cpuUsage;
	_cpuUsageThreadShutdown = false;

	_processorThreads = JSONUtils::asInt(configurationRoot["mms"], "processorThreads", 1);
	_cpuUsageThreshold = JSONUtils::asInt(configurationRoot["mms"], "cpuUsageThreshold", 10);

	_maxDownloadAttemptNumber = JSONUtils::asInt(configurationRoot["download"], "maxDownloadAttemptNumber", 5);
	SPDLOG_INFO(
		"Configuration item"
		", download->maxDownloadAttemptNumber: {}",
		_maxDownloadAttemptNumber
	);
	_progressUpdatePeriodInSeconds = JSONUtils::asInt(configurationRoot["download"], "progressUpdatePeriodInSeconds", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", download->progressUpdatePeriodInSeconds: " + to_string(_progressUpdatePeriodInSeconds));
	_secondsWaitingAmongDownloadingAttempt = JSONUtils::asInt(configurationRoot["download"], "secondsWaitingAmongDownloadingAttempt", 5);
	SPDLOG_INFO(
		string() + "Configuration item" + ", download->secondsWaitingAmongDownloadingAttempt: " + to_string(_secondsWaitingAmongDownloadingAttempt)
	);

	_maxIngestionJobsPerEvent = JSONUtils::asInt(configurationRoot["mms"], "maxIngestionJobsPerEvent", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", mms->maxIngestionJobsPerEvent: " + to_string(_maxIngestionJobsPerEvent));
	_maxEncodingJobsPerEvent = JSONUtils::asInt(configurationRoot["mms"], "maxEncodingJobsPerEvent", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", mms->maxEncodingJobsPerEvent: " + to_string(_maxEncodingJobsPerEvent));

	_maxEventManagementTimeInSeconds = JSONUtils::asInt(configurationRoot["mms"], "maxEventManagementTimeInSeconds", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", mms->maxEventManagementTimeInSeconds: " + to_string(_maxEventManagementTimeInSeconds));

	_dependencyExpirationInHours = JSONUtils::asInt(configurationRoot["mms"], "dependencyExpirationInHours", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", mms->dependencyExpirationInHours: " + to_string(_dependencyExpirationInHours));

	_timeBeforeToPrepareResourcesInMinutes = JSONUtils::asInt(configurationRoot["mms"], "liveRecording_timeBeforeToPrepareResourcesInMinutes", 2);

	_downloadChunkSizeInMegaBytes = JSONUtils::asInt(configurationRoot["download"], "downloadChunkSizeInMegaBytes", 5);
	SPDLOG_INFO(string() + "Configuration item" + ", download->downloadChunkSizeInMegaBytes: " + to_string(_downloadChunkSizeInMegaBytes));

	_emailProviderURL = JSONUtils::asString(_configurationRoot["EmailNotification"], "providerURL", "");
	SPDLOG_INFO(string() + "Configuration item" + ", EmailNotification->providerURL: " + _emailProviderURL);
	_emailUserName = JSONUtils::asString(_configurationRoot["EmailNotification"], "userName", "");
	SPDLOG_INFO(string() + "Configuration item" + ", EmailNotification->userName: " + _emailUserName);
	{
		string encryptedPassword = JSONUtils::asString(_configurationRoot["EmailNotification"], "password", "");
		_emailPassword = Encrypt::opensslDecrypt(encryptedPassword);
		SPDLOG_INFO(
			string() + "Configuration item" + ", EmailNotification->password: " + encryptedPassword
			// + ", EmailNotification->password: " + _emailPassword
		);
	}
	_emailCcsCommaSeparated = JSONUtils::asString(_configurationRoot["EmailNotification"], "cc", "");
	SPDLOG_INFO(string() + "Configuration item" + ", EmailNotification->cc: " + _emailCcsCommaSeparated);

	_facebookGraphAPIProtocol = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "protocol", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->protocol: " + _facebookGraphAPIProtocol);
	_facebookGraphAPIHostName = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "hostName", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->hostName: " + _facebookGraphAPIHostName);
	_facebookGraphAPIVideoHostName = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "videoHostName", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->videoHostName: " + _facebookGraphAPIVideoHostName);
	_facebookGraphAPIPort = JSONUtils::asInt(_configurationRoot["FacebookGraphAPI"], "port", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->port: " + to_string(_facebookGraphAPIPort));
	_facebookGraphAPIVersion = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "version", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->version: " + _facebookGraphAPIVersion);
	_facebookGraphAPITimeoutInSeconds = JSONUtils::asInt(_configurationRoot["FacebookGraphAPI"], "timeout", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->timeout: " + to_string(_facebookGraphAPITimeoutInSeconds));
	_facebookGraphAPIClientId = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "clientId", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->clientId: " + _facebookGraphAPIClientId);
	_facebookGraphAPIClientSecret = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "clientSecret", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->clientSecret: " + _facebookGraphAPIClientSecret);
	_facebookGraphAPIRedirectURL = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "redirectURL", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->redirectURL: " + _facebookGraphAPIRedirectURL);
	_facebookGraphAPIAccessTokenURI = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "accessTokenURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->accessTokenURI: " + _facebookGraphAPIAccessTokenURI);
	_facebookGraphAPILiveVideosURI = JSONUtils::asString(_configurationRoot["FacebookGraphAPI"], "liveVideosURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", FacebookGraphAPI->liveVideosURI: " + _facebookGraphAPILiveVideosURI);

	_youTubeDataAPIProtocol = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "protocol", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->protocol: " + _youTubeDataAPIProtocol);
	_youTubeDataAPIHostName = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "hostName", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->hostName: " + _youTubeDataAPIHostName);
	_youTubeDataAPIPort = JSONUtils::asInt(_configurationRoot["YouTubeDataAPI"], "port", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->port: " + to_string(_youTubeDataAPIPort));
	_youTubeDataAPIRefreshTokenURI = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "refreshTokenURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->refreshTokenURI: " + _youTubeDataAPIRefreshTokenURI);
	_youTubeDataAPIUploadVideoURI = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "uploadVideoURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->uploadVideoURI: " + _youTubeDataAPIUploadVideoURI);
	_youTubeDataAPILiveBroadcastURI = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "liveBroadcastURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->liveBroadcastURI: " + _youTubeDataAPILiveBroadcastURI);
	_youTubeDataAPILiveStreamURI = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "liveStreamURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->liveStreamURI: " + _youTubeDataAPILiveStreamURI);
	_youTubeDataAPILiveBroadcastBindURI = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "liveBroadcastBindURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->liveBroadcastBindURI: " + _youTubeDataAPILiveBroadcastBindURI);
	_youTubeDataAPITimeoutInSeconds = JSONUtils::asInt(_configurationRoot["YouTubeDataAPI"], "timeout", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->timeout: " + to_string(_youTubeDataAPITimeoutInSeconds));
	_youTubeDataAPITimeoutInSecondsForUploadVideo = JSONUtils::asInt(_configurationRoot["YouTubeDataAPI"], "timeoutForUploadVideo", 0);
	SPDLOG_INFO(
		string() + "Configuration item" + ", YouTubeDataAPI->timeoutForUploadVideo: " + to_string(_youTubeDataAPITimeoutInSecondsForUploadVideo)
	);
	_youTubeDataAPIClientId = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "clientId", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->clientId: " + _youTubeDataAPIClientId);
	_youTubeDataAPIClientSecret = JSONUtils::asString(_configurationRoot["YouTubeDataAPI"], "clientSecret", "");
	SPDLOG_INFO(string() + "Configuration item" + ", YouTubeDataAPI->clientSecret: " + _youTubeDataAPIClientSecret);

	_localCopyTaskEnabled = JSONUtils::asBool(_configurationRoot["mms"], "localCopyTaskEnabled", false);
	SPDLOG_INFO(string() + "Configuration item" + ", mms->localCopyTaskEnabled: " + to_string(_localCopyTaskEnabled));

	string mmsAPIProtocol = JSONUtils::asString(_configurationRoot["api"], "protocol", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->protocol: " + mmsAPIProtocol);
	string mmsAPIHostname = JSONUtils::asString(_configurationRoot["api"], "hostname", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->hostname: " + mmsAPIHostname);
	int mmsAPIPort = JSONUtils::asInt(_configurationRoot["api"], "port", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", api->port: " + to_string(mmsAPIPort));
	string mmsAPIVersion = JSONUtils::asString(_configurationRoot["api"], "version", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->version: " + mmsAPIVersion);
	string mmsAPIWorkflowURI = JSONUtils::asString(_configurationRoot["api"], "workflowURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->workflowURI: " + mmsAPIWorkflowURI);
	string mmsAPIIngestionURI = JSONUtils::asString(_configurationRoot["api"], "ingestionURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->ingestionURI: " + mmsAPIIngestionURI);
	string mmsBinaryProtocol = JSONUtils::asString(_configurationRoot["api"]["binary"], "protocol", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->binary->protocol: " + mmsBinaryProtocol);
	string mmsBinaryHostname = JSONUtils::asString(_configurationRoot["api"]["binary"], "hostname", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->binary->hostname: " + mmsBinaryHostname);
	int mmsBinaryPort = JSONUtils::asInt(_configurationRoot["api"]["binary"], "port", 0);
	SPDLOG_INFO(string() + "Configuration item" + ", api->binary->port: " + to_string(mmsBinaryPort));
	string mmsBinaryVersion = JSONUtils::asString(_configurationRoot["api"]["binary"], "version", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->binary->version: " + mmsBinaryVersion);
	string mmsBinaryIngestionURI = JSONUtils::asString(_configurationRoot["api"]["binary"], "ingestionURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->binary->ingestionURI: " + mmsBinaryIngestionURI);
	_mmsAPIVODDeliveryURI = JSONUtils::asString(_configurationRoot["api"], "vodDeliveryURI", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->vodDeliveryURI: " + _mmsAPIVODDeliveryURI);
	_mmsAPITimeoutInSeconds = JSONUtils::asInt(_configurationRoot["api"], "timeoutInSeconds", 120);
	SPDLOG_INFO(string() + "Configuration item" + ", api->timeoutInSeconds: " + to_string(_mmsAPITimeoutInSeconds));

	_deliveryProtocol = JSONUtils::asString(_configurationRoot["api"]["delivery"], "deliveryProtocol", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->delivery->deliveryProtocol: " + _deliveryProtocol);
	_deliveryHost = JSONUtils::asString(_configurationRoot["api"]["delivery"], "deliveryHost", "");
	SPDLOG_INFO(string() + "Configuration item" + ", api->delivery->deliveryHost: " + _deliveryHost);

	_waitingNFSSync_maxMillisecondsToWait = JSONUtils::asInt(configurationRoot["storage"], "waitingNFSSync_maxMillisecondsToWait", 60000);
	SPDLOG_INFO(
		string() + "Configuration item" + ", storage->_waitingNFSSync_maxMillisecondsToWait: " + to_string(_waitingNFSSync_maxMillisecondsToWait)
	);
	_waitingNFSSync_milliSecondsWaitingBetweenChecks =
		JSONUtils::asInt(configurationRoot["storage"], "waitingNFSSync_milliSecondsWaitingBetweenChecks", 100);
	SPDLOG_INFO(
		string() + "Configuration item" +
		", storage->waitingNFSSync_milliSecondsWaitingBetweenChecks: " + to_string(_waitingNFSSync_milliSecondsWaitingBetweenChecks)
	);

	_liveRecorderVirtualVODImageLabel = JSONUtils::asString(_configurationRoot["ffmpeg"], "liveRecorderVirtualVODImageLabel", "");
	SPDLOG_INFO(string() + "Configuration item" + ", ffmpeg->liveRecorderVirtualVODImageLabel: " + _liveRecorderVirtualVODImageLabel);

	_mmsWorkflowIngestionURL =
		mmsAPIProtocol + "://" + mmsAPIHostname + ":" + to_string(mmsAPIPort) + "/catramms/" + mmsAPIVersion + mmsAPIWorkflowURI;

	_mmsIngestionURL = mmsAPIProtocol + "://" + mmsAPIHostname + ":" + to_string(mmsAPIPort) + "/catramms/" + mmsAPIVersion + mmsAPIIngestionURI;

	_mmsBinaryIngestionURL =
		mmsBinaryProtocol + "://" + mmsBinaryHostname + ":" + to_string(mmsBinaryPort) + "/catramms/" + mmsBinaryVersion + mmsBinaryIngestionURI;

	if (_processorIdentifier == 0)
	{
		try
		{
			_mmsEngineDBFacade->resetProcessingJobsIfNeeded(_processorMMS);
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "_mmsEngineDBFacade->resetProcessingJobsIfNeeded failed" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			// throw e;
		}
	}
}

MMSEngineProcessor::~MMSEngineProcessor() {}

bool MMSEngineProcessor::isMaintenanceMode()
{
	string maintenancePathName = "/tmp/mmsMaintenanceMode.txt";

	ifstream f(maintenancePathName.c_str());

	return f.good();
}

bool MMSEngineProcessor::isProcessorShutdown()
{
	string processorShutdownPathName = "/tmp/processorShutdown.txt";

	ifstream f(processorShutdownPathName.c_str());

	return f.good();
}

void MMSEngineProcessor::operator()()
{
	bool blocking = true;
	chrono::milliseconds milliSecondsToBlock(100);

	// SPDLOG_DEBUG(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}",
	// 1, 3.23);
	//  SPDLOG_TRACE(_logger , "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}",
	//  1, 3.23);
	SPDLOG_INFO(string() + "MMSEngineProcessor thread started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

	bool processorShutdown = false;
	while (!processorShutdown)
	{
		if (isProcessorShutdown())
		{
			SPDLOG_INFO(string() + "Processor was shutdown" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			processorShutdown = true;

			continue;
		}

		shared_ptr<Event2> event = _multiEventsSet->getAndRemoveFirstEvent(MMSENGINEPROCESSORNAME, blocking, milliSecondsToBlock);
		if (event == nullptr)
		{
			// cout << "No event found or event not yet expired" << endl;

			continue;
		}

		chrono::system_clock::time_point startEvent = chrono::system_clock::now();

		switch (event->getEventKey().first)
		{
		case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTIONEVENT: // 1
		{
			SPDLOG_DEBUG(
				string() + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				handleCheckIngestionEvent();
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleCheckIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKINGESTION" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT: // 2
		{
			SPDLOG_DEBUG(string() + "1. Received LOCALASSETINGESTIONEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			shared_ptr<LocalAssetIngestionEvent> localAssetIngestionEvent = dynamic_pointer_cast<LocalAssetIngestionEvent>(event);

			try
			{
				/* 2021-02-19: check on threads is already done in
				handleCheckIngestionEvent if
				(_processorsThreadsNumber.use_count() > _processorThreads +
				_maxAdditionalProcessorThreads)
				{
					_logger->warn(string()
						+ "Not enough available threads to manage
				handleLocalAssetIngestionEvent, activity is postponed"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", ingestionJobKey: " +
				to_string(localAssetIngestionEvent->getIngestionJobKey())
						+ ", _processorsThreadsNumber.use_count(): " +
				to_string(_processorsThreadsNumber.use_count())
						+ ", _processorThreads + _maxAdditionalProcessorThreads:
				"
						+ to_string(_processorThreads +
				_maxAdditionalProcessorThreads)
					);

					{
						shared_ptr<LocalAssetIngestionEvent>
				cloneLocalAssetIngestionEvent =
				_multiEventsSet->getEventsFactory()->getFreeEvent<LocalAssetIngestionEvent>(
									MMSENGINE_EVENTTYPEIDENTIFIER_LOCALASSETINGESTIONEVENT);

						cloneLocalAssetIngestionEvent->setSource(
							localAssetIngestionEvent->getSource());
						cloneLocalAssetIngestionEvent->setDestination(
							localAssetIngestionEvent->getDestination());
						// 2019-11-15: it is important this message will expire
				later.
						//	Before this change (+ 5 seconds), the event expires
				istantly and we have file system full "
						//	because of the two messages
						//		- Not enough available threads... and
						//		- addEvent: EVENT_TYPE...
						cloneLocalAssetIngestionEvent->setExpirationTimePoint(
							chrono::system_clock::now() + chrono::seconds(5));

						cloneLocalAssetIngestionEvent->setExternalReadOnlyStorage(
							localAssetIngestionEvent->getExternalReadOnlyStorage());
						cloneLocalAssetIngestionEvent->setExternalStorageMediaSourceURL(
							localAssetIngestionEvent->getExternalStorageMediaSourceURL());
						cloneLocalAssetIngestionEvent->setIngestionJobKey(
							localAssetIngestionEvent->getIngestionJobKey());
						cloneLocalAssetIngestionEvent->setIngestionSourceFileName(
							localAssetIngestionEvent->getIngestionSourceFileName());
						cloneLocalAssetIngestionEvent->setMMSSourceFileName(
							localAssetIngestionEvent->getMMSSourceFileName());
						cloneLocalAssetIngestionEvent->setForcedAvgFrameRate(
							localAssetIngestionEvent->getForcedAvgFrameRate());
						cloneLocalAssetIngestionEvent->setWorkspace(
							localAssetIngestionEvent->getWorkspace());
						cloneLocalAssetIngestionEvent->setIngestionType(
							localAssetIngestionEvent->getIngestionType());
						cloneLocalAssetIngestionEvent->setIngestionRowToBeUpdatedAsSuccess(
							localAssetIngestionEvent->getIngestionRowToBeUpdatedAsSuccess());

						cloneLocalAssetIngestionEvent->setMetadataContent(
							localAssetIngestionEvent->getMetadataContent());

						shared_ptr<Event2>    cloneEvent =
				dynamic_pointer_cast<Event2>( cloneLocalAssetIngestionEvent);
						_multiEventsSet->addEvent(cloneEvent);

						SPDLOG_INFO(string() + "addEvent: EVENT_TYPE
				(INGESTASSETEVENT)"
							+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
							+ ", getEventKey().first: " +
				to_string(event->getEventKey().first)
							+ ", getEventKey().second: " +
				to_string(event->getEventKey().second));
					}
				}
				else
				*/
				{
					// handleLocalAssetIngestionEvent
					// (localAssetIngestionEvent);
					thread handleLocalAssetIngestionEventThread(
						&MMSEngineProcessor::handleLocalAssetIngestionEventThread, this, _processorsThreadsNumber, *localAssetIngestionEvent
					);
					handleLocalAssetIngestionEventThread.detach();
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<LocalAssetIngestionEvent>(localAssetIngestionEvent);

			SPDLOG_DEBUG(string() + "2. Received LOCALASSETINGESTIONEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier));
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODINGEVENT: // 3
		{
			SPDLOG_DEBUG(
				string() + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				handleCheckEncodingEvent();
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleCheckEncodingEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_CHECKENCODING" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT: // 4
		{
			SPDLOG_DEBUG(
				string() +
				"1. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				int maxAdditionalProcessorThreads = getMaxAdditionalProcessorThreads();
				if (_processorsThreadsNumber.use_count() > _processorThreads + maxAdditionalProcessorThreads)
				{
					// content retention is a periodical event, we will wait the
					// next one

					_logger->warn(
						string() +
						"Not enough available threads to manage "
						"handleContentRetentionEventThread, activity is "
						"postponed" +
						", _processorIdentifier: " + to_string(_processorIdentifier) +
						", _processorsThreadsNumber.use_count(): " + to_string(_processorsThreadsNumber.use_count()) +
						", _processorThreads + "
						"maxAdditionalProcessorThreads: " +
						to_string(_processorThreads + maxAdditionalProcessorThreads)
					);
				}
				else
				{
					thread contentRetention(&MMSEngineProcessor::handleContentRetentionEventThread, this, _processorsThreadsNumber);
					contentRetention.detach();
				}
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleContentRetentionEventThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() +
				"2. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_CONTENTRETENTIONEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT: // 5
		{
			SPDLOG_DEBUG(string() + "1. Received MULTILOCALASSETINGESTIONEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier));

			shared_ptr<MultiLocalAssetIngestionEvent> multiLocalAssetIngestionEvent = dynamic_pointer_cast<MultiLocalAssetIngestionEvent>(event);

			try
			{
				/* 2021-02-19: check on threads is already done in
				handleCheckIngestionEvent if
				(_processorsThreadsNumber.use_count() > _processorThreads +
				_maxAdditionalProcessorThreads)
				{
					_logger->warn(string()
						+ "Not enough available threads to manage
				handleLocalAssetIngestionEvent, activity is postponed"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", ingestionJobKey: " +
				to_string(multiLocalAssetIngestionEvent->getIngestionJobKey())
						+ ", _processorsThreadsNumber.use_count(): " +
				to_string(_processorsThreadsNumber.use_count())
						+ ", _processorThreads + _maxAdditionalProcessorThreads:
				"
						+ to_string(_processorThreads +
				_maxAdditionalProcessorThreads)
					);

					{
						shared_ptr<MultiLocalAssetIngestionEvent>
				cloneMultiLocalAssetIngestionEvent =
				_multiEventsSet->getEventsFactory()->getFreeEvent<MultiLocalAssetIngestionEvent>(
									MMSENGINE_EVENTTYPEIDENTIFIER_MULTILOCALASSETINGESTIONEVENT);

						cloneMultiLocalAssetIngestionEvent->setSource(
							multiLocalAssetIngestionEvent->getSource());
						cloneMultiLocalAssetIngestionEvent->setDestination(
							multiLocalAssetIngestionEvent->getDestination());
						// 2019-11-15: it is important this message will expire
				later.
						//	Before this change (+ 5 seconds), the event expires
				istantly and we have file system full "
						//	because of the two messages
						//	- Not enough available threads... and
						//	- addEvent: EVENT_TYPE...
						cloneMultiLocalAssetIngestionEvent->setExpirationTimePoint(
							chrono::system_clock::now() + chrono::seconds(5));

						cloneMultiLocalAssetIngestionEvent->setIngestionJobKey(
							multiLocalAssetIngestionEvent->getIngestionJobKey());
						cloneMultiLocalAssetIngestionEvent->setEncodingJobKey(
							multiLocalAssetIngestionEvent->getEncodingJobKey());
						cloneMultiLocalAssetIngestionEvent->setWorkspace(
							multiLocalAssetIngestionEvent->getWorkspace());
						cloneMultiLocalAssetIngestionEvent->setParametersRoot(
							multiLocalAssetIngestionEvent->getParametersRoot());

						shared_ptr<Event2>    cloneEvent =
				dynamic_pointer_cast<Event2>(
								cloneMultiLocalAssetIngestionEvent);
						_multiEventsSet->addEvent(cloneEvent);

						SPDLOG_INFO(string() + "addEvent: EVENT_TYPE
				(MULTIINGESTASSETEVENT)"
							+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
							+ ", getEventKey().first: " +
				to_string(event->getEventKey().first)
							+ ", getEventKey().second: " +
				to_string(event->getEventKey().second));
					}
				}
				else
				*/
				{
					// handleMultiLocalAssetIngestionEvent
					// (multiLocalAssetIngestionEvent);
					thread handleMultiLocalAssetIngestionEventThread(
						&MMSEngineProcessor::handleMultiLocalAssetIngestionEventThread, this, _processorsThreadsNumber, *multiLocalAssetIngestionEvent
					);
					handleMultiLocalAssetIngestionEventThread.detach();
				}
			}
			catch (runtime_error &e)
			{
				SPDLOG_ERROR(
					string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleMultiLocalAssetIngestionEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<MultiLocalAssetIngestionEvent>(multiLocalAssetIngestionEvent);

			SPDLOG_DEBUG(string() + "2. Received MULTILOCALASSETINGESTIONEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier));
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_DBDATARETENTIONEVENT: // 7
		{
			SPDLOG_DEBUG(
				string() +
				"1. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_DBDATARETENTIONEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				/* 2019-07-10: this check was removed since this event happens
				once a day if (_processorsThreadsNumber.use_count() >
				_processorThreads + _maxAdditionalProcessorThreads)
				{
					// content retention is a periodical event, we will wait the
				next one

					_logger->warn(string() + "Not enough available threads to
				manage handleContentRetentionEventThread, activity is postponed"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", _processorsThreadsNumber.use_count(): " +
				to_string(_processorsThreadsNumber.use_count())
						+ ", _processorThreads + _maxAdditionalProcessorThreads:
				" + to_string(_processorThreads +
				_maxAdditionalProcessorThreads)
					);
				}
				else
				*/
				{
					thread dbDataRetention(&MMSEngineProcessor::handleDBDataRetentionEventThread, this);
					dbDataRetention.detach();
				}
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleDBDataRetentionEventThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() +
				"2. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_DBDATARETENTIONEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_CHECKREFRESHPARTITIONFREESIZEEVENT: // 8
		{
			SPDLOG_DEBUG(
				string() +
				"1. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_"
				"CHECKREFRESHPARTITIONFREESIZEEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				/* 2019-07-10: this check was removed since this event happens
				once a day if (_processorsThreadsNumber.use_count() >
				_processorThreads + _maxAdditionalProcessorThreads)
				{
					// content retention is a periodical event, we will wait the
				next one

					_logger->warn(string() + "Not enough available threads to
				manage handleContentRetentionEventThread, activity is postponed"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", _processorsThreadsNumber.use_count(): " +
				to_string(_processorsThreadsNumber.use_count())
						+ ", _processorThreads + _maxAdditionalProcessorThreads:
				" + to_string(_processorThreads +
				_maxAdditionalProcessorThreads)
					);
				}
				else
				*/
				{
					thread checkRefreshPartitionFreeSize(&MMSEngineProcessor::handleCheckRefreshPartitionFreeSizeEventThread, this);
					checkRefreshPartitionFreeSize.detach();
				}
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleCheckRefreshPartitionFreeSizeEvent failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() +
				"2. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_"
				"CHECKREFRESHPARTITIONFREESIZEEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_THREADSSTATISTICEVENT: // 9
		{
			SPDLOG_DEBUG(
				string() +
				"1. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_THREADSSTATISTICEVENT" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				_mmsThreadsStatistic->logRunningThreads();
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "_mmsThreadsStatistic->logRunningThreads failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() +
				"2. Received "
				"MMSENGINE_EVENTTYPEIDENTIFIER_THREADSSTATISTICEVENT:" +
				", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		case MMSENGINE_EVENTTYPEIDENTIFIER_GEOINFOEVENT: // 10
		{
			SPDLOG_DEBUG(
				string() + "1. Received MMSENGINE_EVENTTYPEIDENTIFIER_GEOINFOEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);

			try
			{
				/* 2019-07-10: this check was removed since this event happens
				once a day if (_processorsThreadsNumber.use_count() >
				_processorThreads + _maxAdditionalProcessorThreads)
				{
					// GEOInfo is a periodical event, we will wait the next one

					_logger->warn(string() + "Not enough available threads to
				manage handleGEOInfoEventThread, activity is postponed"
						+ ", _processorIdentifier: " +
				to_string(_processorIdentifier)
						+ ", _processorsThreadsNumber.use_count(): " +
				to_string(_processorsThreadsNumber.use_count())
						+ ", _processorThreads + _maxAdditionalProcessorThreads:
				" + to_string(_processorThreads +
				_maxAdditionalProcessorThreads)
					);
				}
				else
				*/
				{
					thread geoInfo(&MMSEngineProcessor::handleGEOInfoEventThread, this);
					geoInfo.detach();
				}
			}
			catch (exception &e)
			{
				SPDLOG_ERROR(
					string() + "handleGEOInfoEventThread failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", exception: " + e.what()
				);
			}

			_multiEventsSet->getEventsFactory()->releaseEvent<Event2>(event);

			SPDLOG_DEBUG(
				string() + "2. Received MMSENGINE_EVENTTYPEIDENTIFIER_GEOINFOEVENT" + ", _processorIdentifier: " + to_string(_processorIdentifier)
			);
		}
		break;
		default:
			throw runtime_error(string("Event type identifier not managed") + to_string(event->getEventKey().first));
		}

		chrono::system_clock::time_point endEvent = chrono::system_clock::now();
		long elapsedInSeconds = chrono::duration_cast<chrono::seconds>(endEvent - startEvent).count();

		if (elapsedInSeconds > _maxEventManagementTimeInSeconds)
			_logger->warn(
				string() + "MMSEngineProcessor. Event management took too time" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", event id: " + to_string(event->getEventKey().first) + ", _maxEventManagementTimeInSeconds: " +
				to_string(_maxEventManagementTimeInSeconds) + ", @MMS statistics@ - elapsed in seconds: @" + to_string(elapsedInSeconds) + "@"
			);
	}

	SPDLOG_INFO(string() + "MMSEngineProcessor thread terminated" + ", _processorIdentifier: " + to_string(_processorIdentifier));
}

int MMSEngineProcessor::getMaxAdditionalProcessorThreads()
{
	lock_guard<mutex> locker(*_cpuUsageMutex);

	// int maxAdditionalProcessorThreads = VECTOR_MAX_CAPACITY;	// it could be
	// done
	int maxAdditionalProcessorThreads = 20; // it could be done

	for (int cpuUsage : *_cpuUsage)
	{
		if (cpuUsage > _cpuUsageThreshold)
		{
			maxAdditionalProcessorThreads = 0; // no to be done

			break;
		}
	}

	string lastCPUUsage;
	for (int cpuUsage : *_cpuUsage)
		lastCPUUsage += (to_string(cpuUsage) + " ");
	SPDLOG_INFO(
		string() + "getMaxAdditionalProcessorThreads" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", lastCPUUsage: " + lastCPUUsage + ", maxAdditionalProcessorThreads: " + to_string(maxAdditionalProcessorThreads)
	);

	return maxAdditionalProcessorThreads;
}

void MMSEngineProcessor::cpuUsageThread()
{

	int64_t counter = 0;

	while (!_cpuUsageThreadShutdown)
	{
		this_thread::sleep_for(chrono::milliseconds(50));

		try
		{
			lock_guard<mutex> locker(*_cpuUsageMutex);

			_cpuUsage->pop_back();
			_cpuUsage->push_front(_getCpuUsage.getCpuUsage());
			// *_cpuUsage = _getCpuUsage.getCpuUsage();

			if (++counter % 100 == 0)
			{
				string lastCPUUsage;
				for (int cpuUsage : *_cpuUsage)
					lastCPUUsage += (to_string(cpuUsage) + " ");

				SPDLOG_INFO(string() + "cpuUsageThread" + ", lastCPUUsage: " + lastCPUUsage);
			}
		}
		catch (runtime_error &e)
		{
			string errorMessage = string("cpuUsage thread failed") + ", e.what(): " + e.what();

			SPDLOG_ERROR(string() + errorMessage);
		}
		catch (exception &e)
		{
			string errorMessage = string("cpuUsage thread failed") + ", e.what(): " + e.what();

			SPDLOG_ERROR(string() + errorMessage);
		}
	}
}

void MMSEngineProcessor::stopCPUUsageThread()
{

	_cpuUsageThreadShutdown = true;

	this_thread::sleep_for(chrono::seconds(1));
}

json MMSEngineProcessor::getReviewedOutputsRoot(
	json outputsRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey, bool encodingProfileMandatory
)
{
	json localOutputsRoot = json::array();

	if (outputsRoot == nullptr)
		return localOutputsRoot;

	for (int outputIndex = 0; outputIndex < outputsRoot.size(); outputIndex++)
	{
		json outputRoot = outputsRoot[outputIndex];

		string videoMap;
		string audioMap;
		string outputType;
		string otherOutputOptions;
		// int videoTrackIndexToBeUsed = -1;
		// int audioTrackIndexToBeUsed = -1;
		json filtersRoot = nullptr;
		int64_t encodingProfileKey = -1;
		json encodingProfileDetailsRoot = nullptr;
		MMSEngineDBFacade::ContentType encodingProfileContentType = MMSEngineDBFacade::ContentType::Video;
		string awsChannelConfigurationLabel;
		bool awsSignedURL = false;
		int awsExpirationInMinutes = -1;
		string cdn77ChannelConfigurationLabel;
		int cdn77ExpirationInMinutes = -1;
		string rtmpChannelConfigurationLabel;
		string hlsChannelConfigurationLabel;
		string udpUrl;
		json drawTextDetailsRoot = nullptr;

		string field = "videoMap";
		videoMap = JSONUtils::asString(outputRoot, field, "default");

		field = "audioMap";
		audioMap = JSONUtils::asString(outputRoot, field, "default");

		field = "outputType";
		outputType = JSONUtils::asString(outputRoot, field, "HLS_Channel");

		field = "otherOutputOptions";
		otherOutputOptions = JSONUtils::asString(outputRoot, field, "");

		// field = "videoTrackIndexToBeUsed";
		// videoTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, field, -1);

		// field = "audioTrackIndexToBeUsed";
		// audioTrackIndexToBeUsed = JSONUtils::asInt(outputRoot, field, -1);

		field = "filters";
		if (JSONUtils::isMetadataPresent(outputRoot, field))
			filtersRoot = outputRoot[field];

		filtersRoot = getReviewedFiltersRoot(filtersRoot, workspace, ingestionJobKey);

		if (outputType == "CDN_AWS")
		{
			field = "awsChannelConfigurationLabel";
			awsChannelConfigurationLabel = JSONUtils::asString(outputRoot, field, "");

			field = "awsSignedURL";
			awsSignedURL = JSONUtils::asBool(outputRoot, field, false);

			field = "awsExpirationInMinutes";
			awsExpirationInMinutes = JSONUtils::asInt(outputRoot, field, 1440); // 1 day
		}
		else if (outputType == "CDN_CDN77")
		{
			// it could not exist in case of SHARED CDN77
			field = "cdn77ChannelConfigurationLabel";
			cdn77ChannelConfigurationLabel = JSONUtils::asString(outputRoot, field, "");

			// cdn77ExpirationInMinutes is needed only in case of signed url
			field = "cdn77ExpirationInMinutes";
			cdn77ExpirationInMinutes = JSONUtils::asInt(outputRoot, field, 1440); // 1 day
		}
		else if (outputType == "RTMP_Channel")
		{
			// it could not exist in case of SHARED RTMP
			field = "rtmpChannelConfigurationLabel";
			rtmpChannelConfigurationLabel = JSONUtils::asString(outputRoot, field, "");
		}
		else if (outputType == "HLS_Channel")
		{
			// it could not exist in case of SHARED RTMP
			field = "hlsChannelConfigurationLabel";
			hlsChannelConfigurationLabel = JSONUtils::asString(outputRoot, field, "");
		}
		else // if (outputType == "UDP_Stream")
		{
			field = "udpUrl";
			udpUrl = JSONUtils::asString(outputRoot, field, "");
		}

		string keyField = "encodingProfileKey";
		string labelField = "encodingProfileLabel";
		string contentTypeField = "contentType";
		if (JSONUtils::isMetadataPresent(outputRoot, keyField))
		{
			encodingProfileKey = JSONUtils::asInt64(outputRoot, keyField, 0);

			SPDLOG_INFO(
				string() + "outputRoot encodingProfileKey" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingProfileKey: " + to_string(encodingProfileKey)
			);
		}
		else if (JSONUtils::isMetadataPresent(outputRoot, labelField))
		{
			string encodingProfileLabel = JSONUtils::asString(outputRoot, labelField, "");
			if (encodingProfileLabel != "")
			{
				MMSEngineDBFacade::ContentType contentType;
				if (JSONUtils::isMetadataPresent(outputRoot, contentTypeField))
				{
					contentType = MMSEngineDBFacade::toContentType(JSONUtils::asString(outputRoot, contentTypeField, ""));

					encodingProfileKey =
						_mmsEngineDBFacade->getEncodingProfileKeyByLabel(workspace->_workspaceKey, contentType, encodingProfileLabel);
				}
				else
				{
					bool contentTypeToBeUsed = false;
					encodingProfileKey = _mmsEngineDBFacade->getEncodingProfileKeyByLabel(
						workspace->_workspaceKey, contentType, encodingProfileLabel, contentTypeToBeUsed
					);
				}

				SPDLOG_INFO(
					string() + "outputRoot encodingProfileLabel" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
					", ingestionJobKey: " + to_string(ingestionJobKey) + ", encodingProfileLabel: " + encodingProfileLabel +
					", encodingProfileKey: " + to_string(encodingProfileKey)
				);
			}
		}

		if (encodingProfileKey != -1)
		{
			string jsonEncodingProfile;

			tuple<string, MMSEngineDBFacade::ContentType, MMSEngineDBFacade::DeliveryTechnology, string> encodingProfileDetails =
				_mmsEngineDBFacade->getEncodingProfileDetailsByKey(workspace->_workspaceKey, encodingProfileKey);
			tie(ignore, encodingProfileContentType, ignore, jsonEncodingProfile) = encodingProfileDetails;

			encodingProfileDetailsRoot = JSONUtils::toJson(jsonEncodingProfile);
		}
		else
		{
			if (encodingProfileMandatory)
			{
				string errorMessage = string() + "EncodingProfile is mandatory in case of Image" + ", ingestionJobKey: " + to_string(ingestionJobKey);
				SPDLOG_ERROR(errorMessage);

				throw runtime_error(errorMessage);
			}
		}

		field = "drawTextDetails";
		if (JSONUtils::isMetadataPresent(outputRoot, field))
			drawTextDetailsRoot = outputRoot[field];

		json localOutputRoot;

		field = "videoMap";
		localOutputRoot[field] = videoMap;

		field = "audioMap";
		localOutputRoot[field] = audioMap;

		field = "outputType";
		localOutputRoot[field] = outputType;

		field = "otherOutputOptions";
		localOutputRoot[field] = otherOutputOptions;

		// field = "videoTrackIndexToBeUsed";
		// localOutputRoot[field] = videoTrackIndexToBeUsed;

		// field = "audioTrackIndexToBeUsed";
		// localOutputRoot[field] = audioTrackIndexToBeUsed;

		field = "filters";
		localOutputRoot[field] = filtersRoot;

		{
			field = "encodingProfileKey";
			localOutputRoot[field] = encodingProfileKey;

			field = "encodingProfileDetails";
			localOutputRoot[field] = encodingProfileDetailsRoot;

			field = "encodingProfileContentType";
			outputRoot[field] = MMSEngineDBFacade::toString(encodingProfileContentType);
		}

		field = "awsChannelConfigurationLabel";
		localOutputRoot[field] = awsChannelConfigurationLabel;

		field = "awsSignedURL";
		localOutputRoot[field] = awsSignedURL;

		field = "awsExpirationInMinutes";
		localOutputRoot[field] = awsExpirationInMinutes;

		field = "cdn77ChannelConfigurationLabel";
		localOutputRoot[field] = cdn77ChannelConfigurationLabel;

		field = "cdn77ExpirationInMinutes";
		localOutputRoot[field] = cdn77ExpirationInMinutes;

		field = "rtmpChannelConfigurationLabel";
		localOutputRoot[field] = rtmpChannelConfigurationLabel;

		field = "hlsChannelConfigurationLabel";
		localOutputRoot[field] = hlsChannelConfigurationLabel;

		field = "udpUrl";
		localOutputRoot[field] = udpUrl;

		if (drawTextDetailsRoot != nullptr)
		{
			field = "drawTextDetails";
			localOutputRoot[field] = drawTextDetailsRoot;
		}

		localOutputsRoot.push_back(localOutputRoot);
	}

	return localOutputsRoot;
}

// LO STESSO METODO E' IN API_Ingestion.cpp
json MMSEngineProcessor::getReviewedFiltersRoot(json filtersRoot, shared_ptr<Workspace> workspace, int64_t ingestionJobKey)
{
	if (filtersRoot == nullptr)
		return filtersRoot;

	// se viene usato il filtro imageoverlay, è necessario recuperare sourcePhysicalPathName e sourcePhysicalDeliveryURL
	if (JSONUtils::isMetadataPresent(filtersRoot, "complex"))
	{
		json complexFiltersRoot = filtersRoot["complex"];
		for (int complexFilterIndex = 0; complexFilterIndex < complexFiltersRoot.size(); complexFilterIndex++)
		{
			json complexFilterRoot = complexFiltersRoot[complexFilterIndex];
			if (JSONUtils::isMetadataPresent(complexFilterRoot, "type") && complexFilterRoot["type"] == "imageoverlay")
			{
				if (!JSONUtils::isMetadataPresent(complexFilterRoot, "imagePhysicalPathKey"))
				{
					string errorMessage = fmt::format(
						"imageoverlay filter without imagePhysicalPathKey"
						", ingestionJobKey: {}"
						", imageoverlay filter: {}",
						ingestionJobKey, JSONUtils::toString(complexFilterRoot)
					);
					SPDLOG_ERROR(errorMessage);

					throw runtime_error(errorMessage);
				}

				string sourcePhysicalPathName;
				{
					tuple<string, int, string, string, int64_t, string> physicalPathDetails =
						_mmsStorage->getPhysicalPathDetails(complexFilterRoot["imagePhysicalPathKey"], false);
					tie(sourcePhysicalPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
				}

				// calculate delivery URL in case of an external encoder
				string sourcePhysicalDeliveryURL;
				{
					int64_t utcNow;
					{
						chrono::system_clock::time_point now = chrono::system_clock::now();
						utcNow = chrono::system_clock::to_time_t(now);
					}

					pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
						-1, // userKey,
						workspace,
						"", // clientIPAddress,

						-1, // mediaItemKey,
						"", // uniqueName,
						-1, // encodingProfileKey,
						"", // encodingProfileLabel,

						complexFilterRoot["imagePhysicalPathKey"],

						-1, // ingestionJobKey,	(in case of live)
						-1, // deliveryCode,

						365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
						999999,				// maxRetries,
						false,				// save,
						"MMS_SignedURL",	// deliveryType,

						false, // warningIfMissingMediaItemKey,
						true,  // filteredByStatistic
						""	   // userId (it is not needed it
							   // filteredByStatistic is true
					);

					tie(sourcePhysicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
				}

				complexFilterRoot["imagePhysicalPathName"] = sourcePhysicalPathName;
				complexFilterRoot["imagePhysicalDeliveryURL"] = sourcePhysicalDeliveryURL;
				complexFiltersRoot[complexFilterIndex] = complexFilterRoot;
			}
		}
		filtersRoot["complex"] = complexFiltersRoot;
	}

	return filtersRoot;
}

string MMSEngineProcessor::generateMediaMetadataToIngest(
	int64_t ingestionJobKey, string fileFormat, string title, int64_t imageOfVideoMediaItemKey, int64_t cutOfVideoMediaItemKey,
	int64_t cutOfAudioMediaItemKey, double startTimeInSeconds, double endTimeInSeconds, json parametersRoot
)
{
	string field = "fileFormat";
	if (JSONUtils::isMetadataPresent(parametersRoot, field))
	{
		string fileFormatSpecifiedByUser = JSONUtils::asString(parametersRoot, field, "");
		if (fileFormatSpecifiedByUser != fileFormat)
		{
			string errorMessage = string("Wrong fileFormat") + ", _processorIdentifier: " + to_string(_processorIdentifier) +
								  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", fileFormatSpecifiedByUser: " + fileFormatSpecifiedByUser +
								  ", fileFormat: " + fileFormat;
			SPDLOG_ERROR(string() + errorMessage);

			throw runtime_error(errorMessage);
		}
	}
	else
	{
		parametersRoot[field] = fileFormat;
	}

	if (imageOfVideoMediaItemKey != -1)
	{
		json crossReferencesRoot = json::array();
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::ImageOfVideo;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = imageOfVideoMediaItemKey;

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}
	else if (cutOfVideoMediaItemKey != -1)
	{
		json crossReferencesRoot = json::array();
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::CutOfVideo;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = cutOfVideoMediaItemKey;

			json crossReferenceParametersRoot;
			{
				field = "startTimeInSeconds";
				crossReferenceParametersRoot[field] = startTimeInSeconds;

				field = "endTimeInSeconds";
				crossReferenceParametersRoot[field] = endTimeInSeconds;

				field = "parameters";
				crossReferenceRoot[field] = crossReferenceParametersRoot;
			}

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}
	else if (cutOfAudioMediaItemKey != -1)
	{
		json crossReferencesRoot = json::array();
		{
			json crossReferenceRoot;

			MMSEngineDBFacade::CrossReferenceType crossReferenceType = MMSEngineDBFacade::CrossReferenceType::CutOfAudio;

			field = "type";
			crossReferenceRoot[field] = MMSEngineDBFacade::toString(crossReferenceType);

			field = "mediaItemKey";
			crossReferenceRoot[field] = cutOfAudioMediaItemKey;

			json crossReferenceParametersRoot;
			{
				field = "startTimeInSeconds";
				crossReferenceParametersRoot[field] = startTimeInSeconds;

				field = "endTimeInSeconds";
				crossReferenceParametersRoot[field] = endTimeInSeconds;

				field = "parameters";
				crossReferenceRoot[field] = crossReferenceParametersRoot;
			}

			crossReferencesRoot.push_back(crossReferenceRoot);
		}

		field = "crossReferences";
		parametersRoot[field] = crossReferencesRoot;
	}

	field = "title";
	if (title != "")
		parametersRoot[field] = title;

	// this scenario is for example for the Cut or Concat-Demux or
	// Periodical-Frames that generate a new content (or contents in case of
	// Periodical-Frames) and the Parameters json will contain the parameters
	// for the new content.
	// It will contain also parameters for the Cut or Concat-Demux or
	// Periodical-Frames or ..., we will leave there even because we know they
	// will not be used by the Add-Content task

	string mediaMetadata = JSONUtils::toString(parametersRoot);

	SPDLOG_INFO(
		string() + "Media metadata generated" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
		", ingestionJobKey: " + to_string(ingestionJobKey) + ", mediaMetadata: " + mediaMetadata
	);

	return mediaMetadata;
}

void MMSEngineProcessor::handleGEOInfoEventThread()
{

	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "updateGEOInfo", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey,
	);

	bool alreadyExecuted = true;

	try
	{
		SPDLOG_INFO(string() + "GEOInfo: oncePerDayExecution" + ", _processorIdentifier: " + to_string(_processorIdentifier));

		alreadyExecuted = _mmsEngineDBFacade->oncePerDayExecution(MMSEngineDBFacade::OncePerDayType::GEOInfo);
	}
	catch (runtime_error &e)
	{
		SPDLOG_ERROR(string() + "GEOInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what());

		// no throw since it is running in a detached thread
		// throw e;
	}
	catch (exception &e)
	{
		SPDLOG_ERROR(string() + "GEOInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what());

		// no throw since it is running in a detached thread
		// throw e;
	}

	if (!alreadyExecuted)
	{
		chrono::system_clock::time_point start = chrono::system_clock::now();

		try
		{
			_mmsEngineDBFacade->updateGEOInfo();
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "GEOInfo: updateGEOInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
			);

			// no throw since it is running in a detached thread
			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "GEOInfo: updateGEOInfo failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", exception: " + e.what()
			);

			// no throw since it is running in a detached thread
			// throw e;
		}

		chrono::system_clock::time_point end = chrono::system_clock::now();
		SPDLOG_INFO(
			string() + "GEOInfo: updateGEOInfo finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
		);
	}
}

void MMSEngineProcessor::handleCheckRefreshPartitionFreeSizeEventThread()
{
	ThreadsStatistic::ThreadStatistic threadStatistic(
		_mmsThreadsStatistic, "handleCheckRefreshPartitionFreeSizeEventThread", _processorIdentifier, _processorsThreadsNumber.use_count(),
		-1 // ingestionJobKey,
	);

	chrono::system_clock::time_point start = chrono::system_clock::now();

	{
		SPDLOG_INFO(string() + "Check Refresh Partition Free Size started" + ", _processorIdentifier: " + to_string(_processorIdentifier));

		try
		{
			_mmsStorage->refreshPartitionsFreeSizes();
		}
		catch (runtime_error &e)
		{
			SPDLOG_ERROR(
				string() + "refreshPartitionsFreeSizes failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			// no throw since it is running in a detached thread
			// throw e;
		}
		catch (exception &e)
		{
			SPDLOG_ERROR(
				string() + "refreshPartitionsFreeSizes failed" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", exception: " + e.what()
			);

			// no throw since it is running in a detached thread
			// throw e;
		}

		chrono::system_clock::time_point end = chrono::system_clock::now();
		SPDLOG_INFO(
			string() + "Check Refresh Partition Free Size finished" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", @MMS statistics@ - duration (secs): @" + to_string(chrono::duration_cast<chrono::seconds>(end - start).count()) + "@"
		);
	}
}

void MMSEngineProcessor::ftpUploadMediaSource(
	string mmsAssetPathName, string fileName, int64_t sizeInBytes, int64_t ingestionJobKey, shared_ptr<Workspace> workspace, int64_t mediaItemKey,
	int64_t physicalPathKey, string ftpServer, int ftpPort, string ftpUserName, string ftpPassword, string ftpRemoteDirectory,
	string ftpRemoteFileName
)
{

	// curl -T localfile.ext
	// ftp://username:password@ftp.server.com/remotedir/remotefile.zip

	try
	{
		SPDLOG_INFO(
			string() + "ftpUploadMediaSource" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey)
		);

		chrono::system_clock::time_point lastProgressUpdate = chrono::system_clock::now();
		double lastPercentageUpdated = -1.0;
		bool uploadingStoppedByUser = false;
		curlpp::types::ProgressFunctionFunctor functor = bind(
			&MMSEngineProcessor::progressUploadCallback, this, ingestionJobKey, lastProgressUpdate, lastPercentageUpdated, uploadingStoppedByUser,
			std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4
		);
		MMSCURL::ftpFile(
			_logger, ingestionJobKey, mmsAssetPathName, fileName, sizeInBytes, ftpServer, ftpPort, ftpUserName, ftpPassword, ftpRemoteDirectory,
			ftpRemoteFileName, functor
		);

		{
			SPDLOG_INFO(
				string() + "addIngestionJobOutput" + ", _processorIdentifier: " + to_string(_processorIdentifier) + ", ingestionJobKey: " +
				to_string(ingestionJobKey) + ", mediaItemKey: " + to_string(mediaItemKey) + ", physicalPathKey: " + to_string(physicalPathKey)
			);
			int64_t liveRecordingIngestionJobKey = -1;
			_mmsEngineDBFacade->addIngestionJobOutput(ingestionJobKey, mediaItemKey, physicalPathKey, liveRecordingIngestionJobKey);
		}
	}
	catch (exception e)
	{
		string errorMessage = string() + "Download failed (exception)" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
							  ", ingestionJobKey: " + to_string(ingestionJobKey) + ", mmsAssetPathName: " + mmsAssetPathName +
							  ", exception: " + e.what();
		SPDLOG_ERROR(errorMessage);

		throw runtime_error(errorMessage);
	}
}

int MMSEngineProcessor::progressUploadCallback(
	int64_t ingestionJobKey, chrono::system_clock::time_point &lastTimeProgressUpdate, double &lastPercentageUpdated, bool &uploadingStoppedByUser,
	double dltotal, double dlnow, double ultotal, double ulnow
)
{

	chrono::system_clock::time_point now = chrono::system_clock::now();

	if (ultotal != 0 && (ultotal == ulnow || now - lastTimeProgressUpdate >= chrono::seconds(_progressUpdatePeriodInSeconds)))
	{
		double progress = (ulnow / ultotal) * 100;
		// int uploadingPercentage = floorf(progress * 100) / 100;
		// this is to have one decimal in the percentage
		double uploadingPercentage = ((double)((int)(progress * 10))) / 10;

		SPDLOG_INFO(
			string() + "Upload still running" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
			", ingestionJobKey: " + to_string(ingestionJobKey) + ", uploadingPercentage: " + to_string(uploadingPercentage) +
			", dltotal: " + to_string(dltotal) + ", dlnow: " + to_string(dlnow) + ", ultotal: " + to_string(ultotal) + ", ulnow: " + to_string(ulnow)
		);

		lastTimeProgressUpdate = now;

		if (lastPercentageUpdated != uploadingPercentage)
		{
			SPDLOG_INFO(
				string() + "Update IngestionJob" + ", _processorIdentifier: " + to_string(_processorIdentifier) +
				", ingestionJobKey: " + to_string(ingestionJobKey) + ", uploadingPercentage: " + to_string(uploadingPercentage)
			);
			uploadingStoppedByUser = _mmsEngineDBFacade->updateIngestionJobSourceUploadingInProgress(ingestionJobKey, uploadingPercentage);

			lastPercentageUpdated = uploadingPercentage;
		}

		if (uploadingStoppedByUser)
			return 1; // stop downloading
	}

	return 0;
}

tuple<int64_t, int64_t, MMSEngineDBFacade::ContentType, string, string, string, string, int64_t, string, string, bool>
MMSEngineProcessor::processDependencyInfo(
	shared_ptr<Workspace> workspace, int64_t ingestionJobKey,
	tuple<int64_t, MMSEngineDBFacade::ContentType, Validator::DependencyType, bool> keyAndDependencyType
)
{
	int64_t mediaItemKey;
	int64_t physicalPathKey;
	string relativePath;
	string fileName;
	int64_t durationInMilliSecs;
	MMSEngineDBFacade::ContentType contentType;
	bool stopIfReferenceProcessingError;
	{
		int64_t key;
		Validator::DependencyType dependencyType;

		tie(key, contentType, dependencyType, stopIfReferenceProcessingError) = keyAndDependencyType;

		if (dependencyType == Validator::DependencyType::MediaItemKey)
		{
			mediaItemKey = key;

			tuple<int64_t, int, string, string, int64_t, bool, int64_t> physicalPathDetails = _mmsEngineDBFacade->getSourcePhysicalPath(
				mediaItemKey,
				// 2022-12-18: MIK potrebbe essere stato appena aggiunto
				true
			);
			tie(physicalPathKey, ignore, relativePath, fileName, ignore, ignore, durationInMilliSecs) = physicalPathDetails;
		}
		else
		{
			physicalPathKey = key;

			bool warningIfMissing = false;
			tuple<int64_t, MMSEngineDBFacade::ContentType, string, string, string, int64_t, string, string, int64_t> mediaItemDetails =
				_mmsEngineDBFacade->getMediaItemKeyDetailsByPhysicalPathKey(
					workspace->_workspaceKey, physicalPathKey, warningIfMissing,
					// 2022-12-18: MIK potrebbe essere stato appena aggiunto
					true
				);
			tie(mediaItemKey, ignore, ignore, ignore, ignore, ignore, fileName, relativePath, durationInMilliSecs) = mediaItemDetails;
		}
	}

	string assetPathName;
	{
		tuple<string, int, string, string, int64_t, string> physicalPathDetails = _mmsStorage->getPhysicalPathDetails(
			physicalPathKey,
			// 2022-12-18: MIK potrebbe essere stato appena aggiunto
			true
		);
		tie(assetPathName, ignore, ignore, ignore, ignore, ignore) = physicalPathDetails;
	}

	string fileExtension;
	{
		size_t extensionIndex = fileName.find_last_of(".");
		if (extensionIndex == string::npos)
		{
			string errorMessage = string() + "No extension find in the asset file name" + ", fileName: " + fileName;
			SPDLOG_ERROR(errorMessage);

			throw runtime_error(errorMessage);
		}
		fileExtension = fileName.substr(extensionIndex);
	}

	// calculate delivery URL in case of an external encoder
	string physicalDeliveryURL;
	{
		int64_t utcNow;
		{
			chrono::system_clock::time_point now = chrono::system_clock::now();
			utcNow = chrono::system_clock::to_time_t(now);
		}

		pair<string, string> deliveryAuthorizationDetails = _mmsDeliveryAuthorization->createDeliveryAuthorization(
			-1, // userKey,
			workspace,
			"", // clientIPAddress,

			-1, // mediaItemKey,
			"", // uniqueName,
			-1, // encodingProfileKey,
			"", // encodingProfileLabel,

			physicalPathKey,

			-1, // ingestionJobKey,	(in case of live)
			-1, // deliveryCode,

			365 * 24 * 60 * 60, // ttlInSeconds, 365 days!!!
			999999,				// maxRetries,
			false,				// save,
			"MMS_SignedURL",	// deliveryType,

			false, // warningIfMissingMediaItemKey,
			true,  // filteredByStatistic
			""	   // userId (it is not needed it filteredByStatistic is true
		);

		tie(physicalDeliveryURL, ignore) = deliveryAuthorizationDetails;
	}

	string transcoderStagingAssetPathName; // used in case of external encoder
	{
		bool removeLinuxPathIfExist = false;
		bool neededForTranscoder = true;
		transcoderStagingAssetPathName = _mmsStorage->getStagingAssetPathName(
			neededForTranscoder,
			workspace->_directoryName,	// workspaceDirectoryName
			to_string(ingestionJobKey), // directoryNamePrefix
			"/",						// relativePath,
			fileName,
			-1, // _encodingItem->_mediaItemKey, not used because
				// encodedFileName is not ""
			-1, // _encodingItem->_physicalPathKey, not used because
				// encodedFileName is not ""
			removeLinuxPathIfExist
		);
	}

	return make_tuple(
		mediaItemKey, physicalPathKey, contentType, assetPathName, relativePath, fileName, fileExtension, durationInMilliSecs, physicalDeliveryURL,
		transcoderStagingAssetPathName, stopIfReferenceProcessingError
	);
}

string MMSEngineProcessor::getEncodedFileExtensionByEncodingProfile(json encodingProfileDetailsRoot)
{
	string extension;

	string fileFormat = JSONUtils::asString(encodingProfileDetailsRoot, "fileFormat", "");
	string fileFormatLowerCase;
	fileFormatLowerCase.resize(fileFormat.size());
	transform(fileFormat.begin(), fileFormat.end(), fileFormatLowerCase.begin(), [](unsigned char c) { return tolower(c); });

	if (fileFormatLowerCase == "hls" || fileFormatLowerCase == "dash")
	{
		// it will be a directory, no extension
		;
	}
	else
	{
		extension = "." + fileFormatLowerCase;
	}

	return extension;
}
